#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/processor/TMultiplexedProcessor.h>
#include "ad_rpc.h"
#include "gen_code/cpp/config_management.h"
#include "gen_code/cpp/driver_service.h"
#include "../public/const_var_define.h"
#include "../public/utils/CJsonObject.hpp"

class my_rpc_trans : public AD_EVENT_SC_TCP_DATA_NODE
{
    std::shared_ptr<apache::thrift::TMultiplexedProcessor> m_processor = std::make_shared<apache::thrift::TMultiplexedProcessor>();
    AD_LOGGER m_logger = AD_LOGGER("rpc");
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
        AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
            port,
            "driver_service",
            [&](driver_serviceClient &client)
            {
                client.save_ply_file(ret);
            });
    }

    return ret;
}

std::string ad_rpc_get_current_state()
{
    neb::CJsonObject json_obj;
    AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
        ad_rpc_get_specific_dev_port(AD_CONST_RIDAR_DEVICE_NAME),
        "driver_service",
        [&](driver_serviceClient &client)
        {
            vehicle_rd_detect_result rd_result;
            client.vehicle_rd_detect(rd_result);
            json_obj.Add(AD_CONST_REDIS_KEY_RD_POSITION, rd_result.state);
            json_obj.Add(AD_CONST_REDIS_KEY_RD_FULL, rd_result.is_full);
        });

    AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
        ad_rpc_get_specific_dev_port(AD_CONST_SCALE_DEVICE_NAME),
        "driver_service",
        [&](driver_serviceClient &client)
        {
            json_obj.Add(AD_CONST_REDIS_KEY_SCALE, client.get_scale_weight());
        });
    AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
        ad_rpc_get_specific_dev_port(AD_CONST_LC_DEVICE_NAME),
        "driver_service",
        [&](driver_serviceClient &client)
        {
            json_obj.Add(AD_CONST_REDIS_KEY_LC_OPEN_THRESHOLD, client.get_lc_open());
        });

    return json_obj.ToString();
}
