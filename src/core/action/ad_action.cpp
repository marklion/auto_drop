#include "ad_action.h"
#include "../../public/const_var_define.h"
#include "../../rpc/ad_rpc.h"

AD_REDIS_EVENT_NODE_PTR create_redis_event_node()
{
    auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
    auto ret = std::make_shared<AD_REDIS_EVENT_NODE>(node, AD_RPC_SC::get_instance());

    auto self_serial = getenv("SELF_SERIAL");
    std::string channel = AD_REDIS_CHANNEL_SAVE_PLY(self_serial);
    ret->register_subscribed_callback(
        channel,
        [](const std::string &msg)
        {
            auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
            AD_REDIS_HELPER helper(node, AD_RPC_SC::get_instance());
            helper.set(AD_CONST_REDIS_PLY_PATH, ad_rpc_device_save_ply(msg));
        });

    return ret;
}