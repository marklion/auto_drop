#include "lib/common_driver.h"
#include "../rpc/ad_rpc.h"

class mock_driver : public common_driver
{
    AD_CO_ROUTINE_PTR m_voice_co;
    AD_CO_ROUTINE_PTR m_led_co;
    int m_voice_times = 0;
    std::string m_voice_content;
    std::string m_led_content;
public:
    mock_driver() : common_driver("mock_driver")
    {
        m_voice_co = AD_RPC_SC::get_instance()->add_co(
            [&]()
            {
                while (true)
                {
                    if (m_voice_content.length() > 0 && (m_voice_times > 0 || m_voice_times == -1))
                    {
                        m_logger.log("播报语音: %s", m_voice_content.c_str());
                        m_voice_times--;
                        if (m_voice_times == 0)
                        {
                            m_voice_content.clear();
                        }
                        else if (m_voice_times < 0)
                        {
                            m_voice_times = -1;
                        }
                    }
                    AD_RPC_SC::get_instance()->yield_by_timer(1);
                }
            });
        m_led_co = AD_RPC_SC::get_instance()->add_co(
            [&]()
            {
                while (true)
                {
                    if (m_led_content.length() > 0)
                    {
                        m_logger.log("屏幕显示: %s", m_led_content.c_str());
                    }
                    AD_RPC_SC::get_instance()->yield_by_timer(1);
                }
            });
    }

    virtual void voice_broadcast(const std::string &voice_content, const int32_t times)
    {
        m_voice_content = voice_content;
        m_voice_times = times;
    }

    virtual void led_display(const std::string &led_content)
    {
        m_led_content = led_content;
    }

    virtual void gate_control(const bool is_close)
    {
        m_logger.log("门开始动作: %s", is_close ? "关" : "开");
        AD_RPC_SC::get_instance()->yield_by_timer(2);
        m_logger.log("门动作完成: %s", is_close ? "关" : "开");
        sim_gate_status(is_close);
    }
    virtual void led_stop()
    {
        m_led_content.clear();
    }
    virtual void voice_stop()
    {
        m_voice_content.clear();
        m_voice_times = 0;
    }
    virtual bool running_status_check()
    {
        return true;
    }
};

int main(int argc, char const *argv[])
{
    auto md = std::make_shared<mock_driver>();
    return md->start_driver_daemon(argc, argv);
}
