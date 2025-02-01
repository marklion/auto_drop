#if !defined(_AD_RPC_H_)
#define _AD_RPC_H_
#include <thrift/TProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include "../public/event_sc/ad_event_sc.h"
#include "gen_code/cpp/idl_types.h"

class AD_RPC_TRANSPORT : public apache::thrift::transport::TTransport
{
    std::string m_recv_buf;
    std::shared_ptr<apache::thrift::transport::TTransport> m_transport;

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
    virtual uint32_t read_virt(uint8_t *buf, uint32_t len) override
    {
        uint32_t read_len = 0;
        unsigned char buffer[1024] = {0};
        auto recv_len = m_transport->read(buffer, sizeof(buffer));
        m_recv_buf.append((char *)buffer, recv_len);
        // 1. 检查是否足够读取长度前缀（4字节）
        if (m_recv_buf.size() >= 4)
        {
            // 2. 提取大端序长度并转换为主机字节序
            uint32_t msg_len;
            memcpy(&msg_len, m_recv_buf.data(), 4);
            msg_len = ntohl(msg_len); // 关键：网络字节序转主机序

            // 3. 检查是否包含完整消息体
            if (m_recv_buf.size() >= 4 + msg_len)
            {
                memcpy(buf, m_recv_buf.data()+4, msg_len);
                read_len = msg_len;
                // 5. 从缓冲区移除已处理数据
                m_recv_buf.erase(0, 4 + msg_len);
            }
        }

        return read_len;
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

#endif // _AD_RPC_H_
