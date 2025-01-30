#include "runner.h"

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto runner = RUNNER::runner_init(YAML::LoadFile(argv[1])["sm"]);
    std::string key_input;
    while (key_input != "q")
    {
        std::cout <<"key map: " << std::endl;
        std::cout << "q: quit" << std::endl;
        std::cout << "r: reset" << std::endl;
        std::cout << "a: vehicle_arrived" << std::endl;
        std::cin >> key_input;
        if (key_input == "r")
        {
            runner->proc_event("reset");
        }
        else if (key_input == "a")
        {
            runner->proc_event("vehicle_arrived");
        }
    }
    return 0;
}
