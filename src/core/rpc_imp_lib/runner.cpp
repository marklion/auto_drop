#include "runner.h"

void RUNNER::register_lua_function_virt(lua_State *_L)
{
    luabridge::getGlobalNamespace(_L)
        .deriveClass<RUNNER, DYNAMIC_SM>("RUNNER")
        .addFunction("set_test_var", &RUNNER::set_test_var)
        .addFunction("get_test_var", &RUNNER::get_test_var)
        .addFunction("start_timer", &RUNNER::start_timer)
        .addFunction("stop_timer", &RUNNER::stop_timer)
        .endClass()
        .beginClass<AD_EVENT_SC_TIMER_NODE_PTR>("AD_EVENT_SC_TIMER_NODE_PTR")
        .endClass();

}

std::shared_ptr<RUNNER> RUNNER::runner_init(const YAML::Node &_sm_config)
{
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(_sm_config));
    auto ret = std::make_shared<RUNNER>(sm_state_fac->m_init_state, sm_state_fac);
    ret->begin<RUNNER>();

    return ret;
}
