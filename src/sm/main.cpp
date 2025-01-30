#include "lib/sm.h"
#include <iostream>

void say_hello(DYNAMIC_SM &_sm)
{
    std::cout << "hello" << std::endl;
}

void say_goodbye(DYNAMIC_SM &_sm)
{
    std::cout << "goodbye" << std::endl;
}

void prompt(DYNAMIC_SM &_sm)
{
    std::cout << "please bush keyboard" << std::endl;
}

DYNAMIC_SM_PTR init_sm(const std::string &_config_path)
{
    auto whole_config = YAML::LoadFile(_config_path);
    auto sm_state_fac = SM_STATE_FACTORY_PTR(new SM_STATE_FACTORY(whole_config["sm"]));
    sm_state_fac->register_actions("say_hello", say_hello);
    sm_state_fac->register_actions("say_goodbye", say_goodbye);
    sm_state_fac->register_actions("prompt", prompt);
    return DYNAMIC_SM_PTR(new DYNAMIC_SM(sm_state_fac->m_init_state, sm_state_fac));
}

int main(int argc, char const *argv[])
{
    AD_LOGGER::set_log_level(AD_LOGGER::DEBUG);
    auto sm = init_sm(argv[1]);
    sm->begin();
    std::string key_input;
    while (key_input != "q")
    {
        std::cin >> key_input;
        if (key_input == "y")
        {
            sm->proc_event("y_pushed");
        }
    }

    return 0;
}
