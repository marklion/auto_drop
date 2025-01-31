#if !defined(_AD_EVENT_SC_H_)
#define _AD_EVENT_SC_H_
#include <unordered_map>
#include <functional>
#include <memory>
#include "../utils/ad_utils.h"


class AD_EVENT_SC_NODE {
public:
    virtual ~AD_EVENT_SC_NODE() = default;

    // 获取文件描述符
    virtual int getFd() const = 0;

    // 处理文件描述符上的事件
    virtual void handleEvent() = 0;
};
typedef std::shared_ptr<AD_EVENT_SC_NODE> AD_EVENT_SC_NODE_PTR;
class AD_EVENT_SC_TIMER_NODE : public AD_EVENT_SC_NODE {
    int m_timer_fd = -1;
    std::function<void()> m_callback;
    AD_LOGGER m_logger;
public:
    AD_EVENT_SC_TIMER_NODE(int _timeout, std::function<void()> _callback);

    ~AD_EVENT_SC_TIMER_NODE();

    int getFd() const override;

    void handleEvent() override;
};
typedef std::shared_ptr<AD_EVENT_SC_TIMER_NODE> AD_EVENT_SC_TIMER_NODE_PTR;
class AD_EVENT_SC {
public:
    AD_EVENT_SC();

    ~AD_EVENT_SC();

    // 注册 AD_EVENT_SC_NODE 对象
    void registerNode(AD_EVENT_SC_NODE_PTR _node);
    // 注销 AD_EVENT_SC_NODE 对象
    void unregisterNode(AD_EVENT_SC_NODE_PTR _node);

    AD_EVENT_SC_TIMER_NODE_PTR startTimer(int _timeout, std::function<void()> _callback);
    void stopTimer(AD_EVENT_SC_TIMER_NODE_PTR _timer);

    // 运行事件循环
    void runEventLoop();

    void stopEventLoop();

private:
    int m_epollFd = -1; // epoll 文件描述符
    std::unordered_map<int, AD_EVENT_SC_NODE_PTR> m_fdToNode; // 文件描述符到节点的映射
    AD_LOGGER m_logger;
};



#endif // _AD_EVENT_SC_H_
