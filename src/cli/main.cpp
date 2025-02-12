#include "cli.h"
#include "loopscheduler.h"
#include "clilocalsession.h"
#include "../rpc/ad_rpc.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "sm_config.h"
#include "device_config.h"
#include "runtime_config.h"

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_global_log_level(AD_LOGGER::ERROR);
    common_cli *sub_c[] = {
        new SM_CONFIG_CLI(), new DEVICE_CONFIG_CLI(), new RUNTIME_CONFIG_CLI()};
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
    root_menu->Insert(
        "bash",
        [&](std::ostream &_out)
        {
            constexpr tcflag_t ICANON_FLAG = ICANON;
            constexpr tcflag_t ECHO_FLAG = ECHO;

            termios oldt;
            termios newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag |= (ICANON_FLAG | ECHO_FLAG);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            system("/bin/bash");
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        });

    cli::LoopScheduler *plsc = nullptr;
    cli::Cli cli(std::move(root_menu));
    cli.ExitAction(
        [&](std::ostream &out)
        {
            AD_RPC_SC::get_instance()->stop_server();
            plsc->Stop();
        });

    cli::LoopScheduler lsc;
    cli::CliLocalTerminalSession ss(cli, lsc, std::cout);
    plsc = &lsc;
    auto sc = AD_RPC_SC::get_instance();
    auto cli_co = sc->add_co(
        [&]()
        {
            lsc.Run(ss);
        });
    sc->resume_co(cli_co);

    sc->start_server();

    return 0;
}
