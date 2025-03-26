#if !defined(_AD_REDIS_H_)
#define _AD_REDIS_H_
#include "ad_event_sc.h"
#include "../hiredis/hiredis.h"
#include "../utils/ad_utils.h"
#include <yaml-cpp/yaml.h>

typedef std::function<void(const std::string &)> REDIS_SUBSCRIBED_CALLBACK;
class AD_REDIS_EVENT_NODE:public AD_EVENT_SC_NODE{
    redisContext *m_redis = nullptr;
    AD_LOGGER m_logger;
    std::map<std::string, REDIS_SUBSCRIBED_CALLBACK> m_subscribed_callbacks;
    AD_EVENT_SC_PTR m_sc;
public:
    AD_REDIS_EVENT_NODE(YAML::Node &config, AD_EVENT_SC_PTR _sc);
    virtual ~AD_REDIS_EVENT_NODE();
    virtual int getFd() const override;
    virtual void handleEvent() override;
    void register_subscribed_callback(const std::string &channel, REDIS_SUBSCRIBED_CALLBACK callback);

    virtual std::string node_name() const
    {
        return "redis";
    }
};

class AD_REDIS_HELPER
{
    std::string m_host;
    int m_port;
    std::string m_password;
    AD_LOGGER m_logger;
    AD_EVENT_SC_PTR m_event_sc;
public:
    redisContext *prepare_redis();
    AD_REDIS_HELPER(const std::string &host, int port, const std::string &password, AD_EVENT_SC_PTR _sc) : m_logger("REDIS"), m_event_sc(_sc)
    {
        m_host = host;
        m_port = port;
        m_password = password;
    }
    AD_REDIS_HELPER(YAML::Node &config, AD_EVENT_SC_PTR _sc) : m_logger("REDIS"), m_event_sc(_sc)
    {
        m_host = config["redis"]["host"].as<std::string>();
        m_port = config["redis"]["port"].as<int>();
        m_password = config["redis"]["password"].as<std::string>();
    }
    void set(const std::string &key, const std::string &value);
    std::string get(const std::string &key);
    void pub(const std::string &channel, const std::string &msg);
};

#endif // _AD_REDIS_H_
