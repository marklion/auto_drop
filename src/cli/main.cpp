#include "cli.h"
#include "loopscheduler.h"
#include "clilocalsession.h"
#include "../rpc/ad_rpc.h"

int main(int argc, char const *argv[])
{
    auto sc = AD_RPC_SC::get_instance();
    auto root_menu = std::unique_ptr<cli::Menu>(new cli::Menu("ad"));
    cli::Cli cli(std::move(root_menu));
    AD_EVENT_SC_TIMER_NODE_PTR ott = sc->startTimer(
        1,
        [&]()
        {
            sc->stopTimer(ott);
            cli::LoopScheduler lsc;
            cli::CliLocalTerminalSession ss(cli, lsc, std::cout);
            lsc.Run();
        });

    sc->start_server();

    return 0;
}
