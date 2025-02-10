#include "cli.h"
#include "loopscheduler.h"
#include "clilocalsession.h"
#include "../rpc/ad_rpc.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "sm_config.h"

int main(int argc, char const *argv[])
{
    common_cli *sub_c[] = {
        new SM_CONFIG_CLI()};
    auto root_menu = std::unique_ptr<cli::Menu>(new cli::Menu("ad"));
    for (auto &itr : sub_c)
    {
        root_menu->Insert(std::move(itr->menu));
    }

    root_menu->Insert(
        "bdr",
        [&](std::ostream &_out)
        {
            for (auto &itr : sub_c)
            {
                _out << itr->menu_name << std::endl;
                _out << itr->make_bdr() << std::endl;
                _out << "ad" << std::endl;
            }
        });

    cli::Cli cli(std::move(root_menu));
    cli.ExitAction(
        [&](std::ostream &out)
        { AD_RPC_SC::get_instance()->stop_server(); });

    cli::LoopScheduler lsc;
    cli::CliLocalTerminalSession ss(cli, lsc, std::cout);
    auto sc = AD_RPC_SC::get_instance();
    auto cli_co = sc->add_co(
        [&]()
        {
            lsc.Run();
        });
    sc->resume_co(cli_co);

    sc->start_server();

    return 0;
}
