#if !defined(_RUNNER_H_)
#define _RUNNER_H_
#include "../sm/lib/sm.h"

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

public:
    AD_LOGGER m_logger = AD_LOGGER("", "runner");
    std::vector<std::string> m_event_list = {"reset", "vehicle_arrived"};
    RUNNER_VARIBLES<std::string> cur_order_number = RUNNER_VARIBLES<std::string>("", *this);
    RUNNER_VARIBLES<double> init_p_weight = RUNNER_VARIBLES<double>(0.0, *this, [](double &var)
                                                                    { var = 0.0; });
    void clear_all_variables()
    {
        m_logger.log("clear all variables");
        clear_all();
    }
    void record_order_number(const std::string &_order_number)
    {
        m_logger.log("record order number: %s", _order_number.c_str());
        cur_order_number = _order_number;
    }
    void broadcast_driver_in()
    {
        m_logger.log("broadcast driver in");
    }
    void clear_broadcast()
    {
        m_logger.log("clear broadcast");
    }

    static std::shared_ptr<RUNNER> runner_init(const YAML::Node &_sm_config);
    virtual ~RUNNER() {}
};

#endif // _RUNNER_H_
