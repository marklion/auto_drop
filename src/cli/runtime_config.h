#if !defined(_RUNTIME_CONFIG_H_)
#define _RUNTIME_CONFIG_H_

#include "common_cli.h"

class RUNTIME_CONFIG_CLI : public common_cli
{
public:
    RUNTIME_CONFIG_CLI();
    std::string make_bdr() override;
};

#endif // _RUNTIME_CONFIG_H_
