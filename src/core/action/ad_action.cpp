#include "ad_action.h"
#include "../../public/const_var_define.h"
#include "../../rpc/ad_rpc.h"
#include "../../public/utils/CJsonObject.hpp"
struct subcribe_map
{
    std::function<std::string(const std::string &)> key_func;
    std::function<std::string(const std::string &)> process_func;
};

static subcribe_map g_sb_map[] = {
    {AD_REDIS_CHANNEL_SAVE_PLY, ad_rpc_device_save_ply},
    {AD_REDIS_CHANNEL_CURRENT_STATE, [](const std::string &)
     { return ad_rpc_get_current_state(); }},
    {AD_REDIS_CHANNEL_GATE_CTRL,
     [](const std::string &_msg)
     {
         auto json_obj = neb::CJsonObject(_msg);
         auto dev_name = json_obj("dev_name");
         auto is_open = json_obj("is_open");
         ad_rpc_gate_ctrl(dev_name, is_open == "true");
         return "";
     }},
};

AD_REDIS_EVENT_NODE_PTR create_redis_event_node()
{
    auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
    auto ret = std::make_shared<AD_REDIS_EVENT_NODE>(node, AD_RPC_SC::get_instance());

    for (auto &pair : g_sb_map)
    {
        ret->register_subscribed_callback(
            pair.key_func(getenv(AD_CONST_SELF_SERIAL)),
            [&](const std::string &_msg)
            {
                auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
                AD_REDIS_HELPER helper(node, AD_RPC_SC::get_instance());
                helper.set(pair.key_func(""), pair.process_func(_msg));
            });
    }

    return ret;
}