#include "runner.h"

std::shared_ptr<RUNNER> RUNNER::runner_init(const YAML::Node &_sm_config)
{
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(_sm_config));
    auto ret = std::make_shared<RUNNER>(sm_state_fac->m_init_state, sm_state_fac);
    ret->begin();

    return ret;
}
