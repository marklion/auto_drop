#include "config_rpc.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include "../../rpc/ad_rpc.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../rpc/gen_code/cpp/runner_sm.h"
#include "../../public/const_var_define.h"
#include "rpc_wrapper.h"

u16 config_management_impl::start_daemon(const std::string &_path, const std::vector<std::string> &_argv, const std::string &_name)
{
    u16 min_port = AD_CONST_DAEMON_MIN_PORT;
    u16 max_port = AD_CONST_DAEMON_MAX_PORT;
    u16 ret = 0;
    for (u16 port = min_port; port < max_port; port++)
    {
        if (m_device_map.find(port) == m_device_map.end() && m_sm_map.find(port) == m_sm_map.end())
        {
            ret = port;
            break;
        }
    }
    auto exec_args = _argv;
    exec_args.insert(exec_args.begin(), std::to_string(ret));
    exec_args.insert(exec_args.begin(), _name);

    auto args_size = exec_args.size();

    auto pid = fork();
    if (pid <= 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        char **argv = (char **)malloc((args_size + 1) * sizeof(argv));
        for (size_t i = 0; i < args_size; i++)
        {
            argv[i] = (char *)malloc(exec_args[i].length() + 1);
            strcpy(argv[i], exec_args[i].c_str());
        }
        argv[args_size] = 0;
        execv(("/bin/" + _path).c_str(), argv);
        exit(0);
    }
    return ret;
}

void config_management_impl::start_device(device_config &_return, const std::string &driver_name, const std::vector<std::string> &argv, const std::string &device_name)
{
    device_config tmp;
    tmp.device_name = device_name;
    tmp.driver_name = driver_name;
    tmp.argv = argv;
    tmp.port = start_daemon(driver_name, argv, device_name);
    if (tmp.port > 0)
    {
        m_device_map[tmp.port] = tmp;
        _return = tmp;
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "start %s device failed:%s", device_name.c_str(), strerror(errno));
    }
}

void config_management_impl::stop_device(const u16 port)
{
    if (m_device_map.find(port) != m_device_map.end())
    {
        m_device_map.erase(port);
        rpc_wrapper_call_device(
            port, [&](driver_serviceClient &client)
            { client.stop_device(); });
    }
}

void config_management_impl::get_device_list(std::vector<device_config> &_return)
{
    for (auto &it : m_device_map)
    {
        _return.push_back(it.second);
    }
}

void config_management_impl::start_sm(sm_config &_return, const std::string &sm_name, const std::vector<std::string> &argv)
{
    sm_config tmp;
    tmp.sm_name = sm_name;
    tmp.argv = argv;
    tmp.port = start_daemon("sm_daemon", argv, sm_name);
    if (tmp.port > 0)
    {
        m_sm_map[tmp.port] = tmp;
        _return = tmp;
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "start %s sm failed:%s", sm_name.c_str(), strerror(errno));
    }
}

void config_management_impl::stop_sm(const u16 port)
{
    if (m_sm_map.find(port) != m_sm_map.end())
    {
        m_sm_map.erase(port);
        AD_RPC_SC::get_instance()->call_remote<runner_smClient>(
            port,
            "runner_sm",
            [](runner_smClient &client)
            {
                client.stop_sm();
            });
    }
}

void config_management_impl::get_sm_list(std::vector<sm_config> &_return)
{
    for (auto &it : m_sm_map)
    {
        _return.push_back(it.second);
    }
}
