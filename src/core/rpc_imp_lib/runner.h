#if !defined(_RUNNER_H_)
#define _RUNNER_H_
#include "../../sm/lib/sm.h"
#include "../../public/event_sc/ad_event_sc.h"
#include "../../rpc/ad_rpc.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../public/const_var_define.h"
#include "rpc_wrapper.h"

typedef std::function<void()> CLEAR_FUNC;
class RV_FATHER
{
public:
    std::vector<CLEAR_FUNC> m_clear_funcs;
    void clear_all()
    {
        for (auto &func : m_clear_funcs)
        {
            func();
        }
    }
};

template <typename T>
class RUNNER_VARIBLES
{
public:
    std::function<void(T &)> m_clear_func;
    T m_var;
    RUNNER_VARIBLES(T _var, RV_FATHER &_father, std::function<void(T &)> _clear_func = [](T &var)
                                                { var.clear(); }) : m_var(_var), m_clear_func(_clear_func)
    {
        CLEAR_FUNC clear_func = [this]()
        { this->clear(); };
        _father.m_clear_funcs.push_back(clear_func);
    }
    virtual ~RUNNER_VARIBLES() {}
    T &operator()()
    {
        return m_var;
    }
    void operator=(const T &_var)
    {
        m_var = _var;
    }
    void clear()
    {
        m_clear_func(m_var);
    }
};

class RUNNER : public DYNAMIC_SM, public RV_FATHER
{
    using DYNAMIC_SM::DYNAMIC_SM;
    std::function<void(AD_EVENT_SC_TIMER_NODE_PTR &)> m_clear_timer_func = [this](AD_EVENT_SC_TIMER_NODE_PTR &timer)
    {
        if (timer)
        {
            m_event_sc->stopTimer(timer);
            timer.reset();
        }
    };
    std::map<std::string, uint16_t> m_device_map;
    std::string test_var;
public:
    virtual void register_lua_function_virt(lua_State *_L) override;
    void set_device(const std::string &_device_name, const uint16_t _device_id)
    {
        m_device_map[_device_name] = _device_id;
    }
    uint16_t get_device(const std::string &_device_name)
    {
        uint16_t ret = 0;
        if (m_device_map.find(_device_name) != m_device_map.end())
        {
            ret = m_device_map[_device_name];
            if (ret == 0)
            {
                m_device_map.erase(_device_name);
            }
        }

        return ret;
    }
    std::shared_ptr<AD_EVENT_SC> m_event_sc = std::make_shared<AD_EVENT_SC>();
    AD_LOGGER m_logger = AD_LOGGER("", "runner");
    void broadcast_driver_in()
    {
        m_logger.log("broadcast driver in");
        rpc_wrapper_call_device(
            get_device(AD_CONST_DEVICE_LED),
            [&](driver_serviceClient &client){
                client.voice_broadcast("please enter");
            });
    }
    void clear_broadcast()
    {
        m_logger.log("clear broadcast");
    }

    AD_EVENT_SC_TIMER_NODE_PTR start_timer(int _sec, luabridge::LuaRef _func)
    {
        AD_EVENT_SC_TIMER_NODE_PTR ret;
        if (_func.isFunction())
        {
            ret = m_event_sc->startTimer(_sec, [_func](){
                _func();
            });
        }
        return ret;
    }
    void stop_timer(AD_EVENT_SC_TIMER_NODE_PTR _timer) {
        if (_timer) {
            m_event_sc->stopTimer(_timer);
        }
    }

    void set_test_var(const std::string &_var)
    {
        test_var = _var;
    }
    std::string get_test_var()
    {
        return test_var;
    }

    static std::shared_ptr<RUNNER> runner_init(const YAML::Node &_sm_config);
    virtual ~RUNNER()
    {
        m_logger.log("runner destructed");
    }
};

#endif // _RUNNER_H_
