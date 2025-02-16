#include "runner.h"

static void print_log(const std::string &_log)
{
    AD_LOGGER("LUA").log(AD_LOGGER::INFO, _log);
}

void RUNNER::register_lua_function_virt(lua_State *_L)
{
    luabridge::getGlobalNamespace(_L)
        .deriveClass<RUNNER, DYNAMIC_SM>("RUNNER")
        .addFunction("start_timer", &RUNNER::start_timer)
        .addFunction("stop_timer", &RUNNER::stop_timer)
        .addFunction("dev_voice_broadcast", &RUNNER::dev_voice_broadcast)
        .addFunction("dev_voice_stop", &RUNNER::dev_voice_stop)
        .addFunction("dev_led_display", &RUNNER::dev_led_display)
        .addFunction("dev_led_stop", &RUNNER::dev_led_stop)
        .addFunction("dev_gate_control", &RUNNER::dev_gate_control)
        .addFunction("dev_get_trigger_vehicle_plate", &RUNNER::dev_get_trigger_vehicle_plate)
        .addFunction("dev_vehicle_rd_detect", &RUNNER::dev_vehicle_rd_detect)
        .addFunction("dev_vehicle_passed_gate", &RUNNER::dev_vehicle_passed_gate)
        .endClass()
        .beginClass<AD_EVENT_SC_TIMER_NODE_PTR>("AD_EVENT_SC_TIMER_NODE_PTR")
        .endClass()
        .beginClass<vehicle_rd_detect_result>("vehicle_rd_detect_result")
        .addProperty("state", &vehicle_rd_detect_result::state)
        .addProperty("is_full", &vehicle_rd_detect_result::is_full)
        .endClass()
        .addFunction("print_log_string", &print_log);
    luaL_dostring(
        _L, "function print_log(log)\n"
            "\tprint_log_string(tostring(log))\n"
            "end\n");
}

std::shared_ptr<RUNNER> RUNNER::runner_init(const YAML::Node &_sm_config)
{
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(_sm_config));
    auto ret = std::make_shared<RUNNER>(sm_state_fac->m_init_state, sm_state_fac);

    return ret;
}
