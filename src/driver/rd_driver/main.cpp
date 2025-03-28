#include "../../rpc/ad_rpc.h"
#include "pc_opt.h"

int main(int argc, char const *argv[])
{
    if (argc > 2)
    {
        std::string config_file_path = argv[2];
        auto driver = std::make_shared<RS_DRIVER>(config_file_path);
        driver->start();
        driver->start_driver_daemon(argc, argv);
    }
    else if (argc == 2)
    {
        auto driver = std::make_shared<RS_DRIVER>("");
        driver->start(argv[1]);
        while (true)
        {
            if (should_stop_walk())
            {
                break;
            }
        }
    }
    return 0;
}
