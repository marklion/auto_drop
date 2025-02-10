#include "sm_config.h"

static YAML::Node make_new_state(const std::string &_name)
{
    YAML::Node new_state;
    new_state["name"] = _name;
    new_state["enter"] = "";
    new_state["exit"] = "";
    new_state["do"]["action"] = "";
    new_state["do"]["next"] = "";
    new_state["events"] = YAML::Load("[]");
    return new_state;
}
static std::string edit_by_vim(const std::string &_orig_code)
{
    std::string ret;
    std::string tmp_file = "/tmp/sm_config_edit_XXXXXX";
    std::ofstream fout(tmp_file);
    fout << _orig_code;
    fout.close();
    system(("vim " + tmp_file).c_str());
    auto pfile = popen(("cat " + tmp_file).c_str(), "r");
    if (pfile)
    {
        char buffer[1024] = {0};
        while (fgets(buffer, sizeof(buffer), pfile))
        {
            ret += buffer;
        }
        pclose(pfile);
    }
    return ret;
}
static YAML::Node get_state_from_sm(YAML::Node _sm, const std::string &_state_name)
{
    auto states = _sm["states"];
    for (const auto &itr : states)
    {
        auto state_name = itr["name"].as<std::string>("");
        if (state_name == _state_name)
        {
            return itr;
        }
    }
    return YAML::Node();
}
static std::string get_current_action(const std::string &_state_name, const std::string &_action_type)
{
    std::string ret;
    auto orig_node = common_cli::read_config_file();
    auto sm_state_config = get_state_from_sm(orig_node["sm"], _state_name);
    if (!sm_state_config.IsNull())
    {
        ret = sm_state_config[_action_type].as<std::string>("");
    }
    return ret;
}
static std::string change_state_action(const std::string &_state_name, const std::string &_action_type, const std::string &_lua_code)
{
    std::string ret;
    auto orig_node = common_cli::read_config_file();
    auto sm_state_config = get_state_from_sm(orig_node["sm"], _state_name);
    if (sm_state_config.IsNull())
    {
        ret += "状态" + _state_name + "不存在\n";
    }
    else
    {
        bool is_action_type = false;
        std::string action_types[] = {"enter", "exit", "do"};
        for (auto &itr : action_types)
        {
            if (_action_type == itr)
            {
                is_action_type = true;
                break;
            }
        }
        if (!is_action_type)
        {
            ret += "动作类型" + _action_type + "无效\n";
        }
        else
        {
            if (_action_type == "do")
            {
                sm_state_config["do"]["action"] = _lua_code;
            }
            else
            {
                sm_state_config[_action_type] = _lua_code;
            }
            common_cli::write_config_file(orig_node);
        }
    }
    return ret;
}
static void input_action(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    check_ret += common_cli::check_params(_params, 1, "动作类型");
    check_ret += common_cli::check_params(_params, 2, "Lua代码");
    if (check_ret.empty())
    {
        auto ret = change_state_action(_params[0], _params[1], _params[2]);
        out << ret << std::endl;
    }
    else
    {
        out << check_ret << std::endl;
    }
}
static void edit_action(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    check_ret += common_cli::check_params(_params, 1, "动作类型");
    if (check_ret.empty())
    {
        auto lua_code = edit_by_vim(get_current_action(_params[0], _params[1]));
        auto ret = change_state_action(_params[0], _params[1], lua_code);
        out << ret << std::endl;
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static void create_state(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    if (check_ret.empty())
    {
        auto orig_node = common_cli::read_config_file();
        auto sm_config = orig_node["sm"];
        auto states = sm_config["states"];
        for (const auto &itr : states)
        {
            auto state_name = itr["name"].as<std::string>("");
            if (state_name == _params[0])
            {
                out << "状态" << _params[0] << "已存在" << std::endl;
                return;
            }
        }
        auto new_state = make_new_state(_params[0]);
        states.push_back(new_state);
        common_cli::write_config_file(orig_node);
    }
    else
    {
        out << check_ret << std::endl;
    }
}

static std::unique_ptr<cli::Menu> make_sm_menue()
{
    std::unique_ptr<cli::Menu> sm_menu(new cli::Menu("state_machine"));
    sm_menu->Insert(CLI_MENU_ITEM(create_state), "创建状态", {"状态名称"});
    sm_menu->Insert(CLI_MENU_ITEM(edit_action), "编辑动作", {"状态名称", "动作类型"});
    sm_menu->Insert(CLI_MENU_ITEM(input_action), "输入动作", {"状态名称", "动作类型", "Lua代码"});
    return sm_menu;
}

SM_CONFIG_CLI::SM_CONFIG_CLI() : common_cli(make_sm_menue(), "state_machine")
{
}

std::string SM_CONFIG_CLI::make_bdr()
{
    std::string ret;
    auto node = common_cli::read_config_file();
    for (const auto &itr : node["sm"]["states"])
    {
        auto state_name = itr["name"].as<std::string>("");
        if (!state_name.empty())
        {
            ret += "create_state " + state_name + "\n";
            if (!itr["enter"].as<std::string>("").empty())
            {
                ret += "input_action " + state_name + " enter \"" + itr["enter"].as<std::string>("") + "\"";
            }
            if (!itr["exit"].as<std::string>("").empty())
            {
                ret += "input_action " + state_name + " exit \"" + itr["exit"].as<std::string>("") + "\"";
            }
            if (!itr["do"]["action"].as<std::string>("").empty())
            {
                ret += "input_action " + state_name + " do \"" + itr["do"]["action"].as<std::string>("") + "\"";
            }
        }
    }
    return ret;
}
