#if !defined(_COMMON_DRIVER_H_)
#define _COMMON_DRIVER_H_
#include "../../rpc/gen_code/cpp/idl_types.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../public/utils/ad_utils.h"
#include "../../public/const_var_define.h"
#include "../../rpc/ad_rpc.h"

class common_driver : public driver_serviceIf, public std::enable_shared_from_this<common_driver>
{
    std::string m_latest_plate;
    vehicle_rd_detect_result m_rd_result;
    bool m_gate_is_close = false;
    double m_current_weight = 0;
    int32_t m_lc_open_threshold = 0;
    int threshold_coe = 20;
    enum lc_relay_state_t
    {
        STOP,
        HOLD_OPEN,
        HOLD_CLOSE,
    } m_relay_state = STOP;
    int m_state_stay_position = 0;
    AD_EVENT_SC_TIMER_NODE_PTR m_relay_timer;
    int m_expect_position = 0;
protected:
    AD_LOGGER m_logger;
    u16 m_sm_port = 0;

public:
    common_driver(const std::string &_driver_name) : m_logger(_driver_name)
    {
    }
    virtual ~common_driver();
    virtual bool set_sm(const u16 sm_port) override final;
    virtual void stop_device() override final;
    int start_driver_daemon(int argc, char const *argv[]);
    void emit_event(const std::string &_event);
    virtual bool running_status_check() = 0;
    virtual void save_ply_file(std::string &_return, const std::string &reason)
    {
        _return = "mocked_ply_file.ply";
    }

    virtual void get_trigger_vehicle_plate(std::string &_return) override final;
    virtual void vehicle_rd_detect(vehicle_rd_detect_result &_return) override final;
    virtual void sim_vehicle_came(const std::string &plate) override final;
    virtual void sim_gate_status(const bool is_close) override final;
    virtual void sim_scale_weight(const double weight) override final;
    virtual void sim_vehicle_position(const vehicle_position_detect_state::type state) override final;
    virtual void sim_vehicle_max_volume(const double volume) override final;
    virtual void sim_vehicle_cur_volume(const double volume) override final;
    virtual void sim_vehicle_height(const double height) override final;
    virtual void sim_vehicle_stuff(const bool is_full) override final;
    virtual bool vehicle_passed_gate() override final;
    virtual void set_lc_open(const int32_t thredhold);
    virtual int32_t get_lc_open() override final;
    virtual double get_scale_weight() override final;
    virtual void open_close_lc_cmd(bool _is_open, bool _is_on);
    void start_relay_timer();
    void relay_do_action(lc_relay_state_t _req_state);
    void execute_to_threshold(int32_t threshold);
    virtual int get_position_coe() const;
};

#endif // _COMMON_DRIVER_H_
