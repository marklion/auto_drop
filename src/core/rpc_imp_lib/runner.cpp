#include "runner.h"

std::shared_ptr<RUNNER> RUNNER::runner_init(const YAML::Node &_sm_config)
{
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(_sm_config));
    sm_state_fac->register_actions(
        "clear_all_variables",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.clear_all_variables();
        });
    sm_state_fac->register_actions(
        "record_order_number",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.record_order_number();
        });
    sm_state_fac->register_actions(
        "broadcast_driver_in",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.broadcast_driver_in();
        });
    sm_state_fac->register_actions(
        "clear_broadcast",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.clear_broadcast();
        });
    sm_state_fac->register_actions(
        "start_2s_timer",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.start_2s_timer();
        });
    sm_state_fac->register_actions(
        "stop_2s_timer",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.stop_2s_timer();
        });
    sm_state_fac->register_actions(
        "start_5s_timer",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.start_5s_timer();
        });
    sm_state_fac->register_actions(
        "stop_5s_timer",
        [](DYNAMIC_SM &_sm)
        {
            auto &runner_sm = dynamic_cast<RUNNER &>(_sm);
            runner_sm.stop_5s_timer();
        });
    auto ret = std::make_shared<RUNNER>(sm_state_fac->m_init_state, sm_state_fac);
    ret->begin();

    return ret;
}
