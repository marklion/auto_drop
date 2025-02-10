#if !defined(_COMMON_CLI_H_)
#define _COMMON_CLI_H_

#include <memory>
#include <string>
#include <yaml-cpp/yaml.h>
#include "cli.h"
#include "../rpc/ad_rpc.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "../public/const_var_define.h"

class common_cli {
public:
    std::unique_ptr<cli::Menu> menu;
    std::string menu_name;
    common_cli(std::unique_ptr<cli::Menu> _menu, const std::string &_menu_name):menu(_menu.release()),menu_name(_menu_name) {
        menu->Insert("bdr", [this](std::ostream &_out) {
            _out << make_bdr() << std::endl;
        });
    }
    virtual std::string make_bdr() = 0;
    static YAML::Node read_config_file();
    static void write_config_file(const YAML::Node &node);
    static std::string check_params(const std::vector<std::string> &_params, uint32_t _index, const std::string &_prompt);
};
#define CLI_MENU_ITEM(x) #x, [](std::ostream &out, std::vector<std::string> _params) { x(out, _params); }

#endif // _COMMON_CLI_H_
