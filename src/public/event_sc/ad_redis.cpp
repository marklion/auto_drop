#include "ad_redis.h"
#include "../hiredis/hiredis.h"
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
    auto rc = redisConnect(m_host.c_str(), m_port);
    if (rc)
    {
        if (!rc->err)
        {
            rc->m_event_sc = m_event_sc.get();
            rc->funcs->read = my_redis_read;
            auto auth_reply = (redisReply *)redisCommand(rc, "AUTH %s", m_password.c_str());
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
        auto self_serial = getenv("SELF_SERIAL");
        std::string ser_key = "appliance:";
        ser_key += self_serial;
        const char *cmd_argv[] = {
            "HSET",
            ser_key.c_str(),
            key.c_str(),
            value.c_str(),
        };
        auto cmd_reply = (redisReply *)redisCommandArgv(rc, sizeof(cmd_argv) / sizeof(char *), cmd_argv, nullptr);
        if (cmd_reply->type == REDIS_REPLY_ERROR)
        {
            m_logger.log(AD_LOGGER::ERROR, "Redis set error: %s\n", cmd_reply->str);
        }
        freeReplyObject(cmd_reply);
        redisFree(rc);
    }
}

std::string AD_REDIS_HELPER::get(const std::string &key)
{
    std::string ret;

    auto rc = prepare_redis();
    if (rc)
    {
        auto self_serial = getenv("SELF_SERIAL");
        std::string ser_key = "appliance:";
        ser_key += self_serial;
        const char *cmd_argv[] = {
            "HGET",
            ser_key.c_str(),
            key.c_str(),
        };
        auto cmd_reply = (redisReply *)redisCommandArgv(rc, sizeof(cmd_argv) / sizeof(char *), cmd_argv, nullptr);
        if (cmd_reply->type == REDIS_REPLY_STRING)
        {
            ret = cmd_reply->str;
        }
        else if (cmd_reply->type == REDIS_REPLY_ERROR)
        {
            m_logger.log(AD_LOGGER::ERROR, "Redis get error: %s\n", cmd_reply->str);
        }
        freeReplyObject(cmd_reply);
        redisFree(rc);
    }

    return ret;
}
