#if !defined(_AD_RPC_H_)
#define _AD_RPC_H_
#include <thrift/TProcessor.h>
#include "../public/event_sc/ad_event_sc.h"
#include "gen_code/cpp/idl_types.h"

class AD_RPC_EVENT_NODE : public AD_EVENT_SC_TCP_LISTEN_NODE
{
    std::map<std::string, std::shared_ptr<apache::thrift::TProcessor>> m_processor_map;
    AD_EVENT_SC_TCP_DATA_NODE_PTR create_rpc_tcp_data_node(int fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR listen_node);

public:
    AD_RPC_EVENT_NODE(
        unsigned short _port,
        AD_EVENT_SC_PTR _event_sc) : AD_EVENT_SC_TCP_LISTEN_NODE(_port, [this](int _fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR _listen_node)
                                                                 { return create_rpc_tcp_data_node(_fd, _listen_node); }, _event_sc)
    {
    }
    void add_processor(const std::string &service_name, std::shared_ptr<apache::thrift::TProcessor> processor)
    {
        m_processor_map[service_name] = processor;
    }
};

#endif // _AD_RPC_H_
