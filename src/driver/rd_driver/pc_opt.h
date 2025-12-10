#if !defined(_PC_OPT_H_)
#define _PC_OPT_H_

#include "../lib/common_driver.h"
#include "../lib/modbus_driver.h"

bool should_stop_walk();

void process_one_plyfile(const std::string &file_name);
class RS_DRIVER : public common_driver
{
    void *common_rs_driver_ptr = nullptr;
    std::unique_ptr<modbus_driver> m_modbus_driver;
public:
    RS_DRIVER(const std::string &_config_file);
    virtual ~RS_DRIVER();
    virtual bool running_status_check() override;
    void start(const std::string &_file = "", int _interval_sec = 0);

    virtual void voice_broadcast(const std::string &voice_content, const int32_t times)
    {
        return;
    }
    virtual void voice_stop()
    {
        return;
    }
    virtual void led_display(const std::string &led_content)
    {
        return;
    }
    virtual void led_stop()
    {
        return;
    }
    virtual void gate_control(const bool is_close)
    {
        return;
    }

    virtual void save_ply_file(std::string &_return, const std::string &reason);

};


#endif // _PC_OPT_H_