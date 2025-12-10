#include "runner.h"
#include "../../public/sion.h"
#include "../../public/event_sc/ad_redis.h"
#include <yaml-cpp/yaml.h>

static void print_log(const std::string &_log)
{
    AD_LOGGER("LUA").log(AD_LOGGER::INFO, _log);
}

void RUNNER::register_lua_function_virt(lua_State *_L)
{
    luabridge::getGlobalNamespace(_L)
        .beginClass<RUNNER>("RUNNER")
        .addFunction("start_timer", &RUNNER::start_timer)
        .addFunction("start_timer_ms", &RUNNER::start_timer_ms)
        .addFunction("stop_timer", &RUNNER::stop_timer)
        .addFunction("sleep_wait", &RUNNER::sleep_wait)
        .addFunction("dev_voice_broadcast", &RUNNER::dev_voice_broadcast)
        .addFunction("dev_voice_stop", &RUNNER::dev_voice_stop)
        .addFunction("dev_led_display", &RUNNER::dev_led_display)
        .addFunction("dev_led_stop", &RUNNER::dev_led_stop)
        .addFunction("dev_gate_control", &RUNNER::dev_gate_control)
        .addFunction("dev_get_trigger_vehicle_plate", &RUNNER::dev_get_trigger_vehicle_plate)
        .addFunction("dev_vehicle_rd_detect", &RUNNER::dev_vehicle_rd_detect)
        .addFunction("dev_vehicle_passed_gate", &RUNNER::dev_vehicle_passed_gate)
        .addFunction("dev_save_ply", &RUNNER::dev_save_ply)
        .addFunction("dev_set_lc_open", &RUNNER::dev_set_lc_open)
        .addFunction("call_http_api", &RUNNER::call_http_api)
        .addFunction("trigger_event", &RUNNER::trigger_sm_by_event)
        .addFunction("refresh_current_state", &RUNNER::refresh_current_state)
        .endClass()
        .beginClass<AD_EVENT_SC_TIMER_NODE_PTR>("AD_EVENT_SC_TIMER_NODE_PTR")
        .endClass()
        .beginClass<runner_rd_detect_result>("vehicle_rd_detect_result")
        .addProperty("state", &runner_rd_detect_result::get_state)
        .addProperty("is_full", &runner_rd_detect_result::get_is_full)
        .addProperty("full_offset", &runner_rd_detect_result::get_full_offset)
        .addProperty("side_top_z", &runner_rd_detect_result::get_side_top_z)
        .endClass()
        .addFunction("print_log_string", &print_log);
    luaL_dostring(
        _L, "function print_log(log)\n"
            "\tprint_log_string(tostring(log))\n"
            "end\n"
            "cjson = require('cjson')\n");
}

void RUNNER::sleep_wait(int _sec, int _micro_sec)
{
    AD_RPC_SC::get_instance()->yield_by_timer(_sec, _micro_sec);
}

void RUNNER::refresh_current_state()
{
    ad_rpc_update_current_state();
}

luabridge::LuaRef RUNNER::call_http_api(const std::string &_url, const std::string &_method, luabridge::LuaRef _body, luabridge::LuaRef _header)
{
    luabridge::LuaRef ret = luabridge::newTable(get_lua_state());
    auto json_lib = luabridge::getGlobal(get_lua_state(), "cjson");
    auto encode_func = json_lib["encode"];
    auto decode_func = json_lib["decode"];

    std::string body_str = encode_func(_body);
    auto req = sion::Request().SetUrl(_url).SetHttpMethod(_method).SetBody(body_str);
    for (luabridge::Iterator itr(_header); !itr.isNil(); ++itr)
    {
        auto key = itr.key().tostring();
        auto value = itr.value().tostring();
        req.SetHeader(key, value);
    }
    AD_LOGGER tmp_log("API");
    try
    {
        tmp_log.log(AD_LOGGER::INFO, "call_http_api: %s %s %s", _url.c_str(), _method.c_str(), body_str.c_str());
        auto resp = req.Send(2200);
        tmp_log.log(AD_LOGGER::INFO, "api resp: %s", resp.StrBody().c_str());
        ret = decode_func(std::string(resp.StrBody()));
    }
    catch (const std::exception &e)
    {
        tmp_log.log(AD_LOGGER::ERROR, "call_http_api error: %s", e.what());
    }

    return ret;
}

std::shared_ptr<RUNNER> RUNNER::runner_init(const YAML::Node &_sm_config)
{
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(_sm_config));
    auto ret = std::make_shared<RUNNER>(sm_state_fac->m_init_state, sm_state_fac);

    return ret;
}
