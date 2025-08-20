#include "sm_rpc.h"
#include "../../rpc/ad_rpc.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../rpc/rpc_wrapper.h"

void runner_sm_impl::push_sm_event(const std::string &event_name)
{
    m_runner->proc_event(event_name);
}

void runner_sm_impl::get_sm_state_string(std::string &_return)
{
    _return = m_runner->m_current_state->m_state_name;
}

bool runner_sm_impl::match_device(const std::string &use_for, const u16 port)
{
    bool ret = false;
    m_runner->set_device(use_for, port);
    auto ad_rpc_sc = AD_RPC_SC::get_instance();
    rpc_wrapper_call_device(
        port,
        [&](driver_serviceClient &client)
        {
            ret = client.set_sm(ad_rpc_sc->get_listen_port());
        });
    return ret;
}

bool runner_sm_impl::clear_device(const std::string &use_for)
{
    bool ret = false;
    auto device_port = m_runner->get_device(use_for);

    rpc_wrapper_call_device(
        device_port,
        [&](driver_serviceClient &client)
        {
            ret = client.set_sm(0);
        });
    if (ret)
    {
        m_runner->set_device(use_for, 0);
    }
    return ret;
}

void runner_sm_impl::stop_sm()
{
    AD_RPC_SC::get_instance()->stop_server();
}

bool runner_sm_impl::check_lua_code(const std::string &code, const bool is_real_run)
{
    bool ret = false;

    auto p_tmp_runner = RUNNER::runner_init(YAML::LoadFile("/conf/sample.yaml")["sm"]);
    p_tmp_runner->lock_sm();
    luaL_openlibs(p_tmp_runner->get_lua_state());
    p_tmp_runner->register_lua_function();
    luabridge::setGlobal(p_tmp_runner->get_lua_state(), (RUNNER *)(p_tmp_runner.get()), "sm");
    p_tmp_runner->unlock_sm();
    auto check_ret = p_tmp_runner->check_lua_code(code, is_real_run);
    if (check_ret.empty())
    {
        ret = true;
    }
    else
    {
        ad_gen_exp exp;
        exp.msg = "lua code error:" + check_ret;
        throw exp;
    }

    return ret;
}

void runner_sm_impl::send_sm_event(const std::string &event_name)
{
    m_runner->trigger_sm_by_event(event_name);
}