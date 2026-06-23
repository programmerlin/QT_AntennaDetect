#ifndef MODBUS_COMM_H
#define MODBUS_COMM_H

#include <string>
#include <cstdint>

namespace modbus {

// ============================================================
//  协议常量 (与 STM32 下位机 bsp_power_supply.c 一致)
// ============================================================

/// Modbus RTU 从站地址
static const uint8_t SLAVE_ADDR     = 0x01;

/// 功能码: 写单个保持寄存器
static const uint8_t FUNC_WRITE     = 0x06;

/// 4 路电压寄存器地址
static const uint16_t REG_ADDR[4] = {
    0x01FE,   // 通道 1
    0x02C6,   // 通道 2
    0x032A,   // 通道 3
    0x038E    // 通道 4
};

/// 串口参数
static const int DEFAULT_BAUD       = 9600;
static const int DATA_BITS          = 8;
static const int STOP_BITS          = 1;
static const char PARITY            = 'N';

/// 电压编码: 实际值 × 100 取整 (0.01V 精度)
/// 例: 8.19 V → 819
static const float VOLTAGE_SCALE   = 100.0f;

/// 通道间发送间隔 (ms), 与 STM32 vTaskDelay(2000) 一致
/// 电源模块设置电压后需约 200ms 稳定, 加余量用 500ms
static const int CHANNEL_INTERVAL_MS = 500;

// ============================================================
//  ModbusComm — 串口 Modbus RTU 通信
// ============================================================
//
//  帧格式 (8 字节):
//    [从站地址(1)] [功能码 0x06(1)] [寄存器地址(2)] [数据值(2)] [CRC16(2)]
//
//  数据值 = 电压(V) × 100, 例: 8.19V → 0x0333 (819)
//
//  用法:
//    ModbusComm modbus("/dev/ttyUSB0");
//    if (!modbus.open()) { ... error ... }
//    modbus.writeVoltage(0, 8.19f);  // CH1 = 8.19V
//    modbus.writeAllVoltages(8.19f, 11.17f, 2.02f, 5.06f);
//
// ============================================================

class ModbusComm {
public:
    /**
     * @brief 构造函数
     * @param device   串口设备路径, 如 "/dev/ttyUSB0" 或 "/dev/ttyS4"
     * @param baud     波特率, 默认 9600
     */
    explicit ModbusComm(const std::string& device, int baud = DEFAULT_BAUD);
    ~ModbusComm();

    /**
     * @brief 打开串口并配置参数 (9600-8-N-1)
     * @return true 成功, false 失败
     */
    bool open();

    /// 关闭串口
    void close();

    /// 串口是否已打开
    bool isOpen() const { return fd_ >= 0; }

    /**
     * @brief Modbus CRC-16 校验 (公开, 方便外部验证帧正确性)
     *        多项式: 0xA001, 初始值: 0xFFFF, LSB first
     */
    static uint16_t crc16(const uint8_t* buf, int len);

    /**
     * @brief 写入单路电压
     * @param channel  通道号 (0~3, 对应 CH1~CH4)
     * @param voltage  电压值 (V), 范围 0.00 ~ 14.00
     * @return true 成功, false 失败
     */
    bool writeVoltage(int channel, float voltage);

    /**
     * @brief 写入全部 4 路电压 (逐通道发送, 间隔 CHANNEL_INTERVAL_MS)
     * @param v1~v4  4 路电压值 (V)
     * @return true 全部成功, false 有任一失败
     */
    bool writeAllVoltages(float v1, float v2, float v3, float v4);

private:
    int fd_;                    // 串口文件描述符
    std::string device_;        // 设备路径
    int baud_;                  // 波特率

    /**
     * @brief 构建并发送一条 Modbus RTU 写寄存器帧
     * @param slaveAddr  从站地址
     * @param regAddr    寄存器地址
     * @param value      数据值 (已编码的 uint16)
     * @return true 发送成功, false 失败
     */
    bool sendWriteFrame(uint8_t slaveAddr, uint16_t regAddr, uint16_t value);

    /// 设置串口参数 (9600-8-N-1, raw mode)
    bool configureSerial();
};

}  // namespace modbus

#endif  // MODBUS_COMM_H
