#include "rpc_imp_lib/sm_rpc.h"
#include "../rpc/ad_rpc.h"
#include "../public/const_var_define.h"

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto sc = AD_RPC_SC::get_instance();
    sc->enable_rpc_server(std::stoul(argv[1]));
    auto runner = RUNNER::runner_init(YAML::LoadFile(argv[2])["sm"]);
    auto runner_sm = std::make_shared<runner_sm_impl>(runner);
    sc->registerNode(runner->m_event_sc);
    sc->add_rpc_server("runner_sm", std::make_shared<runner_smProcessor>(runner_sm));
    sc->start_server();

    return 0;
}
