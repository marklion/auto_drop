#include "rpc_wrapper.h"

void rpc_wrapper_call_device(u16 _port, std::function<void(driver_serviceClient &)> _func)
{
    auto ad_rpc_sc = AD_RPC_SC::get_instance();
    ad_rpc_sc->call_remote<driver_serviceClient>(
        _port,
        "driver_service",
        _func);
}