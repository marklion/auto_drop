#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/processor/TMultiplexedProcessor.h>
#include "ad_rpc.h"
#include "gen_code/cpp/config_management.h"
#include "gen_code/cpp/driver_service.h"
#include "../public/const_var_define.h"
#include "../public/utils/CJsonObject.hpp"
#include <yaml-cpp/yaml.h>
#include "../public/event_sc/ad_redis.h"
#include "rpc_wrapper.h"
#include "../public/sion.h"

class my_rpc_trans : public AD_EVENT_SC_TCP_DATA_NODE
{
    std::shared_ptr<apache::thrift::TMultiplexedProcessor> m_processor = std::make_shared<apache::thrift::TMultiplexedProcessor>();
    AD_LOGGER m_logger = AD_LOGGER("RPC");

public:
    using AD_EVENT_SC_TCP_DATA_NODE::AD_EVENT_SC_TCP_DATA_NODE;
    void handleRead(const unsigned char *buf, size_t len) override
    {
        auto it = std::make_shared<apache::thrift::transport::TMemoryBuffer>((uint8_t *)buf, len);
        auto it_ad_trans = std::make_shared<AD_RPC_TRANSPORT>(it);
        auto ip = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(it_ad_trans);
        auto ot = std::make_shared<apache::thrift::transport::TMemoryBuffer>();
        auto ot_ad_trans = std::make_shared<AD_RPC_TRANSPORT>(ot);
        auto op = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(ot_ad_trans);
        m_processor->process(ip, op, nullptr);
        auto reply = ot->getBufferAsString();
        send(getFd(), reply.c_str(), reply.size(), SOCK_NONBLOCK);
    }
    void add_processor(const std::string &service_name, std::shared_ptr<apache::thrift::TProcessor> processor)
    {
        m_processor->registerProcessor(service_name, processor);
    }
};

AD_EVENT_SC_TCP_DATA_NODE_PTR AD_RPC_EVENT_NODE::create_rpc_tcp_data_node(int fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR listen_node)
{
    auto ret = std::make_shared<my_rpc_trans>(fd, listen_node);

    for (auto &pair : m_processor_map)
    {
        ret->add_processor(pair.first, pair.second);
    }

    return ret;
}

std::shared_ptr<AD_RPC_SC> AD_RPC_SC::m_single;

std::vector<device_config> ad_rpc_get_run_dev()
{
    std::vector<device_config> dev_list;
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client)
        {
            client.get_device_list(dev_list);
        });
    return dev_list;
}

u16 ad_rpc_get_specific_dev_port(const std::string &dev_name)
{
    u16 ret = 0;
    auto dev_list = ad_rpc_get_run_dev();
    for (const auto &dev : dev_list)
    {
        if (dev.device_name == dev_name)
        {
            ret = dev.port;
            break;
        }
    }

    return ret;
}

std::vector<sm_config> ad_rpc_get_run_sm()
{
    std::vector<sm_config> sm_list;
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client)
        {
            client.get_sm_list(sm_list);
        });
    return sm_list;
}

std::string ad_rpc_device_save_ply(const std::string &dev_name)
{
    std::string ret;
    u16 port = ad_rpc_get_specific_dev_port(dev_name);
    if (port > 0)
    {
        rpc_wrapper_call_device(
            port,
            [&](driver_serviceClient &client)
            {
                client.save_ply_file(ret, "");
            });
    }

    return ret;
}

static std::map<std::string, std::function<void(driver_serviceClient &, neb::CJsonObject &json_obj)>> g_all_state_get_map = {
    {AD_CONST_GATE_DEVICE_NAME,
     [](driver_serviceClient &client, neb::CJsonObject &json_obj)
     {
         auto gate = client.vehicle_passed_gate();
         json_obj.Add(AD_CONST_REDIS_KEY_GATE, gate, gate);
     }},
    {AD_CONST_PLATE_NAME,
     [](driver_serviceClient &client, neb::CJsonObject &json_obj)
     {
         std::string plate;
         client.get_trigger_vehicle_plate(plate);
         json_obj.Add(AD_CONST_REDIS_KEY_PLATE, plate);
     }},
    {AD_CONST_RIDAR_DEVICE_NAME,
     [](driver_serviceClient &client, neb::CJsonObject &json_obj)
     {
         vehicle_rd_detect_result rd_result;
         client.vehicle_rd_detect(rd_result);
         json_obj.Add(AD_CONST_REDIS_KEY_RD_POSITION, rd_result.state);
         json_obj.Add(AD_CONST_REDIS_KEY_RD_FULL, rd_result.is_full);
         json_obj.Add(AD_CONST_REDIS_KEY_RD_FULL_OFFSET, rd_result.full_offset);
         json_obj.Add(AD_CONST_REDIS_KEY_RD_SIDE_TOP_Z, rd_result.side_top_z);
     }},
    {AD_CONST_SCALE_DEVICE_NAME,
     [](driver_serviceClient &client, neb::CJsonObject &json_obj)
     {
         json_obj.Add(AD_CONST_REDIS_KEY_SCALE, client.get_scale_weight());
     }},
    {AD_CONST_LC_DEVICE_NAME,
     [](driver_serviceClient &client, neb::CJsonObject &json_obj)
     {
         json_obj.Add(AD_CONST_REDIS_KEY_LC_OPEN_THRESHOLD, client.get_lc_open());
     }}};

std::string ad_rpc_get_current_state()
{
    neb::CJsonObject json_obj;
    for (const auto &pair : g_all_state_get_map)
    {
        auto port = ad_rpc_get_specific_dev_port(pair.first);
        if (port > 0)
        {
            rpc_wrapper_call_device(
                port,
                [&](driver_serviceClient &client)
                {
                    pair.second(client, json_obj);
                });
        }
    }

    return json_obj.ToString();
}

void ad_rpc_gate_ctrl(const std::string &dev_name, bool _is_open)
{
    u16 port = ad_rpc_get_specific_dev_port(dev_name);
    if (port > 0)
    {
        rpc_wrapper_call_device(
            port,
            [&](driver_serviceClient &client)
            {
                client.gate_control(!_is_open);
            });
    }
}

void ad_rpc_update_current_state()
{
    AD_LOGGER tmp_log("STATE_UPDATE");
    auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
    AD_REDIS_HELPER helper(node, AD_RPC_SC::get_instance());
    helper.set(AD_REDIS_CHANNEL_CURRENT_STATE(), ad_rpc_get_current_state());
    auto req = sion::Request().SetUrl("http://localhost/api/update_state").SetHttpMethod("POST").SetBody("{}");
    try
    {
        auto resp = req.Send(2200);
    }
    catch (const std::exception &e)
    {
        tmp_log.log(AD_LOGGER::ERROR, "call_http_api error: %s", e.what());
    }
}

void ad_rpc_set_lc_open(const std::string &dev_name, int32_t threshold)
{
    u16 port = ad_rpc_get_specific_dev_port(dev_name);
    if (port > 0)
    {
        rpc_wrapper_call_device(
            port,
            [&](driver_serviceClient &client)
            {
                client.set_lc_open(threshold);
            });
    }
}

void AD_RPC_SC::start_co_record()
{
    startTimer(
        1, []()
        {
            std::string file_name = "/tmp/co_list";
            file_name += std::to_string(getpid()) + ".txt";
            std::ofstream out(file_name, std::ios::trunc);
            out << get_instance()->co_list(); });
}
