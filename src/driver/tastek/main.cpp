#include "../../rpc/ad_rpc.h"
#include "../lib/common_driver.h"
#include <modbus/modbus.h>
#include "../../public/utils/CJsonObject.hpp"
class TASTEK_DRIVER : public common_driver
{
    std::string m_host_name;
    long m_full_spend_sec = 5;
    long m_modbus_retry_times = 3;
    bool m_on_addr_array[4] = {false, false, false, false}; // 0-3
    bool m_real_addr_array[4] = {false, false, false, false};
    bool m_error = false;
    long m_cur_retry_times = 0;
    AD_EVENT_SC_TIMER_NODE_PTR m_loop_timer;

    virtual int get_position_coe() const override {
        return 100 / m_full_spend_sec;
    }
    modbus_t *connect_modbus_tcp()
    {
        modbus_t *ret = nullptr;
        ret = modbus_new_tcp(m_host_name.c_str(), 502);
        if (ret)
        {
            modbus_set_response_timeout(ret, 0, 50000);
            modbus_set_byte_timeout(ret, 0, 50000);
            if (modbus_connect(ret) == -1)
            {
                m_logger.log(AD_LOGGER::ERROR, "modbus_connect failed: %s", modbus_strerror(errno));
                modbus_free(ret);
                ret = nullptr;
            }
            modbus_set_slave(ret, 1);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "modbus_new_tcp failed:%s", modbus_strerror(errno));
        }

        return ret;
    }
    void destroy_modbus_tcp(modbus_t *ctx)
    {
        if (ctx)
        {
            modbus_close(ctx);
            modbus_free(ctx);
            ctx = nullptr;
        }
    }
    void challenge_error(bool _success)
    {
        if (!_success)
        {
            m_cur_retry_times++;
            if (m_cur_retry_times > m_modbus_retry_times)
            {
                m_error = true;
                m_logger.log(AD_LOGGER::ERROR, "modbus操作失败次数过多，进入错误状态");
            }
        }
        else
        {
            m_cur_retry_times = 0;
        }
    }
    void set_coil(int coil_addr, bool on)
    {
        bool success = false;
        auto ctx = connect_modbus_tcp();
        if (ctx)
        {
            int rc = modbus_write_bit(ctx, coil_addr, on);
            if (rc == -1)
            {
                m_logger.log(AD_LOGGER::ERROR, "modbus_write_bit %d->%d failed: %s", coil_addr, on, modbus_strerror(errno));
            }
            else
            {
                m_logger.log(AD_LOGGER::INFO, "Set %d coil to %d", coil_addr, on);
                success = true;
            }
            destroy_modbus_tcp(ctx);
        }
        challenge_error(success);
    }

    virtual void open_close_lc_cmd(bool _is_open, bool _is_on) override
    {
        if (_is_open)
        {
            m_on_addr_array[0] = _is_on;
        }
        else
        {
            m_on_addr_array[1] = _is_on;
        }
    }

    void lc_get_coil()
    {
        bool success = false;
        auto ctx = connect_modbus_tcp();
        if (ctx)
        {
            uint8_t ret_buf[4] = {0};
            auto read_len = modbus_read_bits(ctx, 0, 4, ret_buf);
            if (read_len == 4)
            {
                m_logger.log(AD_LOGGER::INFO, "Get coil status: %d,%d,%d,%d", ret_buf[0], ret_buf[1], ret_buf[2], ret_buf[3]);
                for (int i = 0; i < 4; i++)
                {
                    m_real_addr_array[i] = ret_buf[i] != 0;
                }
                success = true;
            }
            else
            {
                m_logger.log(AD_LOGGER::ERROR, "modbus_read_bits failed: %s", modbus_strerror(errno));
            }
            destroy_modbus_tcp(ctx);
        }
        challenge_error(success);
    }

public:
    TASTEK_DRIVER(
        const std::string &host_name,
        long _retry_times,
        long _full_spend) : common_driver("tastek_driver"),
                            m_host_name(host_name),
                            m_full_spend_sec(_full_spend),
                            m_modbus_retry_times(_retry_times)
    {
    }
    virtual ~TASTEK_DRIVER()
    {
        if (m_loop_timer)
        {
            AD_RPC_SC::get_instance()->stopTimer(m_loop_timer);
            m_loop_timer.reset();
        }
    }
    virtual bool running_status_check()
    {
        return m_error != true;
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
    void run_modbus_loop()
    {
        if (!m_loop_timer)
        {
            m_loop_timer = AD_RPC_SC::get_instance()->startTimer(
                0, 300, [this]() {
                lc_get_coil();
                for (int i = 0; i < 4; i++)
                {
                    if (m_real_addr_array[i] != m_on_addr_array[i])
                    {
                        set_coil(i, m_on_addr_array[i]);
                    }
                }
            });
        }
    }
};

int main(int argc, char const *argv[])
{
    int ret = -1;
    if (argc > 2)
    {
        neb::CJsonObject input_config;
        std::string host_name;
        long retry_times = 3;
        long full_spend = 5;
        if (input_config.Parse(argv[2]))
        {
            input_config.Get("host", host_name);
            input_config.Get("retry", retry_times);
            input_config.Get("spend", full_spend);
        }
        auto driver = std::make_shared<TASTEK_DRIVER>(host_name, retry_times, full_spend);
        driver->start_relay_timer();
        driver->run_modbus_loop();
        ret = driver->start_driver_daemon(argc, argv);
    }

    return ret;
}
