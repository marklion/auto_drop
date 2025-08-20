#if !defined(_SM_RPC_H_)
#define _SM_RPC_H_

#include "../../rpc/gen_code/cpp/idl_types.h"
#include "../../rpc/gen_code/cpp/runner_sm.h"
#include "runner.h"
#include "../../public/event_sc/ad_event_sc.h"
#include "../../public/const_var_define.h"

class runner_sm_impl : public runner_smIf
{
    int m_fd = -1;
    std::shared_ptr<RUNNER> m_runner;

public:
    runner_sm_impl(std::shared_ptr<RUNNER> _runner) : m_runner(_runner)
    {
    }
    virtual void push_sm_event(const std::string &event_name) override;
    virtual void get_sm_state_string(std::string &_return) override;
    virtual bool match_device(const std::string &use_for, const u16 port) override;
    virtual bool clear_device(const std::string &use_for) override;
    virtual void stop_sm() override;
    virtual bool check_lua_code(const std::string &code, const bool is_real_run) override;
    virtual void send_sm_event(const std::string &event_name) override;
};

#endif // _SM_RPC_H_
