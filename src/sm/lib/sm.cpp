#include "sm.h"
#include <yaml-cpp/yaml.h>

SM_STATE_PTR SM_STATE_FACTORY::create_sm_state(const std::string &_name)
{
    SM_STATE_PTR ret;
    auto states = m_config["states"];
    for (const auto &itr : states)
    {
        auto state_name = itr["name"].as<std::string>("");
        if (state_name == _name)
        {
            auto enter_funcs = itr["enter"].as<std::vector<std::string>>();
            auto exit_funcs = itr["exit"].as<std::vector<std::string>>();
            auto do_funcs = itr["do"]["actions"].as<std::vector<std::string>>();
            ACTION_FUNC_LIST enter_funcs_list;
            ACTION_FUNC_LIST exit_funcs_list;
            ACTION_FUNC_LIST do_funcs_list;
            for (auto &func : enter_funcs)
            {
                enter_funcs_list.push_back(m_action_map[func]);
            }
            for (auto &func : exit_funcs)
            {
                exit_funcs_list.push_back(m_action_map[func]);
            }
            for (auto &func : do_funcs)
            {
                do_funcs_list.push_back(m_action_map[func]);
            }

            auto next_state = itr["do"]["next"].as<std::string>("");
            auto tmp_state = new SM_STATE(state_name, enter_funcs_list, exit_funcs_list, do_funcs_list, next_state, shared_from_this());
            ret.reset(tmp_state);
            auto events = itr["events"];
            for (const auto &ev_itr:events)
            {
                ret->add_event(ev_itr["trigger"].as<std::string>(), ev_itr["next"].as<std::string>());
            }
            break;
        }
    }

    return ret;
}
