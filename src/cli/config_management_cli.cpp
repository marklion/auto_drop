#include "config_management_cli.h"

static void list_device(std::ostream &out, std::vector<std::string> _params)
{
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client){
            std::vector<device_config> device_list;
            client.get_device_list(device_list);
            for (auto &itr:device_list)
            {
                out << itr.device_name << std::endl;
            }
        });
}

static std::unique_ptr<cli::Menu> make_config_management_menu()
{
    auto menu = std::unique_ptr<cli::Menu>(new cli::Menu("config-management"));
    menu->Insert(CLI_MENU_ITEM(list_device), "列出设备");
    return menu;
}

config_management_cli::config_management_cli():common_cli(make_config_management_menu(), "config-management")
{
}

std::string config_management_cli::make_bdr()
{
    return std::string();
}
