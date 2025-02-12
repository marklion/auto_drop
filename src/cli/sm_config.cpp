#include "sm_config.h"
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
static void set_next(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    check_ret += common_cli::check_params(_params, 1, "下一个状态");
    if (check_ret.empty())
    {
        auto orig_node = common_cli::read_config_file();
        auto sm_state_config = get_state_from_sm(orig_node["sm"], _params[0]);
        auto next_state = get_state_from_sm(orig_node["sm"], _params[1]);
        if (sm_state_config.IsNull() || next_state.IsNull())
        {
            out << "状态" << _params[0] << "或" << _params[1] << "不存在" << std::endl;
        }
        else
        {
            sm_state_config["do"]["next"] = _params[1];
            common_cli::write_config_file(orig_node);
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}
static void set_init(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    if (check_ret.empty())
    {
        auto orig_node = common_cli::read_config_file();
        auto sm_state_config = get_state_from_sm(orig_node["sm"], _params[0]);
        if (sm_state_config.IsNull())
        {
            out << "状态" << _params[0] << "不存在" << std::endl;
        }
        else
        {
            orig_node["sm"]["init_state"] = _params[0];
            common_cli::write_config_file(orig_node);
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
}
static void add_event(std::ostream &out, std::vector<std::string> _params)
{
    auto check_ret = common_cli::check_params(_params, 0, "状态名称");
    check_ret += common_cli::check_params(_params, 1, "事件名称");
    check_ret += common_cli::check_params(_params, 2, "目标状态");
    if (check_ret.empty())
    {
        auto orig_node = common_cli::read_config_file();
        auto sm_state_config = get_state_from_sm(orig_node["sm"], _params[0]);
        auto target_state = get_state_from_sm(orig_node["sm"], _params[2]);
        if (sm_state_config.IsNull() || target_state.IsNull())
        {
            out << "状态" << _params[0] << "或" << _params[2] << "不存在" << std::endl;
        }
        else
        {
            auto events = sm_state_config["events"];
            for (const auto &itr : events)
            {
                auto event_name = itr["trigger"].as<std::string>("");
                if (event_name == _params[1])
                {
                    out << "事件" << _params[1] << "已存在" << std::endl;
                    return;
                }
            }
            YAML::Node new_event;
            new_event["trigger"] = _params[1];
            new_event["next"] = _params[2];
            events.push_back(new_event);
            common_cli::write_config_file(orig_node);
        }
    }
    else
    {
        out << check_ret << std::endl;
    }
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
        auto lua_code = _params[2];
        size_t pos = 0;

        while ((pos = lua_code.find("<>", pos)) != std::string::npos)
        {
            lua_code.replace(pos, 2, "\n");
            pos += 2; // 跳过替换后的字符
        }
        auto ret = change_state_action(_params[0], _params[1], lua_code);
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
    sm_menu->Insert(CLI_MENU_ITEM(set_next), "设定下一个状态", {"状态名称", "下一个状态"});
    sm_menu->Insert(CLI_MENU_ITEM(add_event), "添加事件", {"状态名称", "事件名称", "目标状态"});
    sm_menu->Insert(CLI_MENU_ITEM(set_init), "指定初始状态", {"状态名称"});
    return sm_menu;
}

SM_CONFIG_CLI::SM_CONFIG_CLI() : common_cli(make_sm_menue(), "state_machine")
{
}

std::string replaceNewlinesAndQuotes(const std::string &str)
{
    std::string result = str;
    size_t pos = 0;

    while ((pos = result.find('\n', pos)) != std::string::npos)
    {
        result.replace(pos, 1, "<>");
        pos += 2; // 跳过替换后的字符
    }
    pos = 0;

    while ((pos = result.find('"', pos)) != std::string::npos)
    {
        result.replace(pos, 1, "\\\"");
        pos += 2; // 跳过替换后的字符
    }

    return result;
}
std::string SM_CONFIG_CLI::make_bdr()
{
    std::string ret;
    auto node = common_cli::read_config_file();
    auto init_state = node["sm"]["init_state"].as<std::string>("");
    std::string trans_config;
    for (const auto &itr : node["sm"]["states"])
    {
        auto state_name = itr["name"].as<std::string>("");
        if (!state_name.empty())
        {
            ret += "create_state " + state_name + "\n";
            auto enter_lua_code = itr["enter"].as<std::string>("");
            auto exit_lua_code = itr["exit"].as<std::string>("");
            auto do_lua_code = itr["do"]["action"].as<std::string>("");
            if (!enter_lua_code.empty())
            {
                ret += "input_action " + state_name + " enter \"" + replaceNewlinesAndQuotes(enter_lua_code) + "\"\n";
            }
            if (!exit_lua_code.empty())
            {
                ret += "input_action " + state_name + " exit \"" + replaceNewlinesAndQuotes(exit_lua_code) + "\"\n";
            }
            if (!do_lua_code.empty())
            {
                ret += "input_action " + state_name + " do \"" + replaceNewlinesAndQuotes(do_lua_code) + "\"\n";
            }
            if (itr["do"]["next"].as<std::string>("") != "")
            {
                trans_config += "set_next " + state_name + " " + itr["do"]["next"].as<std::string>("") + "\n";
            }
            auto events = itr["events"];
            for (const auto &event : events)
            {
                trans_config += "add_event " + state_name + " " + event["trigger"].as<std::string>("") + " " + event["next"].as<std::string>("") + "\n";
            }
        }
    }
    ret += trans_config;
    if (!init_state.empty())
    {
        ret += "set_init " + init_state + "\n";
    }
    return ret;
}
