#if !defined(_REDIS_CONFIG_H_)
#define _REDIS_CONFIG_H_

#include "common_cli.h"

class REDIS_CONFIG_CLI: public common_cli
{
public:
    REDIS_CONFIG_CLI();
    std::string make_bdr() override;
    virtual void clear() override;
};

#endif // _REDIS_CONFIG_H_
