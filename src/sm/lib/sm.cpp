#include "sm.h"
#include <yaml-cpp/yaml.h>

SM_STATE_PTR SM_STATE_FACTORY::create_sm_state(const std::string &_name)
{
    SM_STATE_PTR ret;
    auto states = m_config["states"];
    for (const auto &itr : states)
    {
        auto state_name = itr["name"].as<std::string>("");
        if (state_name == _name)
        {

            auto enter_func = itr["enter"].as<std::string>();
            auto exit_func = itr["exit"].as<std::string>();
            auto do_func = itr["do"]["action"].as<std::string>();
            auto next_state = itr["do"]["next"].as<std::string>("");
            auto tmp_state = new SM_STATE(state_name, enter_func, exit_func, do_func, next_state, shared_from_this());
            ret.reset(tmp_state);
            auto events = itr["events"];
            for (const auto &ev_itr : events)
            {
                ret->add_event(ev_itr["trigger"].as<std::string>(), ev_itr["next"].as<std::string>());
            }
            break;
        }
    }

    return ret;
}

void SM_STATE::before_enter(DYNAMIC_SM &_sm)
{
    m_logger.log(AD_LOGGER::DEBUG, "state %s before enter", m_state_name.c_str());
    call_lua_func(_sm.get_lua_state(), m_before_enter_lua_func);
}

void SM_STATE::after_exit(DYNAMIC_SM &_sm)
{
    m_logger.log(AD_LOGGER::DEBUG, "state %s after exit", m_state_name.c_str());
    call_lua_func(_sm.get_lua_state(), m_after_exit_lua_func);
}

SM_STATE_PTR SM_STATE::do_action(DYNAMIC_SM &_sm)
{
    m_logger.log(AD_LOGGER::DEBUG, "state %s do action", m_state_name.c_str());
    call_lua_func(_sm.get_lua_state(), m_do_lua_func);
    SM_STATE_PTR ret;

    if (m_next_state.length() > 0)
    {
        ret.reset(m_sm_state_factory->create_sm_state(m_next_state).release());
    }
    return ret;
}