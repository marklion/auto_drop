#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include "ad_event_sc.h"

AD_EVENT_SC::AD_EVENT_SC() : m_logger("EVENT_SC")
{
    m_epollFd = epoll_create1(0);
    if (m_epollFd == -1)
    {
        m_logger.log(AD_LOGGER::ERROR, "failed to create epoll fd:%s", strerror(errno));
    }
}

AD_EVENT_SC::~AD_EVENT_SC()
{
    m_co_routines.clear();
    stopEventLoop();
    close(m_epollFd);
}

void AD_EVENT_SC::registerNode(AD_EVENT_SC_NODE_PTR _node)
{
    int fd = _node->getFd();
    if (m_fdToNode.find(fd) == m_fdToNode.end())
    {
        struct epoll_event ev = {0};
        ev.events = EPOLLIN; // 监听读事件
        ev.data.fd = fd;

        if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) != -1)
        {
            m_fdToNode[fd] = _node;
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Failed to add fd to epoll:%s", strerror(errno));
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "File descriptor already registered");
    }
}

void AD_EVENT_SC::unregisterNode(AD_EVENT_SC_NODE_PTR _node)
{
    int fd = _node->getFd();
    if (m_fdToNode.find(fd) != m_fdToNode.end())
    {
        if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr) != -1)
        {
            m_fdToNode.erase(fd);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Failed to remove fd from epoll:%s", strerror(errno));
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "File descriptor not registered");
    }
}

AD_EVENT_SC_TIMER_NODE_PTR AD_EVENT_SC::startTimer(int _timeout, std::function<void()> _callback)
{
    auto timer = std::make_shared<AD_EVENT_SC_TIMER_NODE>(_timeout, _callback);
    registerNode(timer);
    m_logger.log(AD_LOGGER::DEBUG, "Start %ds timer", _timeout);
    return timer;
}

void AD_EVENT_SC::stopTimer(AD_EVENT_SC_TIMER_NODE_PTR _timer)
{
    unregisterNode(_timer);
    m_logger.log(AD_LOGGER::DEBUG, "Stop timer");
}

class AD_CO_EVENT_NODE : public AD_EVENT_SC_NODE
{
    AD_EVENT_SC_PTR m_event_sc;
    AD_CO_ROUTINE_PTR m_co;
    int m_fd = -1;

public:
    AD_CO_EVENT_NODE(AD_EVENT_SC_PTR _event_sc, int _fd) : m_event_sc(_event_sc), m_co(_event_sc->get_current_co()), m_fd(_fd)
    {
    }
    virtual int getFd() const override
    {
        return m_fd;
    }
    virtual void handleEvent() override
    {
        m_co->set_co_state(AD_CO_ROUTINE::ACR_STATE_READY);
        m_event_sc->unregisterNode(shared_from_this());
    }
};
void AD_EVENT_SC::yield_by_fd(int _fd)
{
    auto co_node = std::make_shared<AD_CO_EVENT_NODE>(std::static_pointer_cast<AD_EVENT_SC>(shared_from_this()), _fd);
    registerNode(co_node);
    yield_co();
}

void AD_EVENT_SC::yield_by_timer(int _timeout)
{
    auto cur_cor = m_current_co;
    AD_EVENT_SC_TIMER_NODE_PTR timer = startTimer(_timeout, [&]() {
        stopTimer(timer);
        cur_cor->set_co_state(AD_CO_ROUTINE::ACR_STATE_READY);
    });
    yield_co();
}

void AD_EVENT_SC::runEventLoop()
{
    while (m_fdToNode.size() > 0)
    {
        handleEvent();
    }
}

void AD_EVENT_SC::stopEventLoop()
{
    for (auto &pair : m_fdToNode)
    {
        if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, pair.first, nullptr) == -1)
        {
            m_logger.log(AD_LOGGER::ERROR, "Failed to remove fd from epoll:%s", strerror(errno));
        }
    }
    m_fdToNode.clear();
}

int AD_EVENT_SC::getFd() const
{
    return m_epollFd;
}

void AD_EVENT_SC::handleEvent()
{
    while (true)
    {
        bool has_ready_co = false;
        for (auto itr = m_co_routines.begin(); itr != m_co_routines.end();)
        {
            auto &co = *itr;
            if (co->get_co_state() == AD_CO_ROUTINE::ACR_STATE_READY)
            {
                has_ready_co = true;
                resume_co(co);
            }
            if (co->get_co_state() == AD_CO_ROUTINE::ACR_STATE_DEAD)
            {
                itr = m_co_routines.erase(itr);
            }
            else
            {

                itr++;
            }
        }
        if (!has_ready_co)
        {
            break;
        }
    }
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS] = {0};
    if (m_fdToNode.size() == 0)
    {
        return;
    }
    int n = epoll_wait(m_epollFd, events, MAX_EVENTS, -1); // 阻塞等待事件
    if (n != -1)
    {
        for (int i = 0; i < n; ++i)
        {
            int fd = events[i].data.fd;
            if (m_fdToNode.find(fd) != m_fdToNode.end())
            {
                auto p_event_node = m_fdToNode[fd];
                add_co(
                    [p_event_node]()
                    { p_event_node->handleEvent(); });
            }
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "epoll_wait error:%s", strerror(errno));
    }
}

void AD_EVENT_SC::non_block_system(const std::string &_cmd)
{
    auto pid = fork();
    if (pid == 0)
    {
        execl("/bin/bash", "/bin/bash", "-c", _cmd.c_str(), nullptr);
        exit(0);
    }
    else
    {
        auto fd = syscall(SYS_pidfd_open, pid, 0);
        if (fd >= 0)
        {
            yield_by_fd(fd);
            int status;
            waitpid(pid, &status, 0);
            close(fd);
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Failed to open pidfd:%s", strerror(errno));
        }
    }
}

AD_EVENT_SC_TIMER_NODE::AD_EVENT_SC_TIMER_NODE(int _timeout, std::function<void()> _callback) : m_callback(_callback), m_timeout(_timeout), m_logger("TIMER")
{
    auto fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd >= 0)
    {
        itimerspec ts = {
            {_timeout, 0}, // 第一次超时时间
            {_timeout, 0}  // 之后每次超时时间
        };
        timerfd_settime(fd, 0, &ts, nullptr);
        m_timer_fd = fd;
        m_logger.log(AD_LOGGER::DEBUG, "Create %ds timer fd:%d", _timeout, m_timer_fd);
    }
}

AD_EVENT_SC_TIMER_NODE::~AD_EVENT_SC_TIMER_NODE()
{
    if (m_timer_fd >= 0)
    {
        close(m_timer_fd);
        m_logger.log(AD_LOGGER::DEBUG, "Close timer fd:%d", m_timer_fd);
    }
}

int AD_EVENT_SC_TIMER_NODE::getFd() const
{
    return m_timer_fd;
}

void AD_EVENT_SC_TIMER_NODE::handleEvent()
{
    unsigned long long exp;
    auto read_len = read(m_timer_fd, &exp, sizeof(exp));
    if (read_len == sizeof(exp))
    {
        for (int i = 0; i < exp; ++i)
        {
            m_logger.log(AD_LOGGER::DEBUG, "%ds timer fd:%d expired", m_timeout, m_timer_fd);
            m_callback();
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "Failed to read timer fd:%s", strerror(errno));
    }
}

AD_EVENT_SC_TCP_DATA_NODE::AD_EVENT_SC_TCP_DATA_NODE(int _fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR _listen_node) : m_fd(_fd), m_listen_node(_listen_node), m_logger("TCP_DATA" + std::to_string(_fd))
{
}

AD_EVENT_SC_TCP_DATA_NODE::~AD_EVENT_SC_TCP_DATA_NODE()
{
    if (m_fd >= 0)
    {
        close(m_fd);
    }
}

void AD_EVENT_SC_TCP_DATA_NODE::handleEvent()
{
    unsigned char buf[4096] = {0};
    auto recv_len = recv(m_fd, buf, sizeof(buf), SOCK_NONBLOCK);
    if (recv_len > 0)
    {
        handleRead(buf, recv_len);
    }
    else
    {
        if (recv_len != 0)
        {

            m_logger.log(AD_LOGGER::ERROR, "Failed to read from fd:%s", strerror(errno));
        }
        m_listen_node->close_data_node(m_fd);
    }
}

AD_EVENT_SC_TCP_LISTEN_NODE::AD_EVENT_SC_TCP_LISTEN_NODE(
    unsigned short _port,
    CREATE_DATA_FUNC _create_data_func,
    AD_EVENT_SC_PTR _event_sc) : m_logger("TCP_LISTEN" + std::to_string(_port)),
                                 m_create_data_func(_create_data_func),
                                 m_event_sc(_event_sc), m_port(_port)
{
    auto listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd >= 0)
    {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            if (listen(listen_fd, 10) == 0)
            {
                m_listen_fd = listen_fd;
            }
            else
            {
                m_logger.log(AD_LOGGER::ERROR, "Failed to listen:%s", strerror(errno));
            }
        }
        else
        {
            m_logger.log(AD_LOGGER::ERROR, "Failed to bind:%s", strerror(errno));
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "Failed to create listen fd:%s", strerror(errno));
    }
}

AD_EVENT_SC_TCP_LISTEN_NODE::~AD_EVENT_SC_TCP_LISTEN_NODE()
{
    for (auto &pair : m_fd_to_data_node)
    {
        close_data_node(pair.first);
    }
    if (m_listen_fd >= 0)
    {
        close(m_listen_fd);
    }
}

void AD_EVENT_SC_TCP_LISTEN_NODE::handleEvent()
{
    sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);
    auto data_fd = accept(m_listen_fd, (sockaddr *)&addr, &addr_len);
    if (data_fd >= 0)
    {
        auto data_node = m_create_data_func(data_fd, std::static_pointer_cast<AD_EVENT_SC_TCP_LISTEN_NODE>(shared_from_this()));
        m_fd_to_data_node[data_fd] = data_node;
        m_event_sc->registerNode(data_node);
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "Failed to accept:%s", strerror(errno));
    }
}

void AD_EVENT_SC_TCP_LISTEN_NODE::close_data_node(int _fd)
{
    if (m_fd_to_data_node.find(_fd) != m_fd_to_data_node.end())
    {
        m_event_sc->unregisterNode(m_fd_to_data_node[_fd]);
        m_fd_to_data_node.erase(_fd);
    }
}
static void co_routine_func(AD_CO_ROUTINE *_co)
{
    _co->m_func();
    _co->set_co_state(AD_CO_ROUTINE::ACR_STATE_DEAD);
}
static long g_co_id = 0;
AD_CO_ROUTINE::AD_CO_ROUTINE(AD_CO_ROUTINE_FUNC _func, ucontext_t *_main_co) : m_func(_func)
{
    getcontext(&m_context);
    m_context.uc_stack.ss_sp = m_stacks;
    m_context.uc_stack.ss_size = sizeof(m_stacks);
    m_context.uc_link = _main_co;
    makecontext(&m_context, (void (*)())co_routine_func, 1, this);
    g_co_id++;
    g_co_id %= 1024;
}

ucontext_t AD_CO_ROUTINE::m_global_context;

void AD_CO_ROUTINE::co_run(std::function<void()> _main_func)
{
    auto init_co = std::make_shared<AD_CO_ROUTINE>(_main_func, &m_global_context);
    init_co->resume(&m_global_context);
}
