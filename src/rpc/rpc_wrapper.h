#if !defined(_RPC_WRAPPER_H_)
#define _RPC_WRAPPER_H_

#include "gen_code/cpp/driver_service.h"
#include "../public/const_var_define.h"

void rpc_wrapper_call_device(u16 _port, std::function<void(driver_serviceClient &)> _func);


#endif // _RPC_WRAPPER_H_)
