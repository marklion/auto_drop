#if !defined(_SM_H_)
#define _SM_H_
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <yaml-cpp/yaml.h>
#include <lua5.3/lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include "../../public/utils/ad_utils.h"

class DYNAMIC_SM;
class SM_STATE;
class SM_STATE_FACTORY;
typedef std::unique_ptr<SM_STATE> SM_STATE_PTR;
typedef std::shared_ptr<SM_STATE_FACTORY> SM_STATE_FACTORY_PTR;
typedef std::shared_ptr<DYNAMIC_SM> DYNAMIC_SM_PTR;

class SM_STATE_FACTORY : public std::enable_shared_from_this<SM_STATE_FACTORY>
{
    YAML::Node m_config;

public:
    std::string m_init_state;
    SM_STATE_FACTORY(const YAML::Node &config)
    {
        m_config = YAML::Clone(config);
        m_init_state = m_config["init_state"].as<std::string>();
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
    std::string m_before_enter_lua_func;
    std::string m_after_exit_lua_func;
    std::string m_do_lua_func;
    std::string m_next_state;
    SM_STATE_FACTORY_PTR m_sm_state_factory;
    AD_LOGGER m_logger;
    std::map<std::string, std::string> m_event_state_map;
    SM_STATE(
        const std::string &state_name,
        const std::string &before_enter_lua_func,
        const std::string &after_exit_lua_func,
        const std::string &do_lua_func,
        const std::string &next_state,
        SM_STATE_FACTORY_PTR sm_state_factory) : m_before_enter_lua_func(before_enter_lua_func),
                                                 m_after_exit_lua_func(after_exit_lua_func),
                                                 m_do_lua_func(do_lua_func),
                                                 m_state_name(state_name),
                                                 m_next_state(next_state),
                                                 m_sm_state_factory(sm_state_factory),
                                                 m_logger("", "SM_STATE " + state_name)
    {
    }
    void call_lua_func(lua_State *_L, const std::string &_func)
    {
        if (!_func.empty())
        {
            auto lua_ret = luaL_dostring(_L, _func.c_str());
            if (lua_ret != LUA_OK)
            {
                m_logger.log(AD_LOGGER::ERROR, "lua error: %s", lua_tostring(_L, -1));
            }
        }
    }
    void add_event(const std::string &event, const std::string &next_state)
    {
        m_event_state_map[event] = next_state;
    }
    void before_enter(DYNAMIC_SM &_sm);
    void after_exit(DYNAMIC_SM &_sm);
    SM_STATE_PTR do_action(DYNAMIC_SM &_sm);
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
    lua_State *m_L = luaL_newstate();

public:
    SM_STATE_PTR m_current_state;

    DYNAMIC_SM(const std::string &_init_state, SM_STATE_FACTORY_PTR _sm_state_factory)
    {
        m_current_state.reset(_sm_state_factory->create_sm_state(_init_state).release());
    }
    lua_State *get_lua_state()
    {
        return m_L;
    }
    virtual void register_lua_function_virt(lua_State *_L) = 0;
    void register_lua_function()
    {
        luabridge::getGlobalNamespace(m_L)
            .beginClass<DYNAMIC_SM>("DYNAMIC_SM")
            .addFunction("proc_event", &DYNAMIC_SM::proc_event)
            .endClass();
        register_lua_function_virt(m_L);
    }
    void change_state(SM_STATE_PTR &_next_state)
    {
        m_logger.log(AD_LOGGER::DEBUG, "change state from %s to %s", m_current_state->m_state_name.c_str(), _next_state->m_state_name.c_str());
        m_current_state->after_exit(*this);
        m_current_state = std::move(_next_state);
        m_current_state->before_enter(*this);
    }
    template <typename T>
    void begin()
    {
        luaL_openlibs(m_L);
        register_lua_function();
        luabridge::setGlobal(m_L, (T *)this, "sm");
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
    virtual ~DYNAMIC_SM()
    {
        lua_close(m_L);
    }
};

#endif // _SM_H_
