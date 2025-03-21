#include "redis_config.h"
#include "../public/event_sc/ad_redis.h"

struct redis_config
{
    std::string host;
    int port;
    std::string password;
};

static void params(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "主机名");
    check_ret += common_cli::check_params(_params, 1, "端口");
    check_ret += common_cli::check_params(_params, 2, "密码");
    if (check_ret.empty())
    {
        auto orig_node = common_cli::read_config_file();
        auto redis_node = orig_node["redis"];
        redis_node["host"] = _params[0];
        redis_node["port"] = std::stoi(_params[1]);
        redis_node["password"] = _params[2];
        common_cli::write_config_file(orig_node);
        AD_RPC_SC::get_instance()->call_remote<config_managementClient>(
            AD_CONST_CONFIG_LISTEN_PORT,
            "config_management",
            [&](config_managementClient &client)
            {
                client.restart_redis_subscriber();
            });
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static std::unique_ptr<redis_config> get_redis_config()
{
    auto orig_node = common_cli::read_config_file();
    auto redis_node = orig_node["redis"];
    std::unique_ptr<redis_config> ret;
    if (redis_node["host"].as<std::string>("") != "")
    {
        ret.reset(new redis_config());
        ret->host = redis_node["host"].as<std::string>("");
        ret->port = redis_node["port"].as<int>(0);
        ret->password = redis_node["password"].as<std::string>("");
    }

    return ret;
}

static void get(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "键");
    if (check_ret.empty())
    {
        auto cur_redis_config = get_redis_config();
        if (cur_redis_config)
        {
            AD_REDIS_HELPER adh(cur_redis_config->host, cur_redis_config->port, cur_redis_config->password, AD_RPC_SC::get_instance());
            auto ret = adh.get(_params[0]);
            out << ret << std::endl;
        }
        else
        {
            out << "redis未配置" << std::endl;
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}
static void set(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "键");
    check_ret += common_cli::check_params(_params, 1, "值");
    if (check_ret.empty())
    {
        auto cur_redis_config = get_redis_config();
        if (cur_redis_config)
        {
            AD_REDIS_HELPER adh(cur_redis_config->host, cur_redis_config->port, cur_redis_config->password, AD_RPC_SC::get_instance());
            adh.set(_params[0], _params[1]);
        }
        else
        {
            out << "redis未配置" << std::endl;
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static std::unique_ptr<cli::Menu> make_redis_menue()
{
    std::unique_ptr<cli::Menu> redis_menu(new cli::Menu("redis"));

    redis_menu->Insert(CLI_MENU_ITEM(params), "参数设定", {"主机名", "端口", "密码"});
    redis_menu->Insert(CLI_MENU_ITEM(get), "获取", {"键"});
    redis_menu->Insert(CLI_MENU_ITEM(set), "设置", {"键", "值"});

    return redis_menu;
}
REDIS_CONFIG_CLI::REDIS_CONFIG_CLI() : common_cli(make_redis_menue(), "redis")
{
}

std::string REDIS_CONFIG_CLI::make_bdr()
{
    std::string ret;
    auto cur_redis_config = get_redis_config();
    if (cur_redis_config)
    {
        ret = "params " + cur_redis_config->host + " " + std::to_string(cur_redis_config->port) + " " + cur_redis_config->password + "\n";
    }

    return ret;
}

void REDIS_CONFIG_CLI::clear()
{
    auto node = common_cli::read_config_file();
    auto redis_node = node["redis"];
    redis_node = YAML::Load("[]");
    common_cli::write_config_file(node);
}
