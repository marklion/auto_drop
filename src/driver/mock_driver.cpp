#include "lib/common_driver.h"
#include "../rpc/ad_rpc.h"

class mock_driver : public common_driver
{
public:
    mock_driver() : common_driver("mock_driver")
    {
    }

    virtual void voice_broadcast(const std::string &voice_content)
    {
        m_logger.log("voice broadcast: %s", voice_content.c_str());
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
