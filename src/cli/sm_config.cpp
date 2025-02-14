#include "sm_config.h"
#include "../core/rpc_imp_lib/sm_rpc.h"
static std::string make_comment()
{
    return "--[[\n"
            "环境提供的函数说明：\n"
            "sm:start_timer(sec,func),启动一个定时器，sec为秒数，func为定时器到时时执行的函数.返回一个定时器变量\n"
            "sm:stop_timer(timer),停止一个定时器，timer为定时器变量\n"
            "sm:dev_voice_broadcast(device, content, times),播放语音，device为喇叭设备名称，content为语音内容字符串，times为播放次数，-1代表一直播放\n"
            "sm:dev_voice_stop(device),停止播放语音，device为喇叭设备名称\n"
            "sm:dev_led_display(device, content),显示LED内容，device为LED设备名称，content为显示内容字符串\n"
            "sm:dev_led_stop(device),停止显示LED内容，device为LED设备名称\n"
            "sm:dev_gate_control(device, is_close),控制闸机，device为闸机设备名称，is_close为true时关闭闸机，false时打开闸机\n"
            "sm:dev_get_trigger_vehicle_plate(device),获取触发车牌，device为车牌识别设备名称，返回车牌字符串\n"
            "sm:dev_vehicle_rd_detect(device),车辆雷达检测结果检测，device为检测设备名称，返回车辆检测结果,其中:state是车辆位置 可能是0123分别代表开始、中间、结束、无效，:is_full代表是否装满 \n"
            "sm:dev_vehicle_passed_gate(device),车辆通过闸机，device为闸机设备名称, 返回是否通过\n"
            "sm:proc_event(event_name), 触发事件，event_name为事件名称\n"
            "print_log(log),打印日志，log为日志内容\n"
            "举例：\n"
            "sm:dev_voice_broadcast(\"some_device\",\"hello\",1)\n"
            "sm:dev_led_display(\"some_device\",\"hello\")\n"
            "sm:dev_gate_control(\"some_device\",false)\n"
            "some_timer = sm:start_timer(1,function() \n"
            "\tdetect_result = sm:dev_vehicle_rd_detect(\"some_device\") \n"
            "\tif (sm:dev_vehicle_passed_gate(\"some_device\") and detect_result:is_full) then\n"
            "\t\tsm:dev_voice_stop(\"some_device\") \n"
            "\t\tsm:dev_led_stop(\"some_device\") \n"
            "\t\tsm:stop_timer(some_timer) \n"
            "\t\tsm:proc_event(\"some_event\") \n"
            "\tend)\n"
            "end)\n"
            "这段代码表示：\n"
            "让设备some_device播放hello语音一次，显示hello内容，打开闸机\n"
            "打开定时器，每1秒获取一次车辆雷达检测结果和车辆是否通过，\n"
            "如果车辆通过闸机并且车辆装满，则停止播放语音，停止显示内容，停止定时器，触发事件some_event\n"
            "--]]\n";
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
    std::string tmp_file = "/tmp/sm_config_edit_XXXXXX.lua";
    std::ofstream fout(tmp_file);
    auto comment = make_comment();
    fout << comment;
    fout << _orig_code;
    fout.close();
    system(("bash -c 'vim " + tmp_file + "'").c_str());
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
    auto comment_pos = ret.find(make_comment());
    if (comment_pos != std::string::npos)
    {
        ret.erase(comment_pos, make_comment().length());
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
static std::string check_lua_code(const std::string &_code)
{
    std::string ret;
    try
    {
        runner_sm_impl tmp_sm_rpc(nullptr);
        tmp_sm_rpc.check_lua_code(_code);
    }
    catch(const ad_gen_exp& e)
    {
        ret = e.msg;
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
            auto lua_code = _lua_code;
            auto lua_check_ret = check_lua_code(lua_code);
            if (!lua_check_ret.empty())
            {
                ret += lua_check_ret + "\n";
            }
            else
            {
                if (_action_type == "do")
                {
                    sm_state_config["do"]["action"] = lua_code;
                }
                else
                {
                    sm_state_config[_action_type] = lua_code;
                }
                common_cli::write_config_file(orig_node);
            }
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

void SM_CONFIG_CLI::clear()
{
    auto orig_node = common_cli::read_config_file();
    orig_node["sm"]["init_state"] = "";
    orig_node["sm"]["states"] = YAML::Load("[]");
    common_cli::write_config_file(orig_node);
}
