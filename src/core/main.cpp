#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/TProcessor.h>
#include <thrift/processor/TMultiplexedProcessor.h>
#include <thrift/protocol/TMultiplexedProtocol.h>
#include <signal.h>
#include "runner.h"
#include "sm_rpc.h"
#include "../rpc/ad_rpc.h"

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto runner = RUNNER::runner_init(YAML::LoadFile(argv[1])["sm"]);
    auto runner_sm = std::make_shared<runner_sm_impl>(runner);

    auto event_sc = std::make_shared<AD_EVENT_SC>();
    event_sc->registerNode(runner->m_event_sc);
    event_sc->startTimer(4, [&](){
        auto client_socket_trans = std::make_shared<apache::thrift::transport::TSocket>("localhost", 12345);
        auto client_buffer_trans = std::make_shared<apache::thrift::transport::TBufferedTransport>(client_socket_trans);
        auto client_protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(client_buffer_trans);
        auto mt_protocol = std::make_shared<apache::thrift::protocol::TMultiplexedProtocol>(client_protocol, "runner_sm");
        runner_smClient client(mt_protocol);
        client_buffer_trans->open();
        client.push_sm_event("vehicle_arrived");
        client_buffer_trans->close();
    });
    event_sc->startTimer(20, [&](){
        event_sc->stopEventLoop();
    });
    auto rpc_listen_node =  std::make_shared<AD_RPC_EVENT_NODE>(12345, event_sc);
    rpc_listen_node->add_processor("runner_sm", std::make_shared<runner_smProcessor>(runner_sm));
    event_sc->registerNode(rpc_listen_node);
    event_sc->runEventLoop();

    return 0;
}
