#include "rpc_imp_lib/sm_rpc.h"
#include "../rpc/ad_rpc.h"
#include "../public/const_var_define.h"
#include "../rpc/gen_code/cpp/config_management.h"

static void match_device(u16 _port)
{
    std::vector<device_config> dev_list;
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client)
        {
            client.get_device_list(dev_list);
        });
    for (auto &dev : dev_list)
    {
        AD_RPC_SC::get_instance()->call_remote<runner_smClient>(
            _port,
            "runner_sm",
            [&](runner_smClient &client)
            {
                client.match_device(dev.device_name, dev.port);
            });
    }
}

int main(int argc, char const *argv[])
{
    auto sc = AD_RPC_SC::get_instance();
    sc->enable_rpc_server(std::stoul(argv[1]));
    auto runner = RUNNER::runner_init(YAML::LoadFile(argv[2])["sm"]);
    auto runner_sm = std::make_shared<runner_sm_impl>(runner);
    sc->add_rpc_server("runner_sm", std::make_shared<runner_smProcessor>(runner_sm));
    sc->add_co(
        [&]()
        {
            match_device(std::stoul(argv[1]));
            runner->begin<RUNNER>();
        });
    sc->start_server();

    return 0;
}
