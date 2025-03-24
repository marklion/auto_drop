#include "config_rpc.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include "../../rpc/ad_rpc.h"
#include "../../rpc/gen_code/cpp/driver_service.h"
#include "../../rpc/gen_code/cpp/runner_sm.h"
#include "../../public/const_var_define.h"
#include "rpc_wrapper.h"
#include "../action/ad_action.h"

static int create_sub_process(const std::string &_path, const std::vector<std::string> &_argv)
{
    auto pid = fork();
    if (pid <= 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        char **argv = (char **)malloc((_argv.size() + 1) * sizeof(argv));
        for (size_t i = 0; i < _argv.size(); i++)
        {
            argv[i] = (char *)malloc(_argv[i].length() + 1);
            strcpy(argv[i], _argv[i].c_str());
        }
        argv[_argv.size()] = 0;
        execv(("/bin/" + _path).c_str(), argv);
        exit(0);
    }
    return pid;
}

class SUBPROCESS_EVENT_SC_NODE:public AD_EVENT_SC_NODE
{
    std::string m_path;
    std::vector<std::string> m_argv;
    std::string m_name;
    int m_pid;
    AD_LOGGER m_logger;
    int m_fd;
public:
    SUBPROCESS_EVENT_SC_NODE(const std::string &_path, const std::vector<std::string> &_argv, const std::string &_name, int _pid)
        :m_path(_path),m_argv(_argv),m_name(_name),m_pid(_pid),m_logger( "subprocess " + _name + std::to_string(_pid))
    {
        m_fd = syscall(SYS_pidfd_open, _pid, 0);
    }
    ~SUBPROCESS_EVENT_SC_NODE()
    {
        close(m_fd);
    }
    virtual int getFd() const override
    {
        return m_fd;
    }
    int getPid() const
    {
        return m_pid;
    }
    virtual void handleEvent() override
    {
        int status;
        AD_RPC_SC::get_instance()->unregisterNode(shared_from_this());
        waitpid(m_pid, &status, WNOHANG);
        if (!WIFEXITED(status))
        {
            m_logger.log(AD_LOGGER::ERROR, "subprocess %s exit with code %d", m_name.c_str(), WEXITSTATUS(status));
            AD_RPC_SC::get_instance()->yield_by_timer(2);
            auto new_pid  = create_sub_process(m_path, m_argv);
            close(m_fd);
            m_fd = syscall(SYS_pidfd_open, new_pid, 0);
            m_pid = new_pid;
            AD_RPC_SC::get_instance()->registerNode(shared_from_this());
        }
    }
};

std::pair<u16, SUBPROCESS_EVENT_SC_NODE_PTR> config_management_impl::start_daemon(const std::string &_path, const std::vector<std::string> &_argv, const std::string &_name)
{
    u16 min_port = AD_CONST_DAEMON_MIN_PORT;
    u16 max_port = AD_CONST_DAEMON_MAX_PORT;
    std::pair<u16, SUBPROCESS_EVENT_SC_NODE_PTR> ret;
    ret.first = 0;
    for (u16 port = min_port; port < max_port; port++)
    {
        if (m_device_map.find(port) == m_device_map.end() && m_sm_map.find(port) == m_sm_map.end())
        {
            ret.first = port;
            break;
        }
    }
    auto exec_args = _argv;
    exec_args.insert(exec_args.begin(), std::to_string(ret.first));
    exec_args.insert(exec_args.begin(), _name);

    auto args_size = exec_args.size();
    auto pid = create_sub_process(_path, exec_args);
    if (pid > 0)
    {
        ret.second = std::make_shared<SUBPROCESS_EVENT_SC_NODE>(_path, exec_args, _name, pid);
        AD_RPC_SC::get_instance()->registerNode(ret.second);
    }
    return ret;
}

void config_management_impl::wait_stop_daemon(const u16 port)
{
    if (m_subprocess_map.find(port) != m_subprocess_map.end())
    {
        auto node = m_subprocess_map[port];
        int status;
        AD_RPC_SC::get_instance()->unregisterNode(node);
        AD_RPC_SC::get_instance()->yield_by_fd(node->getFd());
        waitpid(node->getPid(), &status, WNOHANG);
        m_subprocess_map.erase(port);
    }
}

void config_management_impl::start_device(device_config &_return, const std::string &driver_name, const std::vector<std::string> &argv, const std::string &device_name)
{
    device_config tmp;
    tmp.device_name = device_name;
    tmp.driver_name = driver_name;
    tmp.argv = argv;
    auto run_info = start_daemon(driver_name, argv, device_name);
    tmp.port = run_info.first;
    if (tmp.port > 0)
    {
        m_device_map[tmp.port] = tmp;
        m_subprocess_map[tmp.port] = run_info.second;
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
        wait_stop_daemon(port);
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
    auto run_info = start_daemon("sm_daemon", argv, sm_name);
    tmp.port = run_info.first;
    if (tmp.port > 0)
    {
        m_sm_map[tmp.port] = tmp;
        m_subprocess_map[tmp.port] = run_info.second;
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
        wait_stop_daemon(port);
    }
}

void config_management_impl::get_sm_list(std::vector<sm_config> &_return)
{
    for (auto &it : m_sm_map)
    {
        _return.push_back(it.second);
    }
}

void config_management_impl::restart_redis_subscriber()
{
    auto sc = AD_RPC_SC::get_instance();
    if (m_redis_node)
    {
        sc->unregisterNode(m_redis_node);
    }
    m_redis_node = create_redis_event_node();
    if (m_redis_node)
    {
        sc->registerNode(m_redis_node);
    }
}
