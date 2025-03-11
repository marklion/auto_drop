#include "runtime_config.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "../rpc/gen_code/cpp/runner_sm.h"
#include "../rpc/gen_code/cpp/driver_service.h"
#include <fstream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/inotify.h>


static std::vector<device_config> get_run_dev()
{
    std::vector<device_config> dev_list;
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client)
        {
            client.get_device_list(dev_list);
        });
    return dev_list;
}

static std::vector<sm_config> get_run_sm()
{
    std::vector<sm_config> sm_list;
    AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
        AD_CONST_CONFIG_LISTEN_PORT,
        "config_management",
        [&](config_managementClient &client)
        {
            client.get_sm_list(sm_list);
        });
    return sm_list;
}

static void restart(std::ostream &out, std::vector<std::string> _params)
{
    auto dev_list = get_run_dev();
    auto sm_list = get_run_sm();
    for (auto &dev : dev_list)
    {
        AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
            AD_CONST_CONFIG_LISTEN_PORT,
            "config_management",
            [&](config_managementClient &client)
            {
                client.stop_device(dev.port);
            });
    }
    for (auto &sm : sm_list)
    {
        AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
            AD_CONST_CONFIG_LISTEN_PORT,
            "config_management",
            [&](config_managementClient &client)
            {
                client.stop_sm(sm.port);
            });
    }
    auto config = common_cli::read_config_file();
    if (!config.IsNull())
    {
        auto devices = config["devices"];
        auto sm = config["sm"];

        for (const auto &itr : devices)
        {
            device_config dev_ret;
            auto driver = itr["driver"].as<std::string>("");
            auto name = itr["name"].as<std::string>("");
            auto args = itr["args"].as<std::string>("");
            std::vector<std::string> args_v;
            std::string arg;
            for (auto c : args)
            {
                if (c == ' ')
                {
                    args_v.push_back(arg);
                    arg.clear();
                }
                else
                {
                    arg += c;
                }
            }
            args_v.push_back(arg);
            AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
                AD_CONST_CONFIG_LISTEN_PORT,
                "config_management",
                [&](config_managementClient &client)
                {
                    client.start_device(dev_ret, driver, args_v, name);
                });
            if (dev_ret.port <= 0)
            {
                out << "启动设备" << name << "失败" << std::endl;
                return;
            }
        }
        if (!sm.IsNull() && sm["init_state"].as<std::string>("") != "")
        {
            sm_config sm_ret;
            AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
                AD_CONST_CONFIG_LISTEN_PORT,
                "config_management",
                [&](config_managementClient &client)
                {
                    client.start_sm(sm_ret, "sm", {AD_CONST_CONFIG_FILE});
                });
            if (sm_ret.port <= 0)
            {
                out << "启动sm失败" << std::endl;
                return;
            }
        }
    }
}

static void list(std::ostream &out, std::vector<std::string> _params)
{
    auto dev_list = get_run_dev();
    for (const auto &dev : dev_list)
    {
        out << "设备名称:" << dev.device_name << "端口:" << dev.port << " 驱动名称:" << dev.driver_name << " 参数:";
        for (const auto &arg : dev.argv)
        {
            out << arg << " ";
        }
        out << std::endl;
    }
    auto sm_list = get_run_sm();
    for (const auto &sm : sm_list)
    {
        out << "sm名称:" << sm.sm_name << "端口:" << sm.port << " 参数:";
        for (const auto &arg : sm.argv)
        {
            out << arg << " ";
        }
        out << std::endl;
    }
}

static void mock(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "设备名称");
    check_ret += common_cli::check_params(_params, 1, "动作");
    check_ret += common_cli::check_params(_params, 2, "动作效果");
    if (check_ret.empty())
    {
        auto dev_list = get_run_dev();
        u16 port = 0;
        for (const auto &dev : dev_list)
        {
            if (dev.device_name == _params[0])
            {
                port = dev.port;
                break;
            }
        }
        if (port > 0)
        {
            AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
                port,
                "driver_service",
                [&](driver_serviceClient &client)
                {
                    if (_params[1] == "sim_vehicle_came")
                    {
                        client.sim_vehicle_came(_params[2]);
                        out << "模拟车辆到达" << std::endl;
                    }
                    else if (_params[1] == "sim_gate_status")
                    {
                        client.sim_gate_status(_params[2] == "close");
                        out << "模拟闸机状态" << std::endl;
                    }
                    else if (_params[1] == "sim_scale_weight")
                    {
                        client.sim_scale_weight(std::stod(_params[2]));
                        out << "模拟称重" << std::endl;
                    }
                    else if (_params[1] == "sim_vehicle_position")
                    {
                        client.sim_vehicle_position((vehicle_position_detect_state::type)std::stoi(_params[2]));
                        out << "模拟车辆位置" << std::endl;
                    }
                    else if (_params[1] == "sim_vehicle_stuff")
                    {
                        client.sim_vehicle_stuff(_params[2] == "full");
                        out << "模拟车辆满载" << std::endl;
                    }
                });
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static AD_CO_ROUTINE_PTR g_log_co;
static bool g_log_open = false;

struct log_output_config
{
    std::string log_level;
    std::string log_module;
    int operator==(const log_output_config &_rhs) const
    {
        return log_level == _rhs.log_level && log_module == _rhs.log_module;
    }
};

static std::vector<log_output_config> g_log_config;

static bool log_has_keyword(const std::string &_log, const std::vector<log_output_config> &_keywords)
{
    for (const auto &keyword : _keywords)
    {
        if (_log.find(keyword.log_module) != std::string::npos && _log.find(keyword.log_level) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
static std::string read_new_content(std::ifstream &_file)
{
    std::string ret;
    std::string line;
    while (std::getline(_file, line))
    {
        if (log_has_keyword(line, g_log_config))
        {
            ret += line + "\n";
        }
    }
    _file.clear();
    _file.seekg(0, std::ios::end);
    return ret;
}
static void open_log(std::ostream &out, std::vector<std::string> _params)
{
    g_log_open = true;
    if (!g_log_co)
    {
        g_log_co = AD_RPC_SC::get_instance()->add_co(
            [&]()
            {
                const std::string filename = AD_CONST_LOGFILE_PATH;
                std::ifstream file_s(filename, std::ios::in);
                file_s.seekg(0, std::ios::end);
                int fd = inotify_init();
                if (fd >= 0)
                {
                    int wd = inotify_add_watch(fd, filename.c_str(), IN_MODIFY);
                    if (wd >= 0)
                    {
                        char buffer[BUF_LEN];
                        while (g_log_open)
                        {
                            AD_RPC_SC::get_instance()->yield_by_fd(fd);
                            int length = read(fd, buffer, BUF_LEN);
                            if (length > 0)
                            {
                                int i = 0;
                                while (i < length)
                                {
                                    struct inotify_event *event = (struct inotify_event *)&buffer[i];
                                    if (event->mask & IN_MODIFY)
                                    {
                                        auto log_content = read_new_content(file_s);
                                        out << log_content;
                                        out.flush();
                                    }
                                    i += EVENT_SIZE + event->len;
                                }
                            }
                        }

                        inotify_rm_watch(fd, wd);
                    }
                    else
                    {
                        out << "Failed to add watch: " << strerror(errno) << std::endl;
                    }

                    close(fd);
                }
                else
                {
                    out << "Failed to init inotify: " << strerror(errno) << std::endl;
                }
            });
    }
}
static void close_log(std::ostream &out, std::vector<std::string> _params)
{
    g_log_open = false;
    g_log_co.reset();
}

static void enable_log(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "模块");
    check_ret += common_cli::check_params(_params, 1, "日志级别");
    if (check_ret.empty())
    {
        log_output_config config;
        config.log_level = _params[1];
        config.log_module = _params[0];
        if (std::find(g_log_config.begin(), g_log_config.end(), config) == g_log_config.end())
        {
            g_log_config.push_back(config);
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static void disable_log(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "模块");
    check_ret += common_cli::check_params(_params, 1, "日志级别");
    if (check_ret.empty())
    {
        log_output_config config;
        config.log_level = _params[1];
        config.log_module = _params[0];
        auto it = std::find(g_log_config.begin(), g_log_config.end(), config);
        if (it != g_log_config.end())
        {
            g_log_config.erase(it);
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}

void reset_sm(std::ostream &out, std::vector<std::string> _params)
{
    u16 sm_port = 0;
    auto run_sm = get_run_sm();
    if (run_sm.size() > 0)
    {
        sm_port = run_sm[0].port;
    }

    if (sm_port > 0)
    {
        AD_RPC_SC::get_instance()->call_remote<runner_smClient>(
            sm_port,
            "runner_sm",
            [&](runner_smClient &client)
            {
                client.reset_sm();
            });
    }
}

static void save_ply(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "设备名");
    if (check_ret.empty())
    {
        auto dev_list = get_run_dev();
        u16 port = 0;
        for (const auto &dev : dev_list)
        {
            if (dev.device_name == _params[0])
            {
                port = dev.port;
                break;
            }
        }
        if (port > 0)
        {
            AD_RPC_SC::get_instance()->call_remote<driver_serviceClient>(
                port,
                "driver_service",
                [&](driver_serviceClient &client) {
                    std::string ret;
                    client.save_ply_file(ret);
                    out << ret << std::endl;
                });
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static std::unique_ptr<cli::Menu> make_runtime_menu()
{
    std::unique_ptr<cli::Menu> sm_menu(new cli::Menu("runtime"));
    sm_menu->Insert(CLI_MENU_ITEM(restart), "启动");
    sm_menu->Insert(CLI_MENU_ITEM(list), "列出启动进程");
    sm_menu->Insert(CLI_MENU_ITEM(mock), "设备模拟", {"设备名称", "动作", "动作效果"});
    sm_menu->Insert(CLI_MENU_ITEM(enable_log), "设置日志", {"模块", "日志级别"});
    sm_menu->Insert(CLI_MENU_ITEM(disable_log), "取消日志", {"模块", "日志级别"});
    sm_menu->Insert(CLI_MENU_ITEM(open_log), "打开日志");
    sm_menu->Insert(CLI_MENU_ITEM(close_log), "关闭日志");
    sm_menu->Insert(CLI_MENU_ITEM(save_ply), "保存点云", {"设备名"});
    sm_menu->Insert(CLI_MENU_ITEM(reset_sm), "重置状态机");
    return sm_menu;
}

RUNTIME_CONFIG_CLI::RUNTIME_CONFIG_CLI() : common_cli(make_runtime_menu(), "runtime")
{
}

std::string RUNTIME_CONFIG_CLI::make_bdr()
{
    std::string ret;
    for (auto &itr : g_log_config)
    {
        ret += "enable_log " + itr.log_module + " " + itr.log_level + "\n";
    }
    ret += "restart\n";
    return ret;
}

void RUNTIME_CONFIG_CLI::clear()
{
    g_log_config.clear();
    g_log_open = false;
}
