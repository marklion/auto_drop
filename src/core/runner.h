#if !defined(_RUNNER_H_)
#define _RUNNER_H_
#include "../sm/lib/sm.h"
#include "../public/event_sc/ad_event_sc.h"

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

public:
    std::shared_ptr<AD_EVENT_SC> m_event_sc = std::make_shared<AD_EVENT_SC>();
    AD_LOGGER m_logger = AD_LOGGER("", "runner");
    std::vector<std::string> m_event_list = {"reset", "vehicle_arrived"};
    RUNNER_VARIBLES<std::string> cur_order_number = RUNNER_VARIBLES<std::string>("", *this);
    RUNNER_VARIBLES<double> init_p_weight = RUNNER_VARIBLES<double>(0.0, *this, [](double &var)
                                                                    { var = 0.0; });
    RUNNER_VARIBLES<AD_EVENT_SC_TIMER_NODE_PTR> m_2s_timer =
        RUNNER_VARIBLES<AD_EVENT_SC_TIMER_NODE_PTR>(
            nullptr,
            *this,
            m_clear_timer_func);
    RUNNER_VARIBLES<AD_EVENT_SC_TIMER_NODE_PTR> m_5s_timer =
        RUNNER_VARIBLES<AD_EVENT_SC_TIMER_NODE_PTR>(
            nullptr,
            *this,
            m_clear_timer_func);
    void clear_all_variables()
    {
        m_logger.log("clear all variables");
        clear_all();
    }
    void start_2s_timer()
    {
        m_logger.log("start 2s timer");
        m_2s_timer = m_event_sc->startTimer(2, [this]()
                                            { proc_event("nothing"); });
    }
    void record_order_number(const std::string &_order_number)
    {
        m_logger.log("record order number: %s", _order_number.c_str());
        cur_order_number = _order_number;
    }
    void stop_2s_timer()
    {
        m_logger.log("stop 2s timer");
        m_2s_timer.clear();
    }
    void start_5s_timer()
    {
        m_logger.log("start 5s timer");
        m_5s_timer = m_event_sc->startTimer(5, [this]()
                                            { proc_event("reset"); });
    }
    void stop_5s_timer()
    {
        m_logger.log("stop 5s timer");
        m_5s_timer.clear();
    }
    void broadcast_driver_in()
    {
        m_logger.log("broadcast driver in");
    }
    void clear_broadcast()
    {
        m_logger.log("clear broadcast");
    }

    void run_event_loop()
    {
        m_event_sc->runEventLoop();
    }

    static std::shared_ptr<RUNNER> runner_init(const YAML::Node &_sm_config);
    virtual ~RUNNER() {}
};

#endif // _RUNNER_H_
