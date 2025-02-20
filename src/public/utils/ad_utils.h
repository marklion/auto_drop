#if !defined(_AD_UTILS_H_)
#define _AD_UTILS_H_
#include <cstdarg>    // 用于 va_list, va_start, va_end
#include <cstdio>     // 用于 vsnprintf
#include <sstream>    // 用于 std::ostringstream
#include <iomanip>    // 用于 std::setw, std::setfill
#include <iostream>   // 用于 std::cout
#include <fstream>
#include <string>
#include <ctime>
#include <cstdarg>
#include <map>
#include <vector>
#include "SimpleIni.h"

std::vector<std::string> ad_utils_split_string(const std::string &str, const std::string &delimiter);
struct ad_utils_date_time
{
    std::string m_date;
    std::string m_time;
    std::string m_datetime_ms;
    std::string m_datetime;
    ad_utils_date_time(time_t now = time(nullptr))
    {
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
        // 获取毫秒
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        char ms_buf[10];
        snprintf(ms_buf, sizeof(ms_buf), ":%03ld", ts.tv_nsec / 1000000);

        m_datetime_ms = std::string(buf) + ms_buf;
        m_datetime = m_datetime_ms.substr(0, 19);
        m_date = m_datetime.substr(0, 10);
        m_time = m_datetime.substr(11, 8);
    }
};
class AD_LOGGER
{
public:
    enum LogLevel
    {
        DEBUG = 0,
        INFO = 1,
        ERROR = 2
    };

    // 构造函数，默认输出到标准输出
    AD_LOGGER(const std::string &module_name = "None") : m_module_name(module_name)
    {

    }

    // 析构函数
    ~AD_LOGGER()
    {
    }

    static void set_global_log_level(LogLevel level)
    {
        global_log_level = level;
    }

    void output_log(const std::string &_content);

    // 日志记录函数
    void log(LogLevel level, const std::string &format, ...)
    {
        if (level < global_log_level)
        {
            return;
        }

        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
        va_end(args);
        auto multi_line = ad_utils_split_string(buffer, "\n");
        for (const auto &line : multi_line)
        {
            std::string log_message = "[" + ad_utils_date_time().m_datetime_ms + "] [" + level_to_string(level) + "] [" + m_module_name + "] " + line;
            output_log(log_message);
        }
    }

    // 重载log函数，默认日志等级为INFO
    void log(const std::string &format, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format.c_str(), args);
        va_end(args);

        log(INFO, buffer);
    }
    void log_packet(LogLevel level, const uint8_t *packet, size_t len)
    {
        if (level < global_log_level)
        {
            return;
        }

        std::ostringstream oss;
        oss << "[" << ad_utils_date_time().m_datetime_ms << "] [" << level_to_string(level) << "] [" << m_module_name << "] Packet: ";
        for (size_t i = 0; i < len; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packet[i]) << " ";
        }

        std::string log_message = oss.str();

        output_log(log_message);
    }

private:
    std::string m_module_name;
    static LogLevel global_log_level;
    // 将日志等级转换为字符串
    std::string level_to_string(LogLevel level)
    {
        static std::map<LogLevel, std::string> level_map = {
            {DEBUG, "DEBUG"},
            {INFO, "INFO"},
            {ERROR, "ERROR"}};
        return level_map[level];
    }
};

class AD_INI_CONFIG{
    std::string m_file_path;
    CSimpleIniA m_ini;
public:
    AD_INI_CONFIG(const std::string &file_path):m_file_path(file_path){
        m_ini.SetUnicode();
        m_ini.LoadFile(m_file_path.c_str());
    }
    std::string get_config(const std::string &_section, const std::string &_key, const std::string &_default = ""){
        return m_ini.GetValue(_section.c_str(), _key.c_str(), _default.c_str());
    }
};

#endif // _AD_UTILS_H_
