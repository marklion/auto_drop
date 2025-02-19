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
    AD_LOGGER m_logger = AD_LOGGER("runner");
    void dev_voice_broadcast(const std::string &_dev_name, const std::string &_content, int times)
    {
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.voice_broadcast(_content, times);
            });
    }
    void dev_voice_stop(const std::string &_dev_name)
    {
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.voice_stop();
            });
    }
    void dev_led_display(const std::string &_dev_name, const std::string &_content)
    {
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.led_display(_content);
            });
    }
    void dev_led_stop(const std::string &_dev_name)
    {
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.led_stop();
            });
    }
    void dev_gate_control(const std::string &_dev_name, bool is_close)
    {
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.gate_control(is_close);
            });
    }
    std::string dev_get_trigger_vehicle_plate(const std::string &_dev_name)
    {
        std::string ret;
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.get_trigger_vehicle_plate(ret);
            });
        return ret;
    }
    vehicle_rd_detect_result dev_vehicle_rd_detect(const std::string &_dev_name)
    {
        vehicle_rd_detect_result ret;
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                client.vehicle_rd_detect(ret);
            });
        return ret;
    }
    bool dev_vehicle_passed_gate(const std::string &_dev_name)
    {
        bool ret = false;
        rpc_wrapper_call_device(
            get_device(_dev_name),
            [&](driver_serviceClient &client){
                ret = client.vehicle_passed_gate();
            });
        return ret;
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
    void trigger_sm_by_event(const std::string &_event)
    {
        AD_RPC_SC::get_instance()->add_co(
            [this, _event]()
            { proc_event(_event); });
    }
    void sleep_wait(int _sec, int _micro_sec);

    luabridge::LuaRef call_http_api(const std::string &_url, const std::string &_method, luabridge::LuaRef _body, luabridge::LuaRef _header);

    static std::shared_ptr<RUNNER> runner_init(const YAML::Node &_sm_config);
    virtual ~RUNNER()
    {
        m_logger.log("runner destructed");
    }
};

#endif // _RUNNER_H_
