#if !defined(_DEVICE_CONFIG_H_)
#define _DEVICE_CONFIG_H_
#include "common_cli.h"

class DEVICE_CONFIG_CLI : public common_cli
{
public:
    DEVICE_CONFIG_CLI();
    std::string make_bdr() override;
};

#endif // _DEVICE_CONFIG_H_
