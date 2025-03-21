#if !defined(_CONST_VAR_DEFINE_H_)
#define _CONST_VAR_DEFINE_H_
#include <string>

#define AD_CONST_CONFIG_LISTEN_PORT 49911
#define AD_CONST_DAEMON_MIN_PORT 50110
#define AD_CONST_DAEMON_MAX_PORT 53000

#define AD_CONST_SM_EVENT_RESET "reset"

#define AD_CONST_CONFIG_FILE "/conf/config.yaml"
#define AD_CONST_LOGFILE_DIR "/var/ad_log"
#define AD_CONST_LOGFILE_PATH AD_CONST_LOGFILE_DIR"/ad_log.log"
#define AD_CONST_SAVED_CONFIG_FILE "/database/init.txt"
#define AD_CONST_LUA_SAMPLE_FILE "/conf/lua_sample.lua"

inline std::string AD_REDIS_CHANNEL_SAVE_PLY(const std::string &x) { return "save_ply_" + x; }
#define AD_CONST_REDIS_PLY_PATH "current_ply"

#endif // _CONST_VAR_DEFINE_H_
