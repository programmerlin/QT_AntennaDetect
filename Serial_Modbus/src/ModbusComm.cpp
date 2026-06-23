#include "../include/ModbusComm.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <thread>
#include <chrono>

namespace modbus {

// ============================================================
//  构造 / 析构
// ============================================================

ModbusComm::ModbusComm(const std::string& device, int baud)
    : fd_(-1)
    , device_(device)
    , baud_(baud)
{}

ModbusComm::~ModbusComm()
{
    close();
}

// ============================================================
//  串口打开 / 关闭
// ============================================================

bool ModbusComm::open()
{
    if (fd_ >= 0) {
        return true;  // 已打开
    }

    // O_RDWR: 读写; O_NOCTTY: 不作为控制终端; O_NDELAY: 非阻塞
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        fprintf(stderr, "[ModbusComm] 无法打开串口 %s: %s\n",
                device_.c_str(), strerror(errno));
        return false;
    }

    // 清除非阻塞标志, 设为阻塞模式
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }

    if (!configureSerial()) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    printf("[ModbusComm] 串口已打开: %s @ %d baud (8-N-1)\n",
           device_.c_str(), baud_);
    return true;
}

void ModbusComm::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        printf("[ModbusComm] 串口已关闭: %s\n", device_.c_str());
    }
}

// ============================================================
//  串口参数配置 (9600-8-N-1, raw mode)
// ============================================================

bool ModbusComm::configureSerial()
{
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        fprintf(stderr, "[ModbusComm] tcgetattr 失败: %s\n", strerror(errno));
        return false;
    }

    // ======== 波特率 ========
    speed_t speed = B9600;
    switch (baud_) {
        case 2400:   speed = B2400;   break;
        case 4800:   speed = B4800;   break;
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:
            fprintf(stderr, "[ModbusComm] 不支持的波特率: %d, 使用 9600\n", baud_);
            speed = B9600;
            break;
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // ======== 数据位 8 + 无校验 + 1 停止位 ========
    tty.c_cflag &= ~PARENB;        // 无校验
    tty.c_cflag &= ~CSTOPB;        // 1 停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;            // 8 数据位

    tty.c_cflag &= ~CRTSCTS;       // 无硬件流控
    tty.c_cflag |= CREAD | CLOCAL; // 使能接收 + 忽略调制解调器控制线

    // ======== 本地模式: raw (无回显, 无信号, 不规范处理) ========
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // ======== 输入模式 ========
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);       // 无软件流控
    tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);       // 无 CR/LF 转换

    // ======== 输出模式: raw ========
    tty.c_oflag &= ~OPOST;

    // ======== 超时: 0.5s 字节间, 1.0s 总超时 ========
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;   // 0.5s (单位 0.1s)

    // 写入
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[ModbusComm] tcsetattr 失败: %s\n", strerror(errno));
        return false;
    }

    // 清空缓冲区
    tcflush(fd_, TCIOFLUSH);

    return true;
}

// ============================================================
//  CRC-16 (Modbus RTU)
//  ------------------------------------------------------------
//  多项式: 0xA001 (反转的 0x8005)
//  初始值: 0xFFFF
//  输出:   LSB first (低字节在前)
// ============================================================

uint16_t ModbusComm::crc16(const uint8_t* buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ============================================================
//  构建并发送 Modbus RTU 写寄存器帧
//  ------------------------------------------------------------
//  帧格式 (8 字节):
//    [从站地址] [功能码 0x06] [寄存器地址H] [寄存器地址L]
//    [数据H] [数据L] [CRC L] [CRC H]
// ============================================================

bool ModbusComm::sendWriteFrame(uint8_t slaveAddr, uint16_t regAddr, uint16_t value)
{
    if (fd_ < 0) {
        fprintf(stderr, "[ModbusComm] 串口未打开\n");
        return false;
    }

    uint8_t frame[8];

    frame[0] = slaveAddr;
    frame[1] = FUNC_WRITE;
    frame[2] = (regAddr >> 8) & 0xFF;   // 寄存器地址高字节
    frame[3] = regAddr & 0xFF;           // 寄存器地址低字节
    frame[4] = (value >> 8) & 0xFF;      // 数据高字节
    frame[5] = value & 0xFF;             // 数据低字节

    uint16_t crc = crc16(frame, 6);
    frame[6] = crc & 0xFF;               // CRC 低字节
    frame[7] = (crc >> 8) & 0xFF;        // CRC 高字节

    // 发送
    ssize_t written = ::write(fd_, frame, 8);
    if (written != 8) {
        fprintf(stderr, "[ModbusComm] 发送失败: 期望 8 字节, 实际 %zd 字节 (%s)\n",
                written, (written < 0 ? strerror(errno) : "short write"));
        return false;
    }

    // 等待数据真正从硬件发出
    tcdrain(fd_);

    // 调试: 打印发送的帧
    printf("[ModbusComm] TX [%02X %02X %02X %02X %02X %02X %02X %02X] "
           "addr=0x%04X val=%d (%.2f V)\n",
           frame[0], frame[1], frame[2], frame[3],
           frame[4], frame[5], frame[6], frame[7],
           regAddr, value, value / VOLTAGE_SCALE);

    return true;
}

// ============================================================
//  公共接口: 写入电压
// ============================================================

bool ModbusComm::writeVoltage(int channel, float voltage)
{
    if (channel < 0 || channel > 3) {
        fprintf(stderr, "[ModbusComm] 无效通道: %d (有效范围 0~3)\n", channel);
        return false;
    }

    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > 14.0f) voltage = 14.0f;

    // 电压值编码: V × 100 (与 STM32 端 uint16_t val = voltage * 100 一致)
    uint16_t val = (uint16_t)(voltage * VOLTAGE_SCALE + 0.5f);  // 四舍五入

    return sendWriteFrame(SLAVE_ADDR, REG_ADDR[channel], val);
}

bool ModbusComm::writeAllVoltages(float v1, float v2, float v3, float v4)
{
    bool ok = true;
    float voltages[4] = {v1, v2, v3, v4};

    for (int i = 0; i < 4; i++) {
        if (!writeVoltage(i, voltages[i])) {
            fprintf(stderr, "[ModbusComm] CH%d 发送失败\n", i + 1);
            ok = false;
        }

        // 通道间延迟 (给电源模块时间完成电压设置)
        if (i < 3) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(CHANNEL_INTERVAL_MS));
        }
    }

    return ok;
}

}  // namespace modbus
