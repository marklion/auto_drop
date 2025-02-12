#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/processor/TMultiplexedProcessor.h>
#include "ad_rpc.h"

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