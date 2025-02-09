#if !defined(_CONFIG_MANAGEMENT_CLI_H_)
#define _CONFIG_MANAGEMENT_CLI_H_
#include "common_cli.h"

class config_management_cli : public common_cli {
public:
    config_management_cli();
    std::string make_bdr() override;
};


#endif // _CONFIG_MANAGEMENT_CLI_H_
