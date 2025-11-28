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
#define AD_CONST_SELF_SERIAL "SELF_SERIAL"

#define AD_CONST_RIDAR_DEVICE_NAME "ridar"
#define AD_CONST_SCALE_DEVICE_NAME "scale"
#define AD_CONST_GATE_DEVICE_NAME "gate"
#define AD_CONST_LED_DEVICE_NAME "led"
#define AD_CONST_LC_DEVICE_NAME "lc_device"
#define AD_CONST_PLATE_NAME "camera"

inline std::string AD_REDIS_CHANNEL_SAVE_PLY(const std::string &x = "") { return "save_ply_" + x; }
inline std::string AD_REDIS_CHANNEL_CURRENT_STATE(const std::string &x = "") { return "current_state_" + x; }
inline std::string AD_REDIS_CHANNEL_GATE_CTRL(const std::string &x = "") { return "gate_ctrl_" + x; }

#define AD_CONST_REDIS_KEY_SCALE "scale_weight"
#define AD_CONST_REDIS_KEY_RD_POSITION "rd_position"
#define AD_CONST_REDIS_KEY_RD_FULL "rd_full"
#define AD_CONST_REDIS_KEY_RD_FULL_OFFSET "rd_full_offset"
#define AD_CONST_REDIS_KEY_LC_OPEN_THRESHOLD "lc_open_threshold"
#define AD_CONST_REDIS_KEY_GATE "gate"
#define AD_CONST_REDIS_KEY_PLATE "plate"

#endif // _CONST_VAR_DEFINE_H_
