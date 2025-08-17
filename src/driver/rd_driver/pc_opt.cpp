#include "pc_opt.h"
#include <pcl/common/transforms.h>
#include <pcl/common/impl/angles.hpp>
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/io/ply_io.h>
#include <pcl/io/pcd_io.h>
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
static std::mutex g_rd_result_mutex;

typedef pcl::PointXYZRGB myPoint;
typedef pcl::PointCloud<myPoint> myPointCloud;
typedef PointCloudT<myPoint> pcMsg;

robosense::lidar::SyncQueue<std::shared_ptr<pcMsg>> free_cloud_queue;
robosense::lidar::SyncQueue<std::shared_ptr<pcMsg>> stuffed_cloud_queue;
struct pc_after_pickup{
    myPointCloud::Ptr last;
    myPointCloud::Ptr picked;
};
static AD_INI_CONFIG *get_ini_config();
static void save_detect_result(const vehicle_rd_detect_result &result);
static vehicle_rd_detect_result get_detect_result();
static void process_msg(std::shared_ptr<pcMsg> msg);
static void serial_pc(pc_after_pickup &_pap);
void processCloud(void)
{
    while (true)
    {
        std::shared_ptr<pcMsg> msg = stuffed_cloud_queue.popWait();
        if (msg.get() == NULL)
        {
            continue;
        }
        process_msg(msg);
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
    stuffed_cloud_queue.push(msg);
}
static void save_ply(myPointCloud::Ptr cloud, const std::string &_filename = "")
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
static myPointCloud::Ptr find_points_on_plane(myPointCloud::Ptr _cloud, const Eigen::Vector3f &_ax_vec, float _distance_threshold, float _angle_threshold, pcl::ModelCoefficients::Ptr &_coe)
{
    auto ret = myPointCloud::Ptr(new myPointCloud);
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

    for (auto &itr : _cloud->points)
    {
        itr.r = 0;
        itr.g = 0;
        itr.b = 255;
    }

    pcl::ExtractIndices<myPoint> extract;
    extract.setInputCloud(_cloud);
    extract.setIndices(one_plane);
    extract.setNegative(false);
    extract.filter(*ret);
    for (auto &itr : ret->points)
    {
        itr.r = 255;
        itr.g = 255;
        itr.b = 0;
    }

    return ret;
}
static float pointToLineDistance(const Eigen::Vector3f &point, const Eigen::Vector3f &line_point, const Eigen::Vector3f &line_dir)
{
    Eigen::Vector3f diff = point - line_point;
    Eigen::Vector3f cross_product = diff.cross(line_dir);
    return cross_product.norm() / line_dir.norm();
}
static std::pair<myPoint, myPoint> get_key_seg(myPointCloud::Ptr _cloud, float line_distance_threshold, float plane_distance_threshold, float _full_y_offset, float _full_z_offset)
{
    myPoint max_z_point;
    max_z_point.z = std::numeric_limits<float>::lowest();

    float x_min = std::numeric_limits<float>::max();
    float x_max = std::numeric_limits<float>::min();
    float y_bar = 0;
    //遍历点云找到 最小x,最大x, 平均y
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
    max_z_point.y = y_bar / _cloud->points.size();

    float all_z[30];
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        all_z[i] = std::numeric_limits<float>::lowest();
    }

    //在x轴方向分30份，找出每一份紧凑点集中的最大z值
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        for (const auto &point : _cloud->points)
        {
            if (std::abs(point.x - (x_min + (x_max - x_min) * i / (sizeof(all_z) / sizeof(float)))) < line_distance_threshold && point.z > all_z[i])
            {
                all_z[i] = point.z;
            }
        }
    }
    //计算得到一个平均最大z值的点
    float total_z = 0;
    for (int i = 0; i < sizeof(all_z) / sizeof(float); i++)
    {
        total_z += all_z[i];
    }
    max_z_point.z = total_z / (sizeof(all_z) / sizeof(float));
    max_z_point.x = 0;
    max_z_point.z += _full_z_offset;

    //找出距离过上点平行于y轴的直线比较紧凑的若干点
    std::vector<myPoint> line_points;
    Eigen::Vector3f key_line_dir(1, 0, 0);
    for (auto &itr : _cloud->points)
    {
        if (pointToLineDistance(itr.getVector3fMap(), max_z_point.getVector3fMap(), key_line_dir) < plane_distance_threshold)
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
        return std::make_pair(max_z_point, max_z_point);
    }
    auto begin = line_points.front();
    auto end = line_points.front();
    float tmp_x = begin.x;

    //找出刚才找到的点集中距离较大的两个点作为直线的两点并返回
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
    begin.y = max_z_point.y + _full_y_offset;
    end.y = max_z_point.y + _full_y_offset;
    begin.z = max_z_point.z;
    end.z = max_z_point.z;

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
static myPointCloud::Ptr g_cur_cloud;
static myPointCloud::Ptr g_full_cloud;
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

    result->last = myPointCloud::Ptr(new myPointCloud);
    result->picked = myPointCloud::Ptr(new myPointCloud);

    split_cloud_by_pt(_orig_pc, "x", x_min, x_max, result->picked, tmp);
    *result->last += *tmp;
    split_cloud_by_pt(result->picked, "y", y_min, y_max, result->picked, tmp);
    *result->last += *tmp;

    return result;
}

void serial_pc(pc_after_pickup &_pap)
{
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    g_cur_cloud = _pap.last;
    g_update_counter++;
    if (g_update_counter >= 5)
    {
        serializePointCloud(g_cur_cloud);
        g_update_counter = 0; // 重置计数器
    }
}

static std::unique_ptr<pc_after_pickup> pc_get_state(myPointCloud::Ptr _cloud)
{
    vehicle_rd_detect_result ret;
    ret.is_full = false;
    ret.state = vehicle_position_detect_state::vehicle_postion_out;
    const std::string config_sec = "get_state";

    auto plane_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "plane_DistanceThreshold", "0.1").c_str());
    auto line_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "line_DistanceThreshold", "0.1").c_str());
    auto AngleThreshold = atof(get_ini_config()->get_config(config_sec, "AngleThreshold", "0.1").c_str());
    auto E0 = atof(get_ini_config()->get_config(config_sec, "E0", "0.1").c_str());
    auto E1 = atof(get_ini_config()->get_config(config_sec, "E1", "0.1").c_str());
    auto B_0 = atof(get_ini_config()->get_config(config_sec, "B0", "0.1").c_str());
    auto B1 = atof(get_ini_config()->get_config(config_sec, "B1", "0.1").c_str());
    auto full_y_offset = atof(get_ini_config()->get_config(config_sec, "full_y_offset", "0.1").c_str());
    auto full_z_offset = atof(get_ini_config()->get_config(config_sec, "full_z_offset", "0.1").c_str());
    auto line_require_points = atoi(get_ini_config()->get_config(config_sec, "line_require_points", "10").c_str());

    // 取出关心范围内的点云
    auto pickup_resp = pickup_pc_from_spec_range(_cloud);

    // 在范围内点云中拟合垂直y轴的平面,范围内点云染蓝色，平面染黄色
    pcl::ModelCoefficients::Ptr coe_side(new pcl::ModelCoefficients);
    auto side_points = find_points_on_plane(pickup_resp->picked, Eigen::Vector3f(0, 1, 0), plane_DistanceThreshold, AngleThreshold, coe_side);

    // 取出关键线段
    auto key_seg = get_key_seg(side_points, line_DistanceThreshold, plane_DistanceThreshold, 0, full_z_offset);
    if (key_seg.first.x != key_seg.second.x)
    {
        auto ex = key_seg.first.x;
        auto bx = key_seg.second.x;
        AD_LOGGER tmp_logger("LIDAR");
        tmp_logger.log("ex:%f, bx:%f", ex, bx);
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
        ret.is_full = false;
        if (ret.state != vehicle_position_detect_state::vehicle_postion_out)
        {
            myPointCloud::Ptr focus_side = myPointCloud::Ptr(new myPointCloud);
            for (auto &itr : side_points->points)
            {
                if (itr.x < key_seg.second.x)
                {
                    focus_side->points.push_back(itr);
                    itr.g = 255;
                    itr.r = 0;
                    itr.b = 0;
                }
            }
            auto full_seg = get_key_seg(focus_side, line_DistanceThreshold, plane_DistanceThreshold, full_y_offset, full_z_offset);
            insert_several_points(pickup_resp->last, full_seg.first, full_seg.second, true);
            long full_detect_count = 0;
            for (auto &itr : pickup_resp->picked->points)
            {
                if (itr.x > E1 && itr.x < B_0 && plane_DistanceThreshold > pointToLineDistance(itr.getVector3fMap(), full_seg.first.getVector3fMap(), Eigen::Vector3f(1, 0, 0)))
                {
                    full_detect_count++;
                }
            }
            if (full_detect_count > line_require_points)
            {
                ret.is_full = true;
            }
            else
            {
                ret.is_full = false;
            }
            tmp_logger.log("full_detect_count:%ld", full_detect_count);
        }
    }
    insert_several_points(pickup_resp->last, key_seg.first, key_seg.second);
    *pickup_resp->last += *side_points;
    save_detect_result(ret);

    return pickup_resp;
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
    myPointCloud::Ptr one_frame(new myPointCloud);
    for (size_t i = 0; i < _msg->points.size(); i++)
    {
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

long long get_current_us_stamp()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long current_useconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    return current_useconds;
}

void process_msg(std::shared_ptr<pcMsg> msg)
{
    if (msg)
    {
        auto one_frame = make_cloud_by_msg(msg);
        auto start_us_stamp = get_current_us_stamp();
        g_full_cloud = one_frame;
        // 坐标转换
        auto frame_after_trans = pc_transform(one_frame);
        // 有效范围过滤
        auto frame_after_filter = pc_range_filter(frame_after_trans);
        // 计算关键数据
        auto pap = pc_get_state(frame_after_filter);
        serial_pc(*pap);

        auto end_us_stamp = get_current_us_stamp();
        AD_LOGGER tmp_logger("LIDAR");
        tmp_logger.log("take %ld us", end_us_stamp - start_us_stamp);
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
                    sim_vehicle_stuff(cur_result.is_full); });
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

void RS_DRIVER::save_ply_file(std::string &_return)
{
    _return = "/database/cur.ply";
    std::string full_ply = "/database/cur-full.ply";
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    save_ply(g_cur_cloud, _return);
    save_ply(g_full_cloud, full_ply);
}

bool should_stop_walk()
{
    auto cur_count = g_total_frame;
    usleep(1000000);
    return cur_count == g_total_frame;
}

AD_INI_CONFIG *get_ini_config()
{
    return g_ini_config;
}

void save_detect_result(const vehicle_rd_detect_result &result)
{
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    AD_LOGGER tmp_log("LIDAR");
    tmp_log.log(AD_LOGGER::INFO, "save_detect_result: %d %d", result.state, result.is_full);
    g_rd_result = result;
}

vehicle_rd_detect_result get_detect_result()
{
    vehicle_rd_detect_result ret;
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    ret = g_rd_result;
    return ret;
}
