#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/timerfd.h>
#include "ad_event_sc.h"

AD_EVENT_SC::AD_EVENT_SC() : m_logger("", "EVENT_SC")
{
    m_epollFd = epoll_create1(0);
    if (m_epollFd == -1)
    {
        m_logger.log(AD_LOGGER::ERROR, "failed to create epoll fd:%s", strerror(errno));
    }
}

AD_EVENT_SC::~AD_EVENT_SC()
{
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
    return timer;
}

void AD_EVENT_SC::stopTimer(AD_EVENT_SC_TIMER_NODE_PTR _timer)
{
    unregisterNode(_timer);
}

void AD_EVENT_SC::runEventLoop()
{
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    while (m_fdToNode.size() > 0)
    {
        int n = epoll_wait(m_epollFd, events, MAX_EVENTS, -1); // 阻塞等待事件
        if (n == -1)
        {
            m_logger.log(AD_LOGGER::ERROR, "epoll_wait error:%s", strerror(errno));
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            int fd = events[i].data.fd;
            if (m_fdToNode.find(fd) != m_fdToNode.end())
            {
                // 调用对应的处理函数
                m_fdToNode[fd]->handleEvent();
            }
        }
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

AD_EVENT_SC_TIMER_NODE::AD_EVENT_SC_TIMER_NODE(int _timeout, std::function<void()> _callback) : m_callback(_callback), m_logger("", "TIMER")
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
    }
}

AD_EVENT_SC_TIMER_NODE::~AD_EVENT_SC_TIMER_NODE()
{
    if (m_timer_fd >= 0)
    {
        close(m_timer_fd);
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
            m_callback();
        }
    }
    else
    {
        m_logger.log(AD_LOGGER::ERROR, "Failed to read timer fd:%s", strerror(errno));
    }
}
