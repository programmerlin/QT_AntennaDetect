# QT_AntennaDetect - 新版架构

## 项目结构

```
QT_AntennaDetect/
├── QT_AntennaDetect.pro  (项目文件)
├── include/              (存放所有头文件)
│   ├── widget.h         (主窗口头文件)
│   └── camera_utils.h   (相机工具头文件)
├── src/                  (存放所有源文件)
│   ├── main.cpp         (主程序入口)
│   ├── widget.cpp       (主窗口实现)
│   └── camera_utils.cpp  (相机工具实现)
├── ui/                   (界面文件)
│   └── widget.ui        (Qt界面设计文件)
├── lib/                 (静态库文件)
│   ├── libUSB_HikCamera_lib.a
│   └── libdma_buffer_pool_lib.a
└── compile_new_arch.sh  (编译脚本)
```

## 库文件说明

### 缺失库文件的作用

1. **libThreadSafeQueue.a**
   - 作用：线程安全队列实现
   - 状态：无需单独库文件（模板类，在代码中实现）
   - 位置：`USB_HikCamera/include/ThreadSafeQueue.h`

2. **libUSB_HikCamera_lib.a**
   - 作用：海康威视相机SDK的静态封装库
   - 功能：相机的初始化、配置、图像获取等
   - 来源：从 `Antenna_Visioner/build/build_USB_HikCamera_lib/` 复制

3. **libdma_buffer_pool_lib.a**
   - 作用：DMA内存池管理库
   - 功能：管理RK3568的DMA内存，用于零拷贝图像传输
   - 来源：从 `Antenna_Visioner/build/build_USB_HikCamera_lib/dma_build/` 复制

## 主要修改

### 1. 文件结构调整
- 将所有源文件移至 `src/` 目录
- 将所有头文件移至 `include/` 目录  
- 将UI文件移至 `ui/` 目录
- 创建 `lib/` 目录存放静态库文件

### 2. 新增相机管理器
- `CameraManager` 类封装了相机操作
- 提供统一的相机接口
- 支持信号槽机制

### 3. 项目配置更新
- 更新了所有包含路径
- 添加了静态库文件引用
- 简化了依赖关系

## 编译步骤

1. 确保依赖库已就绪
2. 运行编译脚本：
   ```bash
   chmod +x compile_new_arch.sh
   ./compile_new_arch.sh
   ```

3. 编译成功后执行：
   ```bash
   ./QT_AntennaDetect
   ```

## 关键特性

### 性能优化
- 使用DMA内存零拷贝
- RGA硬件加速图像处理
- 线程安全的相机操作

### 模块化设计
- 相机功能独立封装
- 清晰的接口定义
- 易于扩展和维护

### 错误处理
- 完善的异常捕获
- 信号通知机制
- 状态检查和验证