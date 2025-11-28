#include "common_driver.h"
#include "../../rpc/gen_code/cpp/runner_sm.h"

common_driver::~common_driver()
{
    if (m_relay_timer)
    {
        AD_RPC_SC::get_instance()->stopTimer(m_relay_timer);
    }
}

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
}

void common_driver::sim_vehicle_max_volume(const double volume)
{
    m_rd_result.max_volume = volume;
}

void common_driver::sim_vehicle_cur_volume(const double volume)
{
    m_rd_result.cur_volume = volume;
}

void common_driver::sim_vehicle_height(const double height)
{
    m_rd_result.height = height;
}

void common_driver::sim_vehicle_stuff(const bool is_full)
{
    m_rd_result.is_full = is_full;
}

bool common_driver::vehicle_passed_gate()
{
    return m_gate_is_close;
}

void common_driver::set_lc_open(const int32_t thredhold)
{
    execute_to_threshold(thredhold);
}

int32_t common_driver::get_lc_open()
{
    return m_lc_open_threshold;
}

double common_driver::get_scale_weight()
{
    return m_current_weight;
}

void common_driver::open_close_lc_cmd(bool _is_open, bool _is_on)
{
    m_logger.log("收到开关LC命令: %s, %s", _is_open ? "开" : "关", _is_on ? "执行" : "停止");
}

void common_driver::start_relay_timer()
{
    if (!m_relay_timer)
    {
        m_relay_timer = AD_RPC_SC::get_instance()->startTimer(
            1,
            [this]() {
                if (m_relay_state == HOLD_OPEN)
                {
                    m_state_stay_position++;
                }
                else if (m_relay_state == HOLD_CLOSE)
                {
                    m_state_stay_position--;
                }
                if ((m_relay_state == HOLD_OPEN) && (m_state_stay_position >= m_expect_position) ||
                    (m_relay_state == HOLD_CLOSE) && (m_state_stay_position <= m_expect_position))
                {
                    relay_do_action(STOP);
                }
                m_lc_open_threshold = m_state_stay_position * get_position_coe();
                m_logger.log("当前阈值：%d", get_lc_open());
                ad_rpc_update_current_state();
            });
    }
}


void common_driver::relay_do_action(lc_relay_state_t _req_state)
{
    auto orig_state = m_relay_state;
    if (orig_state != _req_state)
    {
        if (_req_state != STOP)
        {
            relay_do_action(STOP);
        }
        switch (_req_state)
        {
        case STOP:
            open_close_lc_cmd(false, false);
            open_close_lc_cmd(true, false);
            break;
        case HOLD_OPEN:
            open_close_lc_cmd(false, false);
            open_close_lc_cmd(true, true);
            break;
        case HOLD_CLOSE:
            open_close_lc_cmd(true, false);
            open_close_lc_cmd(false, true);
            break;
        default:
            break;
        }
        m_relay_state = _req_state;
    }
}

void common_driver::execute_to_threshold(int32_t threshold)
{
    m_logger.log("指定执行到阈值: %d", threshold );
    auto threshold_coe = get_position_coe();
    m_expect_position = threshold / threshold_coe;
    auto position_diff = m_expect_position - m_state_stay_position;
    if (position_diff > 0)
    {
        relay_do_action(HOLD_OPEN);
    }
    else if (position_diff < 0)
    {
        relay_do_action(HOLD_CLOSE);
    }
}

int common_driver::get_position_coe() const
{
    return 20;
}
