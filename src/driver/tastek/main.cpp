#include "../../rpc/ad_rpc.h"
#include "../lib/common_driver.h"
#include <modbus/modbus.h>
class TASTEK_DRIVER : public common_driver
{
    std::string m_host_name;
    modbus_t *m_ctx = nullptr;

    void set_coil(int coil_addr, bool on)
    {
        int rc = modbus_write_bit(m_ctx, coil_addr, on);
        if (rc == -1)
        {
            m_logger.log(AD_LOGGER::ERROR, "modbus_write_bit failed: %s", modbus_strerror(errno));
            modbus_close(m_ctx);
            modbus_free(m_ctx);
            m_ctx = nullptr;
        }
        else
        {
            m_logger.log(AD_LOGGER::INFO, "Set coil to %d", on);
        }
    }

    virtual void open_close_lc_cmd(bool _is_open, bool _is_on) override
    {
        set_coil(_is_open ? 0 : 1, _is_on);
    }

public:
    TASTEK_DRIVER(const std::string &host_name) : common_driver("tastek_driver"), m_host_name(host_name)
    {
        m_ctx = modbus_new_tcp(m_host_name.c_str(), 502);
        if (m_ctx == nullptr)
        {
            m_logger.log(AD_LOGGER::ERROR, "modbus_new_tcp failed");
        }
        else if (modbus_connect(m_ctx) == -1)
        {
            m_logger.log(AD_LOGGER::ERROR, "modbus_connect failed: %s", modbus_strerror(errno));
            modbus_free(m_ctx);
            m_ctx = nullptr;
        }
    }
    virtual bool running_status_check()
    {
        return m_ctx != nullptr;
    }
    virtual ~TASTEK_DRIVER()
    {
        if (m_ctx)
        {
            modbus_close(m_ctx);
            modbus_free(m_ctx);
            m_ctx = nullptr;
        }
    }
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

int main(int argc, char const *argv[])
{
    if (argc > 2)
    {
        std::string host_name = argv[2];
        auto driver = std::make_shared<TASTEK_DRIVER>(host_name);
        driver->start_driver_daemon(argc, argv);
    }

    return 0;
}
