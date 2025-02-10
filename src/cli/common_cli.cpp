#include "common_cli.h"
#include <fstream>

YAML::Node common_cli::read_config_file()
{
    YAML::Node node;
    std::ifstream fin(AD_CONST_CONFIG_FILE);
    if (fin.good())
    {
        node = YAML::LoadFile(AD_CONST_CONFIG_FILE);
    }
    return node;
}

void common_cli::write_config_file(const YAML::Node &node)
{
    std::ofstream fout(AD_CONST_CONFIG_FILE);
    fout << node;
    fout.close();
}

std::string common_cli::check_params(const std::vector<std::string> &_params, uint32_t _index, const std::string &_prompt)
{
    std::string ret;
    if (_params.size() <= _index)
    {
        ret = "第" + std::to_string(_index + 1) + "个参数无效，要求传入" + _prompt;
    }

    return ret;
}
