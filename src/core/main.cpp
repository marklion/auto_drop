#include "rpc_imp_lib/config_rpc.h"
#include "../rpc/ad_rpc.h"
#include "../public/const_var_define.h"
#include "../rpc/gen_code/cpp/runner_sm.h"
#include "../rpc/gen_code/cpp/driver_service.h"
int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto sc = AD_RPC_SC::get_instance();
    sc->enable_rpc_server(AD_CONST_CONFIG_LISTEN_PORT);
    sc->add_rpc_server("config_management", std::make_shared<config_managementProcessor>(std::make_shared<config_management_impl>()));

    sm_config tmp_sm;
    device_config tmp_device;
    AD_EVENT_SC_TIMER_NODE_PTR ot1t = sc->startTimer(
        1,
        [&]()
        {
            sc->stopTimer(ot1t);
            sc->call_remote<config_managementClient>(
                AD_CONST_CONFIG_LISTEN_PORT,
                "config_management",
                [&](config_managementClient &client)
                {
                    client.start_sm(tmp_sm, "test_sm", {{"/conf/sample.yaml"}});
                    client.start_device(tmp_device, "mock_driver", std::vector<std::string>(), "test_device");
                });
        });
    AD_EVENT_SC_TIMER_NODE_PTR ot2t = sc->startTimer(
        2,
        [&]()
        {
            sc->stopTimer(ot2t);
            sc->call_remote<runner_smClient>(
                tmp_sm.port,
                "runner_sm",
                [&](runner_smClient client)
                {
                    client.match_device(AD_CONST_DEVICE_LED, tmp_device.port);
                    client.match_device(AD_CONST_DEVICE_PLATE_CAMERA, tmp_device.port);
                });
        });
    sc->start_server();
    return 0;
}
