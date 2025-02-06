#if !defined(_AD_EVENT_SC_H_)
#define _AD_EVENT_SC_H_
#include <unordered_map>
#include <functional>
#include <memory>
#include <ucontext.h>
#include <vector>
#include "../utils/ad_utils.h"

class AD_EVENT_SC_NODE : public std::enable_shared_from_this<AD_EVENT_SC_NODE>
{
public:
    virtual ~AD_EVENT_SC_NODE() = default;

    // 获取文件描述符
    virtual int getFd() const = 0;

    // 处理文件描述符上的事件
    virtual void handleEvent() = 0;
};
typedef std::shared_ptr<AD_EVENT_SC_NODE> AD_EVENT_SC_NODE_PTR;
class AD_EVENT_SC_TIMER_NODE : public AD_EVENT_SC_NODE
{
    int m_timer_fd = -1;
    std::function<void()> m_callback;
    int m_timeout;
    AD_LOGGER m_logger;

public:
    AD_EVENT_SC_TIMER_NODE(int _timeout, std::function<void()> _callback);

    ~AD_EVENT_SC_TIMER_NODE();

    int getFd() const override;

    void handleEvent() override;
};
typedef std::shared_ptr<AD_EVENT_SC_TIMER_NODE> AD_EVENT_SC_TIMER_NODE_PTR;
typedef std::function<void()> AD_CO_ROUTINE_FUNC;
struct AD_CO_ROUTINE
{
public:
    enum ACR_STATE
    {
        ACR_STATE_READY,
        ACR_STATE_RUNNING,
        ACR_STATE_SUSPEND,
        ACR_STATE_DEAD,
    };

private:
    ACR_STATE m_state = ACR_STATE_READY;
    AD_LOGGER m_logger;
    static ucontext_t m_global_context;
public:
    ucontext_t m_context = {0};
    char m_stacks[128 * 1024] = {0};
    AD_CO_ROUTINE_FUNC m_func;
    AD_CO_ROUTINE(AD_CO_ROUTINE_FUNC _func, ucontext_t *_main_co);
    ~AD_CO_ROUTINE()
    {
        m_logger.log(AD_LOGGER::DEBUG, "co was destroyed");
    }
    void set_co_state(ACR_STATE _state)
    {
        if (_state == ACR_STATE_READY)
        {
            m_logger.log(AD_LOGGER::DEBUG, "co was re-active");
        }
        m_state = _state;
    }
    ACR_STATE get_co_state()
    {
        return m_state;
    }
    void resume(ucontext_t *_main_co)
    {
        m_logger.log(AD_LOGGER::DEBUG, "resume co");
        set_co_state(ACR_STATE_RUNNING);
        swapcontext(_main_co, &m_context);
        m_logger.log(AD_LOGGER::DEBUG, "back to main co because co state is %d", m_state);
    }
    void yield(ucontext_t *_main_co)
    {
        m_logger.log(AD_LOGGER::DEBUG, "yield co");
        set_co_state(ACR_STATE_SUSPEND);
        swapcontext(&m_context, _main_co);
        m_logger.log(AD_LOGGER::DEBUG, "back to co");
    }
    static void co_run(std::function<void()> _main_func);
};
typedef std::shared_ptr<AD_CO_ROUTINE> AD_CO_ROUTINE_PTR;

class AD_EVENT_SC : public AD_EVENT_SC_NODE
{
public:
    AD_EVENT_SC();

    ~AD_EVENT_SC();

    // 注册 AD_EVENT_SC_NODE 对象
    void registerNode(AD_EVENT_SC_NODE_PTR _node);
    // 注销 AD_EVENT_SC_NODE 对象
    void unregisterNode(AD_EVENT_SC_NODE_PTR _node);

    AD_EVENT_SC_TIMER_NODE_PTR startTimer(int _timeout, std::function<void()> _callback);
    void stopTimer(AD_EVENT_SC_TIMER_NODE_PTR _timer);

    void yield_by_fd(int _fd);

    // 运行事件循环
    void runEventLoop();

    void stopEventLoop();
    virtual int getFd() const override;
    virtual void handleEvent() override;
    AD_CO_ROUTINE_PTR get_current_co()
    {
        return m_current_co;
    }
    void resume_co(AD_CO_ROUTINE_PTR _co)
    {
        m_current_co = _co;
        _co->resume(&m_main_context);
        if (_co->get_co_state() == AD_CO_ROUTINE::ACR_STATE_DEAD)
        {
            m_current_co.reset();
        }
    }
    void yield_co()
    {
        auto p_tmp = m_current_co;
        m_current_co.reset();
        p_tmp->yield(&m_main_context);
    }
    void add_co(AD_CO_ROUTINE_FUNC _func)
    {
        auto co = std::make_shared<AD_CO_ROUTINE>(_func, &m_main_context);
        m_co_routines.push_back(co);
    }

private:
    int m_epollFd = -1;                                       // epoll 文件描述符
    std::unordered_map<int, AD_EVENT_SC_NODE_PTR> m_fdToNode; // 文件描述符到节点的映射
    AD_LOGGER m_logger;
    std::vector<AD_CO_ROUTINE_PTR> m_co_routines;
    ucontext_t m_main_context;
    AD_CO_ROUTINE_PTR m_current_co;
};
typedef std::shared_ptr<AD_EVENT_SC> AD_EVENT_SC_PTR;

class AD_EVENT_SC_TCP_LISTEN_NODE;
typedef std::shared_ptr<AD_EVENT_SC_TCP_LISTEN_NODE> AD_EVENT_SC_TCP_LISTEN_NODE_PTR;
class AD_EVENT_SC_TCP_DATA_NODE;
typedef std::shared_ptr<AD_EVENT_SC_TCP_DATA_NODE> AD_EVENT_SC_TCP_DATA_NODE_PTR;
class AD_EVENT_SC_TCP_DATA_NODE : public AD_EVENT_SC_NODE
{
    int m_fd = -1;
    AD_EVENT_SC_TCP_LISTEN_NODE_PTR m_listen_node;
    AD_LOGGER m_logger;

public:
    AD_EVENT_SC_TCP_DATA_NODE(int _fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR _listen_node);

    virtual ~AD_EVENT_SC_TCP_DATA_NODE();

    int getFd() const override
    {
        return m_fd;
    }
    void handleEvent() override;
    virtual void handleRead(const unsigned char *_data, unsigned long _size) = 0;
    virtual void handleError() {};
};

typedef std::function<AD_EVENT_SC_TCP_DATA_NODE_PTR(int, AD_EVENT_SC_TCP_LISTEN_NODE_PTR)> CREATE_DATA_FUNC;
class AD_EVENT_SC_TCP_LISTEN_NODE : public AD_EVENT_SC_NODE
{
    unsigned short m_port;
    int m_listen_fd = -1;
    AD_LOGGER m_logger;
    std::map<int, AD_EVENT_SC_TCP_DATA_NODE_PTR> m_fd_to_data_node;
    CREATE_DATA_FUNC m_create_data_func;
    AD_EVENT_SC_PTR m_event_sc;

public:
    AD_EVENT_SC_TCP_LISTEN_NODE(unsigned short _port, CREATE_DATA_FUNC _create_data_func, AD_EVENT_SC_PTR _event_sc);

    virtual ~AD_EVENT_SC_TCP_LISTEN_NODE();

    int getFd() const override
    {
        return m_listen_fd;
    }
    unsigned short getPort() const
    {
        return m_port;
    }

    void handleEvent() override;
    void close_data_node(int _fd);
};

#endif // _AD_EVENT_SC_H_
