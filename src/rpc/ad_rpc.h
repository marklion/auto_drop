#if !defined(_AD_RPC_H_)
#define _AD_RPC_H_
#include <thrift/TProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/processor/TMultiplexedProcessor.h>
#include <thrift/protocol/TMultiplexedProtocol.h>
#include "../public/event_sc/ad_event_sc.h"
#include "gen_code/cpp/idl_types.h"

class AD_RPC_TRANSPORT : public apache::thrift::transport::TTransport
{
    std::string m_recv_buf;
    std::shared_ptr<apache::thrift::transport::TTransport> m_transport;
    std::string m_bufferd_recv_data;
public:
    AD_RPC_TRANSPORT(std::shared_ptr<apache::thrift::transport::TTransport> transport) : m_transport(transport) {}
    virtual bool isOpen() const override
    {
        return m_transport->isOpen();
    }
    virtual void open() override
    {
        m_transport->open();
    }
    virtual void close() override
    {
        m_transport->close();
    }
    void read_more()
    {
        unsigned char buffer[1024] = {0};
        auto recv_len = m_transport->read(buffer, sizeof(buffer));
        m_recv_buf.append((char *)buffer, recv_len);
        while (m_recv_buf.size() >= 4)
        {
            // 2. 提取大端序长度并转换为主机字节序
            uint32_t msg_len;
            memcpy(&msg_len, m_recv_buf.data(), 4);
            msg_len = ntohl(msg_len); // 关键：网络字节序转主机序

            // 3. 检查是否包含完整消息体
            if (m_recv_buf.size() >= 4 + msg_len)
            {
                m_bufferd_recv_data += m_recv_buf.substr(4, msg_len);
                m_recv_buf = m_recv_buf.substr(4 + msg_len);
            }
        }
    }
    virtual uint32_t read_virt(uint8_t *buf, uint32_t len) override
    {
        if (m_bufferd_recv_data.empty())
        {
            read_more();
        }
        auto copy_len = std::min(len, (uint32_t)m_bufferd_recv_data.size());
        m_bufferd_recv_data.copy((char *)buf, copy_len);
        m_bufferd_recv_data = m_bufferd_recv_data.substr(copy_len);

        return copy_len;
    }
    virtual void write_virt(const uint8_t *buf, uint32_t len) override
    {
        std::string send_buf;
        uint32_t msg_len = htonl(len);
        send_buf.append((char *)&msg_len, 4);
        send_buf.append((char *)buf, len);
        m_transport->write((uint8_t *)send_buf.data(), send_buf.size());
    }
    virtual void flush() override
    {
        m_transport->flush();
    }
};
class AD_RPC_EVENT_NODE : public AD_EVENT_SC_TCP_LISTEN_NODE
{
    std::map<std::string, std::shared_ptr<apache::thrift::TProcessor>> m_processor_map;
    AD_EVENT_SC_TCP_DATA_NODE_PTR create_rpc_tcp_data_node(int fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR listen_node);

public:
    AD_RPC_EVENT_NODE(
        unsigned short _port,
        AD_EVENT_SC_PTR _event_sc) : AD_EVENT_SC_TCP_LISTEN_NODE(_port, [this](int _fd, AD_EVENT_SC_TCP_LISTEN_NODE_PTR _listen_node)
                                                                 { return create_rpc_tcp_data_node(_fd, _listen_node); }, _event_sc)
    {
    }
    void add_processor(const std::string &service_name, std::shared_ptr<apache::thrift::TProcessor> processor)
    {
        m_processor_map[service_name] = processor;
    }
};
class my_co_trans : public apache::thrift::transport::TSocket
{
    AD_EVENT_SC_PTR m_event_sc;
public:
    my_co_trans(const std::string &host, int port, AD_EVENT_SC_PTR _sc) : apache::thrift::transport::TSocket(host, port),m_event_sc(_sc)
    {
    }
    virtual uint32_t read(uint8_t *buf, uint32_t len) override
    {
        if (!this->hasPendingDataToRead())
        {
            m_event_sc->yield_by_fd(getSocketFD());
        }
        return apache::thrift::transport::TSocket::read(buf, len);
    }
};
class AD_RPC_SC : public AD_EVENT_SC
{
    std::shared_ptr<AD_RPC_EVENT_NODE> m_rpc_listen_node;
    using AD_EVENT_SC::AD_EVENT_SC;
    static std::shared_ptr<AD_RPC_SC> m_single;
    AD_LOGGER m_logger = AD_LOGGER("rpc");
public:
    static std::shared_ptr<AD_RPC_SC> get_instance()
    {
        if (!m_single)
        {
            m_single = std::make_shared<AD_RPC_SC>();
        }
        return m_single;
    }
    template <typename TH>
    void call_remote(unsigned short _port, const std::string &_service, std::function<void(TH &)> _func)
    {
        auto client_socket_trans = std::make_shared<my_co_trans>("localhost", _port, std::static_pointer_cast<AD_EVENT_SC>(shared_from_this()));
        auto client_buffer_trans = std::make_shared<apache::thrift::transport::TBufferedTransport>(client_socket_trans);
        auto client_ad_trans = std::make_shared<AD_RPC_TRANSPORT>(client_buffer_trans);
        auto client_protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(client_ad_trans);
        auto mt_protocol = std::make_shared<apache::thrift::protocol::TMultiplexedProtocol>(client_protocol, _service);
        TH client(mt_protocol);
        try
        {
            client_buffer_trans->open();
            try
            {
                _func(client);
            }
            catch (const std::exception &e)
            {
                m_logger.log(AD_LOGGER::ERROR, "call_remote error:%s", e.what());
            }

            client_buffer_trans->close();
        }
        catch (const std::exception &e)
        {
            m_logger.log(AD_LOGGER::ERROR, "call_remote error:%s", e.what());
        }
    }
    void enable_rpc_server(unsigned short _port)
    {
        if (!m_rpc_listen_node)
        {
            m_rpc_listen_node = std::make_shared<AD_RPC_EVENT_NODE>(_port, std::static_pointer_cast<AD_EVENT_SC>(shared_from_this()));
            registerNode(m_rpc_listen_node);
        }
    }
    void disable_rpc_server()
    {
        if (m_rpc_listen_node)
        {
            unregisterNode(m_rpc_listen_node);
            m_rpc_listen_node.reset();
        }
    }
    void add_rpc_server(const std::string &_service, std::shared_ptr<apache::thrift::TProcessor> processor)
    {
        if (m_rpc_listen_node)
        {
            m_rpc_listen_node->add_processor(_service, processor);
        }
    }
    void start_co_record();
    void start_server()
    {
        signal(SIGPIPE, SIG_IGN);
        runEventLoop();
    }
    void stop_server()
    {
        disable_rpc_server();
        stopEventLoop();
    }
    unsigned short get_listen_port()
    {
        if (m_rpc_listen_node)
        {
            return m_rpc_listen_node->getPort();
        }
        return 0;
    }
};
std::vector<device_config> ad_rpc_get_run_dev();
u16 ad_rpc_get_specific_dev_port(const std::string &dev_name);
std::vector<sm_config> ad_rpc_get_run_sm();
std::string ad_rpc_device_save_ply(const std::string &dev_name);
void ad_rpc_gate_ctrl(const std::string &dev_name, bool _is_open);
std::string ad_rpc_get_current_state();
void ad_rpc_update_current_state();

#endif // _AD_RPC_H_
