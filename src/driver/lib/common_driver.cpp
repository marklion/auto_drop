#include "common_driver.h"
#include "../../rpc/ad_rpc.h"
#include "../../rpc/gen_code/cpp/runner_sm.h"

bool common_driver::set_sm(const u16 sm_port)
{
    m_sm_port = sm_port;
    return true;
}

void common_driver::stop_device()
{
    AD_RPC_SC::get_instance()->stop_server();
}

int common_driver::start_driver_daemon(int argc, char const *argv[])
{
    int ret = 0;
    std::vector<std::string> args(argv, argv + argc);
    u16 listen_port = std::stoul(args[1]);
    auto sc = AD_RPC_SC::get_instance();
    sc->enable_rpc_server(listen_port);
    sc->add_rpc_server("driver_service", std::make_shared<driver_serviceProcessor>(shared_from_this()));
    sc->startTimer(
        10,
        [&]()
        {
            if (!running_status_check())
            {
                sc->stop_server();
                ret = -1;
            }
        });
    sc->start_server();
    return ret;
}

void common_driver::emit_event(const std::string &_event)
{
    AD_RPC_SC::get_instance()->call_remote<runner_smClient>(
        m_sm_port,
        "runner_sm",
        [&](runner_smClient &client)
        {
            client.push_sm_event(_event);
        });
}

void common_driver::get_trigger_vehicle_plate(std::string &_return)
{
    _return = m_latest_plate;
}

void common_driver::vehicle_rd_detect(vehicle_rd_detect_result &_return)
{
    _return = m_rd_result;
}

void common_driver::sim_vehicle_came(const std::string &plate)
{
    m_latest_plate = plate;
    ad_rpc_update_current_state();
}

void common_driver::sim_gate_status(const bool is_close)
{
    m_gate_is_close = is_close;
    ad_rpc_update_current_state();
}

void common_driver::sim_scale_weight(const double weight)
{
    m_current_weight = weight;
    ad_rpc_update_current_state();
}

void common_driver::sim_vehicle_position(const vehicle_position_detect_state::type state)
{
    m_rd_result.state = state;
    ad_rpc_update_current_state();
}

void common_driver::sim_vehicle_max_volume(const double volume)
{
    m_rd_result.max_volume = volume;
    ad_rpc_update_current_state();
}

void common_driver::sim_vehicle_cur_volume(const double volume)
{
    m_rd_result.cur_volume = volume;
    ad_rpc_update_current_state();
}

void common_driver::sim_vehicle_height(const double height)
{
    m_rd_result.height = height;
    ad_rpc_update_current_state();
}

void common_driver::sim_vehicle_stuff(const bool is_full)
{
    m_rd_result.is_full = is_full;
    ad_rpc_update_current_state();
}

bool common_driver::vehicle_passed_gate()
{
    return m_gate_is_close;
}

void common_driver::set_lc_open(const int32_t thredhold)
{
    m_lc_open_threshold = thredhold;
    ad_rpc_update_current_state();
}

int32_t common_driver::get_lc_open()
{
    return m_lc_open_threshold;
}

double common_driver::get_scale_weight()
{
    return m_current_weight;
}
