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
    sc->start_server();
    return 0;
}
