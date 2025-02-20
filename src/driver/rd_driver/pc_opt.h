#if !defined(_PC_OPT_H_)
#define _PC_OPT_H_

#include "../lib/common_driver.h"

class RS_DRIVER : public common_driver
{
public:
    RS_DRIVER(const std::string &_config_file);
    virtual bool running_status_check() override;
    void start();

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
};

AD_INI_CONFIG *get_ini_config();
void save_detect_result(const vehicle_rd_detect_result &result);
vehicle_rd_detect_result get_detect_result();

#endif // _PC_OPT_H_
