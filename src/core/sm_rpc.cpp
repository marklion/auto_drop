#include "sm_rpc.h"


void runner_sm_impl::push_sm_event(const std::string &event_name)
{
    m_runner->proc_event(event_name);
}