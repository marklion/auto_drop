#include "ad_redis.h"
#include "../hiredis/hiredis.h"
#include "../const_var_define.h"
extern "C"
{
#include "../hiredis/net.h"
}
static ssize_t my_redis_read(struct redisContext *rc, char *buf, size_t buf_size)
{
    auto event_sc = (AD_EVENT_SC *)(rc->m_event_sc);
    event_sc->yield_by_fd(rc->fd);
    return redisNetRead(rc, buf, buf_size);
}
redisContext *AD_REDIS_HELPER::prepare_redis()
{
    redisContext *ret = nullptr;
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 700000; // 500 ms
    auto rc = redisConnectWithTimeout(m_host.c_str(), m_port, tv);
    if (rc)
    {
        if (!rc->err)
        {
            rc->m_event_sc = m_event_sc.get();
            rc->funcs->read = my_redis_read;
            auto auth_reply = (redisReply *)redisCommand(rc, "AUTH %s", m_password.c_str());
            if (auth_reply)
            {
                if (auth_reply->type != REDIS_REPLY_ERROR)
                {
                    ret = rc;
                }
                else
                {
                    m_logger.log(AD_LOGGER::ERROR, "Redis auth error: %s\n", auth_reply->str);
                }
                freeReplyObject(auth_reply);
            }
            else
            {
                m_logger.log(AD_LOGGER::ERROR, "Connection error: %s\n", rc->errstr);
            }
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Connection error: %s\n", rc->errstr);
        }
        if (!ret)
        {
            redisFree(rc);
        }
    }
    return ret;
}
void AD_REDIS_HELPER::set(const std::string &key, const std::string &value)
{
    auto rc = prepare_redis();
    if (rc)
    {
        auto self_serial = getenv(AD_CONST_SELF_SERIAL);
        std::string ser_key = "appliance:";
        ser_key += self_serial;
        const char *cmd_argv[] = {
            "HSET",
            ser_key.c_str(),
            key.c_str(),
            value.c_str(),
        };
        auto cmd_reply = (redisReply *)redisCommandArgv(rc, sizeof(cmd_argv) / sizeof(char *), cmd_argv, nullptr);
        if (cmd_reply)
        {
            if (cmd_reply->type == REDIS_REPLY_ERROR)
            {
                m_logger.log(AD_LOGGER::ERROR, "Redis set error: %s\n", cmd_reply->str);
            }
            freeReplyObject(cmd_reply);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Redis set error: %s\n", rc->errstr);
        }

        redisFree(rc);
    }
}

std::string AD_REDIS_HELPER::get(const std::string &key)
{
    std::string ret;

    auto rc = prepare_redis();
    if (rc)
    {
        auto self_serial = getenv(AD_CONST_SELF_SERIAL);
        std::string ser_key = "appliance:";
        ser_key += self_serial;
        const char *cmd_argv[] = {
            "HGET",
            ser_key.c_str(),
            key.c_str(),
        };
        auto cmd_reply = (redisReply *)redisCommandArgv(rc, sizeof(cmd_argv) / sizeof(char *), cmd_argv, nullptr);
        if (cmd_reply)
        {
            if (cmd_reply->type == REDIS_REPLY_STRING)
            {
                ret = cmd_reply->str;
            }
            else if (cmd_reply->type == REDIS_REPLY_ERROR)
            {
                m_logger.log(AD_LOGGER::ERROR, "Redis get error: %s\n", cmd_reply->str);
            }
            freeReplyObject(cmd_reply);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Redis get error: %s\n", rc->errstr);
        }

        redisFree(rc);
    }

    return ret;
}

void AD_REDIS_HELPER::pub(const std::string &channel, const std::string &msg)
{
    auto rc = prepare_redis();
    if (rc)
    {
        auto self_serial = getenv(AD_CONST_SELF_SERIAL);
        auto real_channel = channel + self_serial;
        auto cmd_reply = (redisReply *)redisCommand(rc, "PUBLISH %s %s", real_channel.c_str(), msg.c_str());
        if (cmd_reply)
        {
            if (cmd_reply->type == REDIS_REPLY_ERROR)
            {
                m_logger.log(AD_LOGGER::ERROR, "Redis pub error: %s\n", cmd_reply->str);
            }
            freeReplyObject(cmd_reply);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Redis pub error: %s\n", rc->errstr);
        }

        redisFree(rc);
    }
}

AD_REDIS_EVENT_NODE::AD_REDIS_EVENT_NODE(YAML::Node &config, AD_EVENT_SC_PTR _sc) : m_logger("REDIS SC"), m_sc(_sc)
{
    AD_REDIS_HELPER tmp_helper(config, _sc);
    auto rc = tmp_helper.prepare_redis();
    if (rc)
    {
        m_redis = rc;
        m_redis->funcs->read = redisNetRead;
    }
}

AD_REDIS_EVENT_NODE::~AD_REDIS_EVENT_NODE()
{
    if (m_redis)
    {
        redisFree(m_redis);
    }
}

int AD_REDIS_EVENT_NODE::getFd() const
{
    int ret = -1;

    if (m_redis)
    {
        ret = m_redis->fd;
    }

    return ret;
}

void AD_REDIS_EVENT_NODE::handleEvent()
{
    redisReply *reply = nullptr;
    if (redisGetReply(m_redis, (void **)&reply) == REDIS_OK)
    {
        if (reply->type == REDIS_REPLY_ARRAY)
        {
            if (reply->elements == 3)
            {
                if (reply->element[0]->type == REDIS_REPLY_STRING)
                {
                    auto channel = reply->element[1]->str;
                    auto callback = m_subscribed_callbacks.find(channel);
                    if (callback != m_subscribed_callbacks.end())
                    {
                        callback->second(reply->element[2]->str);
                    }
                }
            }
        }
        freeReplyObject(reply);
    }
    else if (m_redis->err == REDIS_ERR_EOF)
    {
        m_sc->unregisterNode(shared_from_this());
        auto node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
        auto ret = std::make_shared<AD_REDIS_EVENT_NODE>(node, m_sc);
        for (auto &pair : m_subscribed_callbacks)
        {
            ret->register_subscribed_callback(pair.first, pair.second);
        }
        m_sc->registerNode(ret);
    }
}

void AD_REDIS_EVENT_NODE::register_subscribed_callback(const std::string &channel, REDIS_SUBSCRIBED_CALLBACK callback)
{
    auto reply = (redisReply *)redisCommand(m_redis, "SUBSCRIBE %s", channel.c_str());
    freeReplyObject(reply);
    m_subscribed_callbacks[channel] = callback;
}
