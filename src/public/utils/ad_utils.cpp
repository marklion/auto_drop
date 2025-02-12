#include "ad_utils.h"
#include "../const_var_define.h"
#include <fstream>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
// 初始化全局日志等级
AD_LOGGER::LogLevel AD_LOGGER::global_log_level = AD_LOGGER::DEBUG;
void create_directories(const std::string& path) {
    std::vector<std::string> dirs;
    std::stringstream ss(path);
    std::string dir;

    // 分割路径中的各级目录
    while (std::getline(ss, dir, '/')) {
        dirs.push_back(dir);
    }

    std::string current_path;
    for (const auto& d : dirs) {
        if (!d.empty()) {
            current_path += "/" + d;
            // 创建目录
            mkdir(current_path.c_str(), 0755);
        }
    }
}
void AD_LOGGER::output_log(const std::string &_content)
{
    auto today_date = ad_utils_date_time().m_date;
    std::string log_file = AD_CONST_LOGFILE_DIR "/log_" + today_date + ".log";
    std::string symlink_path = AD_CONST_LOGFILE_PATH;
    std::ifstream ifs(log_file);
    if (!ifs.good())
    {
        create_directories(log_file.substr(0, log_file.find_last_of('/')));
        std::ofstream new_log_file(log_file);
        new_log_file << "log file created at " << today_date << std::endl;
        new_log_file.close();
        symlink(log_file.c_str(), symlink_path.c_str());
    }
    std::ofstream ofs(symlink_path, std::ios::app);
    ofs << _content << std::endl;
}
