#if !defined(_COMMON_CLI_H_)
#define _COMMON_CLI_H_

#include <memory>
#include <string>
#include "cli.h"
#include "../rpc/ad_rpc.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "../public/const_var_define.h"

class common_cli {
public:
    std::unique_ptr<cli::Menu> menu;
    std::string menu_name;
    common_cli(std::unique_ptr<cli::Menu> _menu, const std::string &_menu_name):menu(_menu.release()),menu_name(_menu_name) {
    }
    virtual std::string make_bdr() = 0;
};
#define CLI_MENU_ITEM(x) #x, [](std::ostream &out, std::vector<std::string> _params) { x(out, _params); }

#endif // _COMMON_CLI_H_
