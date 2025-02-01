#if !defined(_AD_UTILS_H_)
#define _AD_UTILS_H_
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdarg>
#include <map>

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
    AD_LOGGER(const std::string &filepath = "", const std::string &module_name = "None") : log_file(nullptr), m_module_name(module_name)
    {
        if (!filepath.empty())
        {
            log_file = new std::ofstream(filepath);
            if (!log_file->is_open())
            {
                std::cerr << "Failed to open log file: " << filepath << std::endl;
                delete log_file;
                log_file = nullptr;
            }
        }
    }

    // 析构函数
    ~AD_LOGGER()
    {
        if (log_file)
        {
            log_file->close();
            delete log_file;
        }
    }

    // 设置全局日志等级
    static void set_log_level(LogLevel level)
    {
        global_log_level = level;
    }

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

        std::string log_message = "[" + ad_utils_date_time().m_datetime_ms + "] [" + level_to_string(level) + "] [" + m_module_name+ "] " + buffer;

        if (log_file)
        {
            *log_file << log_message << std::endl;
        }
        else
        {
            std::cout << log_message << std::endl;
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

private:
    std::string m_module_name;
    std::ofstream *log_file;
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

#endif // _AD_UTILS_H_
