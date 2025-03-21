#if !defined(_COMMON_DRIVER_H_)
#define _COMMON_DRIVER_H_
#include "../../rpc/gen_code/cpp/idl_types.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../public/utils/ad_utils.h"
#include "../../public/const_var_define.h"

class common_driver : public driver_serviceIf, public std::enable_shared_from_this<common_driver>
{
    std::string m_latest_plate;
    vehicle_rd_detect_result m_rd_result;
    bool m_gate_is_close = false;
    double m_current_weight = 0;
protected:
    AD_LOGGER m_logger;
    u16 m_sm_port = 0;
public:
    common_driver(const std::string &_driver_name) : m_logger( _driver_name)
    {
    }
    virtual bool set_sm(const u16 sm_port) override final;
    virtual void stop_device() override final;
    int start_driver_daemon(int argc, char const *argv[]);
    void emit_event(const std::string &_event);
    virtual bool running_status_check() = 0;
    virtual void save_ply_file(std::string &_return) {
        _return = "mocked_ply_file.ply";
    }

    virtual void get_trigger_vehicle_plate(std::string &_return) override final;
    virtual void vehicle_rd_detect(vehicle_rd_detect_result &_return) override final;
    virtual void sim_vehicle_came(const std::string &plate) override final;
    virtual void sim_gate_status(const bool is_close) override final;
    virtual void sim_scale_weight(const double weight) override final;
    virtual void sim_vehicle_position(const vehicle_position_detect_state::type state) override final;
    virtual void sim_vehicle_stuff(const bool is_full) override final;
    virtual bool vehicle_passed_gate() override final;
};

#endif // _COMMON_DRIVER_H_
