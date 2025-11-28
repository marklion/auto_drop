#include "pc_opt.h"
#include <pcl/common/transforms.h>
#include <pcl/common/impl/angles.hpp>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/common/pca.h>
#include <boost/filesystem.hpp>
#include <rs_driver/api/lidar_driver.hpp>
#include <rs_driver/msg/pcl_point_cloud_msg.hpp>
#include <mutex>
#include <thread>
#include "../../public/utils/ad_utils.h"
#include "../../rpc/ad_rpc.h"
#include <sys/file.h> // 引入 flock
#include <unistd.h>   // 引入 close
#include <rs_driver/utility/sync_queue.hpp>

static AD_INI_CONFIG *g_ini_config = nullptr;
static vehicle_rd_detect_result g_rd_result;
static std::recursive_mutex g_rd_result_mutex;

std::vector<float> g_full_offset_array;

typedef pcl::PointXYZRGB myPoint;
typedef pcl::PointCloud<myPoint> myPointCloud;
typedef PointCloudT<pcl::PointXYZI> pcMsg;

robosense::lidar::SyncQueue<std::shared_ptr<pcMsg>> free_cloud_queue;
robosense::lidar::SyncQueue<std::shared_ptr<pcMsg>> stuffed_cloud_queue;

static void rd_print_log_2stdout(const std::string &_format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, _format);
    vsnprintf(buffer, sizeof(buffer), _format.c_str(), args);
    va_end(args);
    puts(buffer);
}

static void rd_print_log(const std::string &_format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, _format);
    vsnprintf(buffer, sizeof(buffer), _format.c_str(), args);
    va_end(args);

    AD_LOGGER tmp_logger("RIDAR");
    tmp_logger.log("%s", buffer);
}

struct pc_after_pickup
{
    myPointCloud::Ptr last;
    myPointCloud::Ptr picked;
    pc_after_pickup() : last(new myPointCloud), picked(new myPointCloud)
    {
    }
};
struct pc_after_split
{
    myPointCloud::Ptr legal_side;
    myPointCloud::Ptr illegal_side;
    myPointCloud::Ptr content;
    pc_after_split() : legal_side(new myPointCloud), illegal_side(new myPointCloud), content(new myPointCloud)
    {
    }
};
static AD_INI_CONFIG *get_ini_config();
static void save_detect_result(const vehicle_rd_detect_result &result);
static vehicle_rd_detect_result get_detect_result();
static void process_msg(std::shared_ptr<pcMsg> msg);
static void serial_pc();
static myPointCloud::Ptr g_cur_cloud;
static myPointCloud::Ptr g_output_cloud;
static std::shared_ptr<pcMsg> g_full_cloud;
static void put_cloud(myPointCloud::Ptr _cloud)
{
    if (g_cur_cloud)
    {
        *g_cur_cloud += *_cloud;
    }
}
static myPointCloud::Ptr get_cur_cloud()
{
    myPointCloud::Ptr ret(new myPointCloud);
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    pcl::copyPointCloud(*g_output_cloud, *ret);

    return ret;
}
static std::shared_ptr<pcMsg> get_full_cloud()
{
    auto ret = std::make_shared<pcMsg>();
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    if (g_full_cloud)
    {
        pcl::copyPointCloud(*g_full_cloud, *ret);
    }

    return ret;
}
static void save_full_cloud(std::shared_ptr<pcMsg> _cloud)
{
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    g_full_cloud = _cloud;
}
static void update_cur_cloud()
{
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    g_output_cloud = g_cur_cloud;
    g_cur_cloud.reset(new myPointCloud);
}
static void color_cloud(int r, int g, int b, myPointCloud::Ptr _cloud)
{
    for (auto &itr : _cloud->points)
    {
        itr.r = r;
        itr.g = g;
        itr.b = b;
    }
}
long long get_current_us_stamp()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long current_useconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    return current_useconds;
}
void processCloud(void)
{
    while (true)
    {
        std::shared_ptr<pcMsg> msg = stuffed_cloud_queue.popWait();
        if (msg.get() == NULL)
        {
            continue;
        }
        auto begin_us_stamp = get_current_us_stamp();
        rd_print_log("====start to process one cloud====");
        process_msg(msg);
        auto end_us_stamp = get_current_us_stamp();
        rd_print_log("====process one cloud takes %ld us====", end_us_stamp - begin_us_stamp);
        free_cloud_queue.push(msg);
    }
}
std::shared_ptr<pcMsg> driverGetPointCloudFromCallerCallback(void)
{
    std::shared_ptr<pcMsg> msg = free_cloud_queue.pop();
    if (msg.get() != NULL)
    {
        return msg;
    }

    return std::make_shared<pcMsg>();
}
void driverReturnPointCloudToCallerCallback(std::shared_ptr<pcMsg> msg)
{
    if (stuffed_cloud_queue.size() > 1)
    {
        rd_print_log("stuffed_cloud_queue size is %zu", stuffed_cloud_queue.size());
    }
    stuffed_cloud_queue.push(msg);
}
std::string make_file_name(const std::string &_filename)
{
    static int file_no = 0;
    auto file_name = _filename;
    if (_filename.empty())
    {
        file_name = "/tmp/ply/file_" + std::to_string(file_no++) + ".ply";
    }
    boost::filesystem::path dir = boost::filesystem::path(file_name).parent_path();
    if (!boost::filesystem::exists(dir))
    {
        boost::filesystem::create_directories(dir);
    }
    return file_name;
}
static void save_ply(std::shared_ptr<pcMsg> cloud, const std::string &_filename = "")
{
    auto file_name = make_file_name(_filename);
    pcl::io::savePLYFileASCII(file_name, *cloud);
}
static void save_ply(myPointCloud::Ptr cloud, const std::string &_filename = "")
{
    auto file_name = make_file_name(_filename);
    pcl::io::savePLYFileASCII(file_name, *cloud);
}
static std::shared_ptr<pcMsg> create_new_msg()
{
    return std::make_shared<pcMsg>();
    // static int inter = 0;
    // inter++;
    // if (inter == 15)
    // {
    //     inter = 0;
    // }
    // else
    // {
    //     return nullptr;
    // }
}
static void split_cloud_by_pt(myPointCloud::Ptr _cloud, const std::string &_field, float _min, float _max, myPointCloud::Ptr &_cloud_filtered, myPointCloud::Ptr &_cloud_last)
{
    pcl::PassThrough<myPoint> pass;
    myPointCloud::Ptr tmp(new myPointCloud);
    pcl::copyPointCloud(*_cloud, *tmp);
    pass.setFilterFieldName(_field);
    pass.setFilterLimits(_min, _max);
    pass.setInputCloud(tmp);
    pass.filter(*_cloud_filtered);
    pass.setFilterLimitsNegative(true);
    pass.filter(*_cloud_last);
}
// 聚类函数
static std::vector<myPointCloud::Ptr> cluster_plane_points(
    myPointCloud::Ptr plane_points,
    float cluster_tolerance = 0.02f, // 聚类距离容差
    int min_cluster_size = 50,       // 最小聚类大小
    int max_cluster_size = 25000     // 最大聚类大小
)
{
    std::vector<myPointCloud::Ptr> clusters;

    if (plane_points->empty())
    {
        return clusters;
    }

    // 创建KdTree用于搜索
    pcl::search::KdTree<myPoint>::Ptr tree(new pcl::search::KdTree<myPoint>);
    tree->setInputCloud(plane_points);

    // 欧几里得聚类
    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<myPoint> ec;
    ec.setClusterTolerance(cluster_tolerance);
    ec.setMinClusterSize(min_cluster_size);
    ec.setMaxClusterSize(max_cluster_size);
    ec.setSearchMethod(tree);
    ec.setInputCloud(plane_points);
    ec.extract(cluster_indices);

    // 提取每个聚类
    for (const auto &indices : cluster_indices)
    {
        myPointCloud::Ptr cluster(new myPointCloud);
        pcl::copyPointCloud(*plane_points, indices, *cluster);
        clusters.push_back(cluster);
    }

    return clusters;
}

static pc_after_pickup find_max_cluster(myPointCloud::Ptr _cloud, float _cluster_distance_threshold, int _cluster_require_points)
{
    pc_after_pickup ret;
    auto picked_up = _cloud;
    auto after_cluster = cluster_plane_points(picked_up, _cluster_distance_threshold, _cluster_require_points, 999999999);
    if (!after_cluster.empty())
    {
        picked_up = after_cluster[0];
        for (size_t i = 1; i < after_cluster.size(); ++i)
        {
            if (after_cluster[i]->points.size() > picked_up->points.size())
            {
                picked_up = after_cluster[i];
                *ret.last += *picked_up;
            }
            else
            {
                *ret.last += *(after_cluster[i]);
            }
        }
    }
    ret.picked = picked_up;

    return ret;
}

static pc_after_pickup find_points_on_plane(myPointCloud::Ptr _cloud, const Eigen::Vector3f &_ax_vec, float _distance_threshold, float _angle_threshold, pcl::ModelCoefficients::Ptr &_coe, float _cluster_distance_threshold, int _cluster_require_points)
{
    auto picked_up = myPointCloud::Ptr(new myPointCloud);
    pcl::PointIndices::Ptr one_plane(new pcl::PointIndices);
    pcl::SACSegmentation<myPoint> seg;
    seg.setModelType(pcl::SACMODEL_PERPENDICULAR_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(_distance_threshold);
    seg.setOptimizeCoefficients(true);
    seg.setMaxIterations(1000);
    seg.setEpsAngle(pcl::deg2rad(_angle_threshold));
    seg.setAxis(_ax_vec);
    seg.setInputCloud(_cloud);
    seg.segment(*one_plane, *_coe);

    pcl::ExtractIndices<myPoint> extract;
    extract.setInputCloud(_cloud);
    extract.setIndices(one_plane);
    extract.setNegative(false);
    extract.filter(*picked_up);
    auto last = myPointCloud::Ptr(new myPointCloud);
    extract.setNegative(true);
    extract.filter(*last);

    pc_after_pickup ret;
    ret.picked = picked_up;
    ret.last = last;
    return ret;
}
static float pointToLineDistance(const Eigen::Vector3f &point, const Eigen::Vector3f &line_point, const Eigen::Vector3f &line_dir)
{
    Eigen::Vector3f diff = point - line_point;
    Eigen::Vector3f cross_product = diff.cross(line_dir);
    return cross_product.norm() / line_dir.norm();
}

static std::unique_ptr<myPoint> find_max_or_min_z_point(myPointCloud::Ptr _cloud, float _line_distance_threshold, bool _is_min = false)
{
    auto ret = std::make_unique<myPoint>();
    ret->z = std::numeric_limits<float>::lowest();
    if (_is_min)
    {
        ret->z = std::numeric_limits<float>::max();
    }

    float x_min = std::numeric_limits<float>::max();
    float x_max = std::numeric_limits<float>::lowest();
    float y_bar = 0;
    // 遍历点云找到 最小x,最大x, 平均y
    for (const auto &point : _cloud->points)
    {
        if (point.x < x_min)
        {
            x_min = point.x;
        }
        if (point.x > x_max)
        {
            x_max = point.x;
        }
        y_bar += point.y;
    }
    ret->y = y_bar / _cloud->points.size();

    float all_z[30];
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        all_z[i] = std::numeric_limits<float>::lowest();
        if (_is_min)
        {
            all_z[i] = std::numeric_limits<float>::max();
        }
    }

    // 在x轴方向分30份，找出每一份紧凑点集中的最大z值
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        for (const auto &point : _cloud->points)
        {
            if (std::abs(point.x - (x_min + (x_max - x_min) * i / (sizeof(all_z) / sizeof(float)))) < _line_distance_threshold)
            {
                if (!_is_min && point.z > all_z[i])
                {
                    all_z[i] = point.z;
                }
                else if (_is_min && point.z < all_z[i])
                {
                    all_z[i] = point.z;
                }
            }
        }
    }

    // 计算得到一个平均最大z值的点
    float total_z = 0;
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        total_z += all_z[i];
    }
    ret->z = total_z / (sizeof(all_z) / sizeof(float));
    return ret;
}

static std::pair<myPoint, myPoint> get_key_seg(myPointCloud::Ptr _cloud, float line_distance_threshold, float _full_y_offset, float _full_z_offset)
{
    auto max_z_point = find_max_or_min_z_point(_cloud, line_distance_threshold, false);

    max_z_point->x = 0;
    max_z_point->z += _full_z_offset;

    // 找出距离过上点平行于y轴的直线比较紧凑的若干点
    std::vector<myPoint> line_points;
    Eigen::Vector3f key_line_dir(1, 0, 0);
    for (auto &itr : _cloud->points)
    {
        auto tmp_point = itr;
        tmp_point.y = max_z_point->y;
        if (pointToLineDistance(tmp_point.getVector3fMap(), max_z_point->getVector3fMap(), key_line_dir) < line_distance_threshold)
        {
            line_points.push_back(itr);
        }
    }
    std::sort(
        line_points.begin(), line_points.end(),
        [](const myPoint &a, const myPoint &b)
        {
            return a.x < b.x;
        });
    if (line_points.empty())
    {
        return std::make_pair(*max_z_point, *max_z_point);
    }
    auto begin = line_points.front();
    auto end = line_points.front();
    float tmp_x = begin.x;

    // 找出刚才找到的点集中距离较大的两个点作为直线的两点并返回
    for (auto &itr : line_points)
    {
        if (itr.x - tmp_x < line_distance_threshold)
        {
            tmp_x = itr.x;
            end = itr;
        }
        else
        {
            break;
        }
    }
    begin.y = max_z_point->y + _full_y_offset;
    end.y = max_z_point->y + _full_y_offset;
    begin.z = max_z_point->z;
    end.z = max_z_point->z;

    return std::make_pair(begin, end);
}
static void insert_several_points(myPointCloud::Ptr _cloud, const myPoint &p1, const myPoint &p2, bool _is_red = false)
{
    _cloud->points.push_back(p1);
    for (int i = 1; i <= 100; ++i)
    {
        float t = static_cast<float>(i) / (100 + 1);
        myPoint p;
        p.x = p1.x + t * (p2.x - p1.x);
        p.y = p1.y + t * (p2.y - p1.y);
        p.z = p1.z + t * (p2.z - p1.z);
        p.r = 127;
        p.b = 255;
        p.g = 100;
        if (_is_red)
        {
            p.r = 255;
            p.b = 0;
            p.g = 0;
        }
        _cloud->points.push_back(p);
    }
    _cloud->points.push_back(p2);
}
static int g_update_counter = 0; // 添加一个全局计数器
static void serializePointCloud(myPointCloud::Ptr cloud)
{
    int fd = open("/tmp/cloud.lock", O_CREAT | O_RDWR, 0666); // 打开或创建锁文件
    if (fd == -1)
    {
        perror("Failed to open lock file");
        return;
    }

    if (flock(fd, LOCK_EX) == -1) // 加独占锁
    {
        perror("Failed to acquire file lock");
        close(fd);
        return;
    }

    // 写入点云数据
    std::ofstream ofs("/tmp/cloud.bin", std::ios::binary);
    if (!ofs.is_open())
    {
        perror("Failed to open /tmp/cloud.bin");
        flock(fd, LOCK_UN); // 释放锁
        close(fd);
        return;
    }

    for (const auto &point : *cloud)
    {
        float data[7] = {
            point.x, point.y, point.z, point.r * 1.0f, point.g * 1.0f, point.b * 1.0f, 0.0f};
        ofs.write(reinterpret_cast<char *>(data), sizeof(data));
    }

    ofs.close();

    flock(fd, LOCK_UN); // 释放锁
    close(fd);          // 关闭文件描述符
}

std::unique_ptr<pc_after_pickup> pickup_pc_from_spec_range(myPointCloud::Ptr _orig_pc)
{
    const std::string config_sec = "get_state";

    auto x_min = atof(get_ini_config()->get_config(config_sec, "x_min", "0").c_str());
    auto x_max = atof(get_ini_config()->get_config(config_sec, "x_max", "0").c_str());
    auto y_min = atof(get_ini_config()->get_config(config_sec, "y_min", "0").c_str());
    auto y_max = atof(get_ini_config()->get_config(config_sec, "y_max", "0").c_str());
    auto result = std::make_unique<pc_after_pickup>();
    myPointCloud::Ptr tmp(new myPointCloud);

    split_cloud_by_pt(_orig_pc, "x", x_min, x_max, result->picked, tmp);
    *result->last += *tmp;
    split_cloud_by_pt(result->picked, "y", y_min, y_max, result->picked, tmp);
    *result->last += *tmp;

    return result;
}

void serial_pc()
{
    g_update_counter++;
    if (g_update_counter >= 5)
    {
        serializePointCloud(get_cur_cloud());
        g_update_counter = 0; // 重置计数器
    }
}

struct grid_volume_detial
{
    float full_offset = 0;
    myPointCloud::Ptr cloud;
    myPointCloud::Ptr last_cloud;
};

static grid_volume_detial calc_grid_volume(myPointCloud::Ptr _cloud, float _x_min, float _x_max, float _y_min, float _y_max, float _top_z)
{
    grid_volume_detial ret;

    myPointCloud::Ptr x_last(new myPointCloud);
    myPointCloud::Ptr y_last(new myPointCloud);
    myPointCloud::Ptr cloud_filtered_x(new myPointCloud);
    myPointCloud::Ptr cloud_filtered_y(new myPointCloud);
    split_cloud_by_pt(_cloud, "x", _x_min, _x_max, cloud_filtered_x, x_last);
    split_cloud_by_pt(cloud_filtered_x, "y", _y_min, _y_max, cloud_filtered_y, y_last);

    *y_last += *x_last;
    bool is_vertical = false;
    try
    {
        pcl::PCA<myPoint> pca;
        pca.setInputCloud(cloud_filtered_y);
        auto eig_vec = pca.getEigenVectors();
        auto z_axis = eig_vec.col(2);
        Eigen::Vector3f target_z_axis(0, 0, 1);
        float dot_product = z_axis.dot(target_z_axis);
        is_vertical = (std::fabs(dot_product) < 0.25);
    }
    catch (...)
    {
    }

    ret.last_cloud = y_last;
    float top_z = 0;
    if (is_vertical)
    {
        *ret.last_cloud += *cloud_filtered_y;
    }
    else
    {
        ret.cloud = cloud_filtered_y;
        if (cloud_filtered_y->points.size() > 0)
        {
            for (auto &itr : cloud_filtered_y->points)
            {
                top_z += itr.z;
            }
            top_z = top_z / cloud_filtered_y->points.size();
        }
    }
    if (top_z != 0)
    {
    ret.full_offset = top_z - _top_z;
    }
    else
    {
        ret.full_offset = -0.5;
    }

    return ret;
}

struct vehicle_detail_info
{
    bool is_full = false;
    float full_offset = 0;
    myPointCloud::Ptr clouds;
    myPointCloud::Ptr last_clouds;
    vehicle_detail_info() : clouds(new myPointCloud), last_clouds(new myPointCloud)
    {
    }
};

bool calc_filtered_full(float _full_offset, float _full_rate, long filter_length)
{
    bool ret = true;
    auto cur_full_offset = 0.0f;
    g_full_offset_array.push_back(_full_offset);
    if (g_full_offset_array.size() > filter_length)
    {
        g_full_offset_array.erase(g_full_offset_array.begin());

    }
    for (const auto &itr : g_full_offset_array)
    {
        cur_full_offset += itr;
    }
    cur_full_offset = cur_full_offset / g_full_offset_array.size();
    if (cur_full_offset > _full_rate)
    {
        ret = false;
    }
    return ret;
}

static vehicle_detail_info vehicle_is_full(myPointCloud::Ptr _cloud, float _line_DistanceThreshold, float _max_z, float _x_min, float _x_max, float _y, myPointCloud::Ptr _full_cloud, long filter_length)
{
    vehicle_detail_info ret;
    auto begin_us_timestamp = get_current_us_stamp();
    const std::string config_sec = "get_state";

    auto full_rate = atof(get_ini_config()->get_config(config_sec, "full_rate", "0.8").c_str());
    auto spec_width = atof(get_ini_config()->get_config(config_sec, "spec_width", "2.45").c_str());
    auto x_grid_number = atoi(get_ini_config()->get_config(config_sec, "x_grid_number", "10").c_str());
    auto y_grid_number = atoi(get_ini_config()->get_config(config_sec, "y_grid_number", "7").c_str());
    ret.is_full = true;

    auto min_z_point = find_max_or_min_z_point(_cloud, _line_DistanceThreshold, true);
    myPoint left_point, right_point;
    left_point.x = _x_min;
    left_point.y = min_z_point->y;
    left_point.z = min_z_point->z;
    right_point.x = _x_max;
    right_point.y = min_z_point->y;
    right_point.z = min_z_point->z;
    insert_several_points(_cloud, left_point, right_point);
    auto min_z = min_z_point->z;
    auto max_height = _max_z - min_z;
    auto cur_length = _x_max - _x_min;

    std::vector<float> full_offset_array;
    auto grid_x = cur_length / x_grid_number;
    auto grid_y = spec_width / y_grid_number;
    ret.last_clouds = _full_cloud;
    for (auto i = 0; i < x_grid_number; i++)
    {
        for (auto j = 0; j < y_grid_number; j++)
        {
            auto grid_volume_detail = calc_grid_volume(ret.last_clouds, _x_min + i * grid_x, _x_min + (i + 1) * grid_x, _y + j * grid_y, _y + (j + 1) * grid_y, _max_z);
            if (grid_volume_detail.cloud)
            {
                *ret.clouds += *(grid_volume_detail.cloud);
            }
            if (grid_volume_detail.last_cloud)
            {
                ret.last_clouds = grid_volume_detail.last_cloud;
            }
            full_offset_array.push_back(grid_volume_detail.full_offset);
        }
    }
    float full_offset = 0;
    for (const auto &itr : full_offset_array)
    {
        full_offset += itr;
    }
    full_offset = full_offset / full_offset_array.size();
    ret.full_offset = full_offset;
    auto end_us_timestamp = get_current_us_stamp();
    rd_print_log("current volume rate:%f, spend:%dus", full_offset, end_us_timestamp - begin_us_timestamp);
    ret.is_full = calc_filtered_full(full_offset, full_rate, filter_length);

    return ret;
}

static myPointCloud::Ptr draw_line_x(float _x, float _z)
{
    myPointCloud::Ptr ret(new myPointCloud);
    myPoint first, second;
    first.x = _x;
    first.y = -2;
    first.z = _z;
    second.x = _x;
    second.y = 2;
    second.z = _z;
    insert_several_points(ret, first, second, true);

    return ret;
}

static pc_after_split split_cloud_to_side_and_content(myPointCloud::Ptr _cloud)
{
    const std::string config_sec = "get_state";

    auto plane_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "plane_DistanceThreshold", "0.1").c_str());
    auto cluster_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "cluster_DistanceThreshold", "0.1").c_str());
    auto AngleThreshold = atof(get_ini_config()->get_config(config_sec, "AngleThreshold", "0.1").c_str());
    auto cluster_require_points = atoi(get_ini_config()->get_config(config_sec, "cluster_require_points", "200").c_str());

    auto start_us_stamp = get_current_us_stamp();
    pc_after_split ret;
    auto pickup_resp = pickup_pc_from_spec_range(_cloud);

    // 在范围内点云中拟合垂直y轴的平面,范围内点云染蓝色，平面染黄色
    pcl::ModelCoefficients::Ptr coe_side(new pcl::ModelCoefficients);
    auto fpop_ret = find_points_on_plane(pickup_resp->picked, Eigen::Vector3f(0, 1, 0), plane_DistanceThreshold, AngleThreshold, coe_side, cluster_DistanceThreshold, cluster_require_points);
    auto clst_ret = find_max_cluster(fpop_ret.picked, cluster_DistanceThreshold, cluster_require_points);
    ret.content = pickup_resp->last;
    *ret.content += *fpop_ret.last;
    color_cloud(90, 23, 255, ret.content);
    ret.legal_side = clst_ret.picked;
    color_cloud(255, 255, 0, ret.legal_side);
    ret.illegal_side = clst_ret.last;

    auto end_us_stamp = get_current_us_stamp();
    rd_print_log("split cloud takes %ld us", end_us_stamp - start_us_stamp);
    return ret;
}

struct key_seg_params
{
    float x_min = 0;
    float x_max = 0;
    float y = 0;
    float z = 0;
    vehicle_position_detect_state::type state = vehicle_position_detect_state::vehicle_postion_out;
};

static key_seg_params get_position_state_from_cloud(myPointCloud::Ptr _cloud)
{
    key_seg_params ret;
    const std::string config_sec = "get_state";

    auto line_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "line_DistanceThreshold", "0.1").c_str());
    auto E0 = atof(get_ini_config()->get_config(config_sec, "E0", "0.1").c_str());
    auto E1 = atof(get_ini_config()->get_config(config_sec, "E1", "0.1").c_str());
    auto B_0 = atof(get_ini_config()->get_config(config_sec, "B0", "0.1").c_str());
    auto B1 = atof(get_ini_config()->get_config(config_sec, "B1", "0.1").c_str());
    auto full_z_offset = atof(get_ini_config()->get_config(config_sec, "full_z_offset", "0.1").c_str());

    auto key_seg = get_key_seg(_cloud, line_DistanceThreshold, 0, full_z_offset);
    insert_several_points(_cloud, key_seg.first, key_seg.second);

    if (key_seg.first.x != key_seg.second.x)
    {
        auto ex = key_seg.first.x;
        auto bx = key_seg.second.x;
        rd_print_log("ex:%f, bx:%f", ex, bx);
        if (ex <= E0 && bx >= B_0 && bx <= B1)
        {
            ret.state = vehicle_position_detect_state::vehicle_postion_begin;
        }
        else if (ex <= E0 && bx >= B1)
        {
            ret.state = vehicle_position_detect_state::vehicle_postion_middle;
        }
        else if (ex >= E0 && ex <= E1 && bx >= B1)
        {
            ret.state = vehicle_position_detect_state::vehicle_postion_end;
        }
        else
        {
            ret.state = vehicle_position_detect_state::vehicle_postion_out;
        }
        ret.x_min = key_seg.first.x;
        ret.x_max = key_seg.second.x;
        ret.y = key_seg.first.y;
        ret.z = key_seg.first.z;
    }
    auto line_z = key_seg.first.z;
    *_cloud += *draw_line_x(E0, line_z);
    *_cloud += *draw_line_x(E1, line_z);
    *_cloud += *draw_line_x(B_0, line_z);
    *_cloud += *draw_line_x(B1, line_z);

    return ret;
}

static vehicle_rd_detect_result pc_get_state(myPointCloud::Ptr _cloud)
{
    vehicle_rd_detect_result ret;
    ret.is_full = true;
    ret.state = vehicle_position_detect_state::vehicle_postion_out;

    const std::string config_sec = "get_state";
    auto line_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "line_DistanceThreshold", "0.1").c_str());
    auto filter_length = atoi(get_ini_config()->get_config(config_sec, "filter_length", "1").c_str());

    auto pc_after_split_ret = split_cloud_to_side_and_content(_cloud);
    auto key_seg_ret = get_position_state_from_cloud(pc_after_split_ret.legal_side);
    ret.state = key_seg_ret.state;
    if (ret.state != vehicle_position_detect_state::vehicle_postion_out)
    {
        auto vehicle_detail = vehicle_is_full(pc_after_split_ret.legal_side, line_DistanceThreshold, key_seg_ret.z, key_seg_ret.x_min, key_seg_ret.x_max, key_seg_ret.y, pc_after_split_ret.content, filter_length);
        ret.is_full = vehicle_detail.is_full;
        ret.full_offset = vehicle_detail.full_offset;
        color_cloud(0, 255, 0, pc_after_split_ret.legal_side);

        color_cloud(255, 0, 0, vehicle_detail.clouds);
        put_cloud(vehicle_detail.clouds);
        put_cloud(vehicle_detail.last_clouds);
    }
    else
    {
        put_cloud(pc_after_split_ret.content);
    }
    put_cloud(pc_after_split_ret.legal_side);
    put_cloud(pc_after_split_ret.illegal_side);
    update_cur_cloud();

    return ret;
}
static myPointCloud::Ptr pc_range_filter(myPointCloud::Ptr cloud)
{
    const std::string config_sec = "range_filter";

    auto x_min = atof(get_ini_config()->get_config(config_sec, "x_min", "0").c_str());
    auto x_max = atof(get_ini_config()->get_config(config_sec, "x_max", "100").c_str());
    auto y_min = atof(get_ini_config()->get_config(config_sec, "y_min", "0").c_str());
    auto y_max = atof(get_ini_config()->get_config(config_sec, "y_max", "100").c_str());
    auto z_min = atof(get_ini_config()->get_config(config_sec, "z_min", "0").c_str());
    auto z_max = atof(get_ini_config()->get_config(config_sec, "z_max", "100").c_str());
    auto i_min = atof(get_ini_config()->get_config(config_sec, "i_min", "0").c_str());
    auto i_max = atof(get_ini_config()->get_config(config_sec, "i_max", "100").c_str());
    myPointCloud::Ptr tmp = cloud;
    pcl::PassThrough<myPoint> pass_x;
    pass_x.setInputCloud(tmp);
    pass_x.setFilterFieldName("x");
    pass_x.setFilterLimits(x_min, x_max);
    myPointCloud::Ptr cloud_filtered_x(new myPointCloud);
    pass_x.filter(*cloud_filtered_x);

    pcl::PassThrough<myPoint> pass_y;
    pass_y.setInputCloud(cloud_filtered_x);
    pass_y.setFilterFieldName("y");
    pass_y.setFilterLimits(y_min, y_max);
    myPointCloud::Ptr cloud_filtered_y(new myPointCloud);
    pass_y.filter(*cloud_filtered_y);

    pcl::PassThrough<myPoint> pass_z;
    pass_z.setInputCloud(cloud_filtered_y);
    pass_z.setFilterFieldName("z");
    pass_z.setFilterLimits(z_min, z_max);
    myPointCloud::Ptr cloud_filtered_z(new myPointCloud);
    pass_z.filter(*cloud_filtered_z);

    return cloud_filtered_z;
}
static myPointCloud::Ptr pc_vox_filter(myPointCloud::Ptr cloud)
{
    const std::string config_sec = "voxel_filter";

    auto leaf_size = atof(get_ini_config()->get_config(config_sec, "leaf_size", "0.02").c_str());
    pcl::VoxelGrid<myPoint> voxel_filter;
    voxel_filter.setInputCloud(cloud);
    voxel_filter.setLeafSize(leaf_size, leaf_size, leaf_size);
    myPointCloud::Ptr cloud_filtered(new myPointCloud);
    voxel_filter.filter(*cloud_filtered);
    return cloud_filtered;
}

static myPointCloud::Ptr pc_transform(myPointCloud::Ptr cloud)
{
    const std::string config_sec = "transform";

    Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 4; j++)
        {
            transform(i, j) = atof(get_ini_config()->get_config(config_sec, std::to_string(i) + "-" + std::to_string(j), "0").c_str());
        }
    }
    myPointCloud::Ptr ret(new myPointCloud);
    myPointCloud::Ptr tmp = cloud;

    pcl::transformPointCloud(*tmp, *ret, transform);

    return ret;
}
static int g_total_frame = 0;
static void write_cloud_to_file(std::shared_ptr<pcMsg> msg)
{
    if (msg)
    {
        myPointCloud::Ptr one_frame(new myPointCloud);
        for (size_t i = 0; i < msg->points.size(); i++)
        {
            myPoint tmp;
            tmp.x = msg->points[i].x;
            tmp.y = msg->points[i].y;
            tmp.z = msg->points[i].z;
            tmp.r = 255;
            tmp.g = 255;
            tmp.b = 255;
            one_frame->push_back(tmp);
        }
        save_ply(one_frame);
        g_total_frame++;
    }
}

myPointCloud::Ptr make_cloud_by_msg(std::shared_ptr<pcMsg> _msg)
{
    const std::string config_sec = "range_filter";
    auto i_min = atof(get_ini_config()->get_config(config_sec, "i_min", "0").c_str());
    auto i_max = atof(get_ini_config()->get_config(config_sec, "i_max", "400").c_str());
    myPointCloud::Ptr one_frame(new myPointCloud);
    for (size_t i = 0; i < _msg->points.size(); i++)
    {
        if (_msg->points[i].intensity < i_min || _msg->points[i].intensity > i_max)
        {
            continue;
        }
        myPoint tmp;
        tmp.x = _msg->points[i].x;
        tmp.y = _msg->points[i].y;
        tmp.z = _msg->points[i].z;
        tmp.r = 255;
        tmp.g = 255;
        tmp.b = 255;
        one_frame->push_back(tmp);
    }

    return one_frame;
}

void process_msg(std::shared_ptr<pcMsg> msg)
{
    if (msg)
    {
        save_full_cloud(msg);
        auto one_frame = make_cloud_by_msg(msg);
        // 坐标转换
        auto frame_after_trans = pc_transform(one_frame);
        // 有效范围过滤
        auto frame_after_filter = pc_range_filter(frame_after_trans);
        // 体素滤波
        frame_after_filter = pc_vox_filter(frame_after_filter);
        // 计算关键数据
        auto detect_ret = pc_get_state(frame_after_filter);
        save_detect_result(detect_ret);
    }
}

RS_DRIVER::RS_DRIVER(const std::string &_config_file) : common_driver("RD_DRIVER")
{
    g_ini_config = new AD_INI_CONFIG(_config_file);
}

RS_DRIVER::~RS_DRIVER()
{
    using namespace robosense::lidar;
    if (this->common_rs_driver_ptr)
    {
        delete reinterpret_cast<LidarDriver<pcMsg> *>(this->common_rs_driver_ptr);
    }
}

bool RS_DRIVER::running_status_check()
{
    return true;
}

std::unique_ptr<robosense::lidar::RSDriverParam> init_rs_param(const std::string &_file_name)
{
    using namespace robosense::lidar;
    std::unique_ptr<RSDriverParam> param(new RSDriverParam);
    param->decoder_param.dense_points = true;
    param->input_param.msop_port = 6699;
    param->input_param.difop_port = 7788;
    param->lidar_type = LidarType::RSAIRY;
    param->input_type = InputType::ONLINE_LIDAR;
    if (!_file_name.empty())
    {
        param->input_type = InputType::PCAP_FILE;
        param->input_param.pcap_path = _file_name;
        param->input_param.pcap_rate = 1000;
        param->input_param.pcap_repeat = false;
    }
    return param;
}

void bind_callback2driver(robosense::lidar::LidarDriver<pcMsg> &driver, bool _for_file)
{
    if (_for_file)
    {
        driver.regPointCloudCallback(create_new_msg, write_cloud_to_file);
    }
    else
    {
        driver.regPointCloudCallback(driverGetPointCloudFromCallerCallback, driverReturnPointCloudToCallerCallback);
        std::thread(processCloud).detach();
    }
}

void RS_DRIVER::start(const std::string &_file, int _interval_sec)
{
    using namespace robosense::lidar;

    this->common_rs_driver_ptr = (void *)new LidarDriver<pcMsg>();
    auto &driver = *reinterpret_cast<LidarDriver<pcMsg> *>(this->common_rs_driver_ptr);

    bind_callback2driver(driver, !_file.empty());
    g_rd_result.state = vehicle_position_detect_state::vehicle_postion_out;
    g_rd_result.is_full = false;
    driver.regExceptionCallback([this](const Error &e)
                                { m_logger.log(AD_LOGGER::ERROR, "Lidar driver exception: %s", e.toString().c_str()); });
    auto param_ptr = init_rs_param(_file);
    if (driver.init(*param_ptr))
    {
        if (driver.start())
        {
            AD_RPC_SC::get_instance()->startTimer(
                0,
                200, [this]()
                {
                    auto cur_result = get_detect_result();
                    sim_vehicle_position(cur_result.state);
                    sim_vehicle_stuff(cur_result.is_full);
                    sim_full_offset(cur_result.full_offset);
                    ad_rpc_update_current_state();
                    serial_pc(); });
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Lidar driver start failed.");
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "Lidar driver init failed.");
    }
}

void RS_DRIVER::save_ply_file(std::string &_return, const std::string &reason)
{
    auto date_string = ad_utils_date_time().m_datetime;
    std::string ply_file_name = reason + "_" + date_string + ".ply";
    _return = "/database/" + ply_file_name;
    std::string full_ply = "/database/full_" + ply_file_name;
    save_ply(get_cur_cloud(), _return);
    save_ply(get_full_cloud(), full_ply);
}

bool should_stop_walk()
{
    auto cur_count = g_total_frame;
    usleep(1000000);
    return cur_count == g_total_frame;
}

void process_one_plyfile(const std::string &file_name)
{
    std::shared_ptr<pcMsg> cloud = std::make_shared<pcMsg>();
    if (pcl::io::loadPLYFile(file_name, *cloud) == -1)
    {
        rd_print_log_2stdout("Couldn't read file %s \n", file_name.c_str());
        return;
    }
    update_cur_cloud();
    process_msg(cloud);
    auto date_string = ad_utils_date_time().m_datetime;
    std::string ply_file_name = "after_one_process_" + date_string + ".ply";
    std::string out_file_name = "/database/" + ply_file_name;
    save_ply(get_cur_cloud(), out_file_name);
    auto ret = get_detect_result();
    rd_print_log_2stdout("out_file:%s, state:%d, is_full:%d", out_file_name.c_str(), ret.state, ret.is_full);
}

AD_INI_CONFIG *get_ini_config()
{
    return g_ini_config;
}

void save_detect_result(const vehicle_rd_detect_result &result)
{
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    g_rd_result = result;
}

vehicle_rd_detect_result get_detect_result()
{
    vehicle_rd_detect_result ret;
    std::lock_guard<std::recursive_mutex> lock(g_rd_result_mutex);
    ret = g_rd_result;
    return ret;
}
