#include "runner.h"

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto runner = RUNNER::runner_init(YAML::LoadFile(argv[1])["sm"]);

    runner->run_event_loop();

    return 0;
}
