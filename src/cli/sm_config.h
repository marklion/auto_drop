#if !defined(_SM_CONFIG_H_)
#define _SM_CONFIG_H_

#include "common_cli.h"

class SM_CONFIG_CLI : public common_cli
{
public:
    SM_CONFIG_CLI();
    virtual std::string make_bdr() override;
    virtual void clear() override;
};


#endif // _SM_CONFIG_H_
