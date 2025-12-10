#if !defined(_MODBUS_DRIVER_H_)
#define _MODBUS_DRIVER_H_
#include <string>
#include <map>
#include <modbus/modbus.h>
#include <thread>
#include <mutex>
#include "../../rpc/ad_rpc.h"
struct float_addr_pair{
    int addr;
    float value;
};
struct coil_addr_pair{
    int addr;
    bool value;
};
class modbus_driver {
    std::map<std::string, float_addr_pair> m_float32_abcd_meta;
    std::map<std::string, coil_addr_pair> m_coil_meta;
    modbus_t *m_ctx;
    AD_LOGGER m_logger;
    bool m_is_working = false;
    bool exception_occurred = false;
    std::mutex m_mutex;
public:
    modbus_driver(const std::string &_ip, unsigned short _port, int _slave_id);
    ~modbus_driver();
    void add_float32_abcd_meta(const std::string &_name, int addr);
    void add_coil_meta(const std::string &_name, int addr);
    float read_float32_abcd(const std::string &_name);
    void write_coil(const std::string &_name, bool _value);
};

#endif // _MODBUS_DRIVER_H_
