#include "rpc_imp_lib/config_rpc.h"
#include "../rpc/ad_rpc.h"
#include "../public/const_var_define.h"
#include "../rpc/gen_code/cpp/runner_sm.h"
#include "../rpc/gen_code/cpp/driver_service.h"

void config_import_run()
{
    std::string cmd = "ad_cli /database/init.txt";
    AD_RPC_SC::get_instance()->non_block_system(cmd);
}

int main(int argc, char const *argv[])
{
    auto sc = AD_RPC_SC::get_instance();
    sc->enable_rpc_server(AD_CONST_CONFIG_LISTEN_PORT);
    sc->add_rpc_server("config_management", std::make_shared<config_managementProcessor>(std::make_shared<config_management_impl>()));
    sc->add_co(config_import_run);
    sc->start_server();
    return 0;
}
