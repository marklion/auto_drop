#if !defined(_SM_RPC_H_)
#define _SM_RPC_H_

#include "../rpc/gen_code/cpp/idl_types.h"
#include "../rpc/gen_code/cpp/runner_sm.h"
#include "runner.h"
#include "../public/event_sc/ad_event_sc.h"

class runner_sm_impl : public runner_smIf
{
    int m_fd = -1;
    std::shared_ptr<RUNNER> m_runner;
public:
    runner_sm_impl(std::shared_ptr<RUNNER> _runner):m_runner(_runner)
    {
    }
    virtual void push_sm_event(const std::string &event_name) override;
};

#endif // _SM_RPC_H_
