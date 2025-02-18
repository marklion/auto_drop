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
void WriteNodeAsMultiLine(YAML::Emitter& out, const YAML::Node& node) {
    if (node.IsScalar()) {
        std::string value = node.as<std::string>();
        if (value.find('\n') != std::string::npos) {
            out << YAML::Literal << value;
        } else {
            out << value;
        }
    } else if (node.IsSequence()) {
        out << YAML::BeginSeq;
        for (auto it = node.begin(); it != node.end(); ++it) {
            WriteNodeAsMultiLine(out, *it);
        }
        out << YAML::EndSeq;
    } else if (node.IsMap()) {
        out << YAML::BeginMap;
        for (auto it = node.begin(); it != node.end(); ++it) {
            out << YAML::Key << it->first.as<std::string>();
            out << YAML::Value;
            WriteNodeAsMultiLine(out, it->second);
        }
        out << YAML::EndMap;
    }
}

void common_cli::write_config_file(const YAML::Node &node)
{
    std::ofstream fout(AD_CONST_CONFIG_FILE);
    YAML::Emitter out;
    WriteNodeAsMultiLine(out, node);
    fout << out.c_str();
    fout.close();
}

std::string common_cli::check_params(const std::vector<std::string> &_params, uint32_t _index, const std::string &_prompt)
{
    std::string ret;
    if (_params.size() <= _index)
    {
        ret = "第" + std::to_string(_index + 1) + "个参数无效，要求传入" + _prompt + "\n";
    }

    return ret;
}
