#if !defined(_CONFIG_RPC_H_)
#define _CONFIG_RPC_H_

#include "../../rpc/gen_code/cpp/idl_types.h"
#include "../../rpc/gen_code/cpp/config_management.h"
#include <unordered_map>
#include "../../public/utils/ad_utils.h"
#include "../action/ad_action.h"

class SUBPROCESS_EVENT_SC_NODE;
typedef std::shared_ptr<SUBPROCESS_EVENT_SC_NODE> SUBPROCESS_EVENT_SC_NODE_PTR;

class config_management_impl : public config_managementIf
{
    std::unordered_map<u16, device_config> m_device_map;
    std::unordered_map<u16, sm_config> m_sm_map;
    AD_LOGGER m_logger = AD_LOGGER("config_management");
    std::unordered_map<u16, SUBPROCESS_EVENT_SC_NODE_PTR> m_subprocess_map;
    std::pair<u16, SUBPROCESS_EVENT_SC_NODE_PTR> start_daemon(const std::string &_path, const std::vector<std::string> &_argv, const std::string &_name);
    void wait_stop_daemon(const u16 port);
    AD_REDIS_EVENT_NODE_PTR m_redis_node;
public:
    virtual void start_device(device_config &_return, const std::string &driver_name, const std::vector<std::string> &argv, const std::string &device_name) override;
    virtual void stop_device(const u16 port) override;
    virtual void get_device_list(std::vector<device_config> &_return) override;
    virtual void start_sm(sm_config &_return, const std::string &sm_name, const std::vector<std::string> &argv) override;
    virtual void stop_sm(const u16 port) override;
    virtual void get_sm_list(std::vector<sm_config> &_return) override;
    virtual void restart_redis_subscriber() override;
};

#endif // _CONFIG_RPC_H_
