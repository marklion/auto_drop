#if !defined(_SM_H_)
#define _SM_H_
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <yaml-cpp/yaml.h>
#include "../../public/utils/ad_utils.h"

class DYNAMIC_SM;
class SM_STATE;
class SM_STATE_FACTORY;
typedef std::function<void(DYNAMIC_SM &)> ACTION_FUNC;
typedef std::vector<ACTION_FUNC> ACTION_FUNC_LIST;
typedef std::unique_ptr<SM_STATE> SM_STATE_PTR;
typedef std::shared_ptr<SM_STATE_FACTORY> SM_STATE_FACTORY_PTR;
typedef std::shared_ptr<DYNAMIC_SM> DYNAMIC_SM_PTR;

class SM_STATE_FACTORY : public std::enable_shared_from_this<SM_STATE_FACTORY>
{
    YAML::Node m_config;
    std::map<std::string, ACTION_FUNC> m_action_map;

public:
    std::string m_init_state;
    SM_STATE_FACTORY(const YAML::Node &config)
    {
        m_config = YAML::Clone(config);
        m_init_state = m_config["init_state"].as<std::string>();
    }
    void register_actions(const std::string &_action_name, ACTION_FUNC _func)
    {
        m_action_map[_action_name] = _func;
    }
    SM_STATE_PTR create_sm_state(const std::string &_name);
    ~SM_STATE_FACTORY()
    {
    }
};
class SM_STATE
{
public:
    const std::string m_state_name;
    ACTION_FUNC_LIST m_enter_funcs;
    ACTION_FUNC_LIST m_exit_funcs;
    ACTION_FUNC_LIST m_do_funcs;
    std::string m_next_state;
    SM_STATE_FACTORY_PTR m_sm_state_factory;
    AD_LOGGER m_logger;
    std::map<std::string, std::string> m_event_state_map;
    SM_STATE(
        const std::string &state_name,
        ACTION_FUNC_LIST enter_funcs,
        ACTION_FUNC_LIST exit_funcs,
        ACTION_FUNC_LIST do_funcs,
        const std::string &next_state,
        SM_STATE_FACTORY_PTR sm_state_factory) : m_enter_funcs(enter_funcs),
                                                 m_exit_funcs(exit_funcs),
                                                 m_do_funcs(do_funcs),
                                                 m_state_name(state_name),
                                                 m_next_state(next_state),
                                                 m_sm_state_factory(sm_state_factory),
                                                 m_logger("", "SM_STATE " + state_name)
    {
    }
    void add_event(const std::string &event, const std::string &next_state)
    {
        m_event_state_map[event] = next_state;
    }
    void before_enter(DYNAMIC_SM &_sm)
    {
        m_logger.log(AD_LOGGER::DEBUG, "state %s before enter", m_state_name.c_str());
        for (auto &func : m_enter_funcs)
        {
            func(_sm);
        }
    }
    void after_exit(DYNAMIC_SM &_sm)
    {
        m_logger.log(AD_LOGGER::DEBUG, "state %s after exit", m_state_name.c_str());
        for (auto &func : m_exit_funcs)
        {
            func(_sm);
        }
    }
    SM_STATE_PTR do_action(DYNAMIC_SM &_sm)
    {
        m_logger.log(AD_LOGGER::DEBUG, "state %s do action", m_state_name.c_str());
        SM_STATE_PTR ret;
        for (auto &func : m_do_funcs)
        {
            func(_sm);
        }
        if (m_next_state.length() > 0)
        {
            ret.reset(m_sm_state_factory->create_sm_state(m_next_state).release());
        }
        return ret;
    }
    SM_STATE_PTR get_event_state(const std::string &_event)
    {
        SM_STATE_PTR ret;
        auto it = m_event_state_map.find(_event);
        if (it != m_event_state_map.end())
        {
            ret.reset(m_sm_state_factory->create_sm_state(it->second).release());
        }
        return ret;
    }
    virtual ~SM_STATE() {}
};

class DYNAMIC_SM
{
    AD_LOGGER m_logger = AD_LOGGER("", "sm");
public:
    SM_STATE_PTR m_current_state;

    DYNAMIC_SM(const std::string &_init_state, SM_STATE_FACTORY_PTR _sm_state_factory)
    {
        m_current_state.reset(_sm_state_factory->create_sm_state(_init_state).release());
    }
    void change_state(SM_STATE_PTR &_next_state)
    {
        m_logger.log(AD_LOGGER::DEBUG, "change state from %s to %s", m_current_state->m_state_name.c_str(), _next_state->m_state_name.c_str());
        m_current_state->after_exit(*this);
        m_current_state = std::move(_next_state);
        m_current_state->before_enter(*this);
    }
    void begin() {
        m_current_state->before_enter(*this);
        trigger();
    }
    void trigger()
    {
        auto next = m_current_state->do_action(*this);
        if (next)
        {
            change_state(next);
            trigger();
        }
    }
    void proc_event(const std::string &_event)
    {
        m_logger.log(AD_LOGGER::DEBUG, "proc event %s", _event.c_str());
        auto next = m_current_state->get_event_state(_event);
        if (next)
        {
            change_state(next);
            trigger();
        }
    }
    virtual ~DYNAMIC_SM() {}
};

#endif // _SM_H_
