#include "device_config.h"

static std::vector<std::string> get_all_driver(){
    std::vector<std::string> ret;
    std::string list_cmd = "ls /bin/ | grep '_driver'";
    std::string cmd_out;
    auto pfile = popen(list_cmd.c_str(), "r");
    if (pfile)
    {
        char buffer[1024] = {0};
        while (fgets(buffer, sizeof(buffer), pfile))
        {
            cmd_out += buffer;
        }
        pclose(pfile);
    }
    std::string::size_type pos = 0;
    std::string::size_type pre_pos = 0;
    while ((pos = cmd_out.find('\n', pre_pos)) != std::string::npos)
    {
        ret.push_back(cmd_out.substr(pre_pos, pos - pre_pos));
        pre_pos = pos + 1;
    }
    return ret;
}

static void list_driver(std::ostream &out, std::vector<std::string> _params)
{
    auto drivers = get_all_driver();
    for (const auto &itr : drivers)
    {
        out << itr << std::endl;
    }
}

YAML::Node make_new_device(const std::string &_driver_name, const std::string &_device_name, const std::string &_args)
{
    YAML::Node new_device;
    new_device["name"] = _device_name;
    new_device["driver"] = _driver_name;
    new_device["args"] = _args;
    return new_device;
}

static void add_device(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "驱动名称");
    check_ret += common_cli::check_params(_params, 1, "设备名称");
    check_ret += common_cli::check_params(_params, 2, "参数列表");
    if (check_ret.empty())
    {
        auto all_driver = get_all_driver();
        if (std::find(all_driver.begin(), all_driver.end(), _params[0]) == all_driver.end())
        {
            out << "驱动" << _params[0] << "不存在" << std::endl;
            return;
        }
        auto orig_node = common_cli::read_config_file();
        auto devices = orig_node["devices"];
        for (const auto &itr : devices)
        {
            auto device_name = itr["name"].as<std::string>("");
            if (device_name == _params[1])
            {
                out << "设备" << _params[1] << "已存在" << std::endl;
                return;
            }
        }
        auto new_device = make_new_device(_params[0], _params[1], _params[2]);
        devices.push_back(new_device);
        common_cli::write_config_file(orig_node);
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static std::unique_ptr<cli::Menu> make_device_menu()
{
    std::unique_ptr<cli::Menu> sm_menu(new cli::Menu("device"));
    sm_menu->Insert(CLI_MENU_ITEM(list_driver), "列出驱动");
    sm_menu->Insert(CLI_MENU_ITEM(add_device), "添加设备", {"驱动名称", "设备名称", "参数列表"});
    return sm_menu;
}
DEVICE_CONFIG_CLI::DEVICE_CONFIG_CLI() : common_cli(make_device_menu(), "device")
{
}

std::string DEVICE_CONFIG_CLI::make_bdr()
{

    std::string ret;
    auto node = common_cli::read_config_file();
    auto devices = node["devices"];
    for (const auto &itr : devices)
    {
        auto dev_name = itr["name"].as<std::string>();
        auto driver_name = itr["driver"].as<std::string>();
        auto args = itr["args"].as<std::string>();
        ret += "add_device " + driver_name + " " + dev_name + " '" + args + "'\n";
    }
    return ret;
}

void DEVICE_CONFIG_CLI::clear()
{
    auto node = common_cli::read_config_file();
    auto devices = node["devices"];
    devices = YAML::Load("[]");
    common_cli::write_config_file(node);
}
