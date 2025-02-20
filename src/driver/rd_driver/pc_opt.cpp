#include "pc_opt.h"
#include <pcl/common/transforms.h>
#include <pcl/common/impl/angles.hpp>
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/filters/extract_indices.h>
#include <rs_driver/api/lidar_driver.hpp>
#include <rs_driver/msg/pcl_point_cloud_msg.hpp>
#include <mutex>
#include <thread>
#include "../../public/utils/ad_utils.h"
#include "../../rpc/ad_rpc.h"

static AD_INI_CONFIG *g_ini_config = nullptr;
static vehicle_rd_detect_result g_rd_result;
static std::mutex g_rd_result_mutex;

typedef pcl::PointXYZRGB myPoint;
typedef pcl::PointCloud<myPoint> myPointCloud;
typedef PointCloudT<myPoint> pcMsg;
static std::shared_ptr<pcMsg> create_new_msg()
{
    return std::make_shared<pcMsg>();
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

    for (auto &itr : one_plane->indices)
    {
        _cloud->points[itr].r = 255;
        _cloud->points[itr].g = (_ax_vec.y() > 0 ? 255 : 0);
        _cloud->points[itr].b = 0;
    }

    pcl::ExtractIndices<myPoint> extract;
    extract.setInputCloud(_cloud);
    extract.setIndices(one_plane);
    extract.setNegative(false);
    extract.filter(*ret);

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
    float x0_z = std::numeric_limits<float>::lowest();
    float x1_z = std::numeric_limits<float>::lowest();
    float x2_z = std::numeric_limits<float>::lowest();

    for (const auto &point : _cloud->points)
    {
        if (std::abs(point.x) < line_distance_threshold && point.z > x0_z)
        {
            x0_z = point.z;
        }
        if (std::abs(point.x - x_max) < line_distance_threshold && point.z > x1_z)
        {
            x1_z = point.z;
        }
        if (std::abs(point.x - x_min) < line_distance_threshold && point.z > x2_z)
        {
            x2_z = point.z;
        }
    }
    max_z_point.z = (x0_z + x1_z + x2_z) / 3;
    max_z_point.x = 0;

    max_z_point.z += _full_z_offset;

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
static void pc_get_state(myPointCloud::Ptr _cloud)
{
    vehicle_rd_detect_result ret;
    const std::string config_sec = "get_state";

    auto x_min = atof(get_ini_config()->get_config(config_sec, "x_min", "0").c_str());
    auto x_max = atof(get_ini_config()->get_config(config_sec, "x_max", "0").c_str());
    auto y_min = atof(get_ini_config()->get_config(config_sec, "y_min", "0").c_str());
    auto y_max = atof(get_ini_config()->get_config(config_sec, "y_max", "0").c_str());
    auto plane_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "plane_DistanceThreshold", "0.1").c_str());
    auto line_DistanceThreshold = atof(get_ini_config()->get_config(config_sec, "line_DistanceThreshold", "0.1").c_str());
    auto AngleThreshold = atof(get_ini_config()->get_config(config_sec, "AngleThreshold", "0.1").c_str());
    auto alpha = atof(get_ini_config()->get_config(config_sec, "alpha", "0.1").c_str());
    auto E0 = atof(get_ini_config()->get_config(config_sec, "E0", "0.1").c_str());
    auto E1 = atof(get_ini_config()->get_config(config_sec, "E1", "0.1").c_str());
    auto B_0 = atof(get_ini_config()->get_config(config_sec, "B0", "0.1").c_str());
    auto B1 = atof(get_ini_config()->get_config(config_sec, "B1", "0.1").c_str());
    auto full_y_offset = atof(get_ini_config()->get_config(config_sec, "full_y_offset", "0.1").c_str());
    auto full_z_offset = atof(get_ini_config()->get_config(config_sec, "full_z_offset", "0.1").c_str());
    auto line_require_points = atoi(get_ini_config()->get_config(config_sec, "line_require_points", "10").c_str());

    myPointCloud::Ptr cloud = _cloud;

    auto cloud_filtered = myPointCloud::Ptr(new myPointCloud);
    auto cloud_last = myPointCloud::Ptr(new myPointCloud);
    auto tmp = myPointCloud::Ptr(new myPointCloud);
    split_cloud_by_pt(cloud, "x", x_min, x_max, cloud_filtered, tmp);
    *cloud_last += *tmp;
    split_cloud_by_pt(cloud_filtered, "y", y_min, y_max, cloud_filtered, tmp);
    *cloud_last += *tmp;

    pcl::ModelCoefficients::Ptr coe_side(new pcl::ModelCoefficients);
    auto side_points = find_points_on_plane(cloud_filtered, Eigen::Vector3f(0, 1, 0), plane_DistanceThreshold, AngleThreshold, coe_side);
    auto key_seg = get_key_seg(side_points, line_DistanceThreshold, plane_DistanceThreshold, full_y_offset, full_z_offset);
    if (key_seg.first.x != key_seg.second.x)
    {
        auto ex = key_seg.first.x;
        auto bx = key_seg.second.x;
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
            long full_detect_count = 0;
            for (auto &itr : cloud_filtered->points)
            {
                if (itr.x > E1 && itr.x < B_0 && plane_DistanceThreshold > pointToLineDistance(itr.getVector3fMap(), key_seg.first.getVector3fMap(), Eigen::Vector3f(1, 0, 0)))
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
            RS_DEBUG << "points on line:" << full_detect_count << RS_REND;
        }
    }
    save_detect_result(ret);
}
static void pc_range_filter(myPointCloud::Ptr cloud)
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

    pc_get_state(cloud_filtered_z);
}
static void pc_transform(myPointCloud::Ptr cloud)
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

    pc_range_filter(ret);
}
static void process_msg(std::shared_ptr<pcMsg> msg)
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
    pc_transform(one_frame);
}

RS_DRIVER::RS_DRIVER(const std::string &_config_file) : common_driver("RD_DRIVER")
{
    g_ini_config = new AD_INI_CONFIG(_config_file);
}

bool RS_DRIVER::running_status_check()
{
    return true;
}

void RS_DRIVER::start()
{
    using namespace robosense::lidar;
    RSDriverParam param;
    param.decoder_param.dense_points = true;
    param.input_param.msop_port = 6699;
    param.input_param.difop_port = 7788;
    param.lidar_type = LidarType::RSBP;
    param.input_type = InputType::ONLINE_LIDAR;

    LidarDriver<pcMsg> driver;
    driver.regPointCloudCallback(create_new_msg, process_msg);
    driver.regExceptionCallback([this](const Error &e)
                                { m_logger.log(AD_LOGGER::ERROR, "Lidar driver exception: %s", e.toString().c_str()); });
    if (driver.init(param))
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

AD_INI_CONFIG *get_ini_config()
{
    return g_ini_config;
}

void save_detect_result(const vehicle_rd_detect_result &result)
{
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    g_rd_result = result;
}

vehicle_rd_detect_result get_detect_result()
{
    vehicle_rd_detect_result ret;
    std::lock_guard<std::mutex> lock(g_rd_result_mutex);
    ret = g_rd_result;
    return ret;
}
