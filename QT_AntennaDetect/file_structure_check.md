# 文件架构修改清单

## 新增文件 ✅

### 目录结构
- [x] 创建 `include/` 目录
- [x] 创建 `src/` 目录
- [x] 创建 `ui/` 目录
- [x] 创建 `lib/` 目录

### 头文件
- [x] `include/widget.h` - 从原目录移动
- [x] `include/camera_utils.h` - 新增相机管理器头文件

### 源文件
- [x] `src/main.cpp` - 从原目录移动
- [x] `src/widget.cpp` - 从原目录移动
- [x] `src/camera_utils.cpp` - 新增相机工具实现

### 配置文件
- [x] `QT_AntennaDetect.pro` - 新的项目配置文件
- [x] `compile_new_arch.sh` - 新的编译脚本
- [x] `README_NewArchitecture.md` - 架构说明文档

### 库文件
- [x] `lib/libUSB_HikCamera_lib.a` - 复制自 build 目录
- [x] `lib/libdma_buffer_pool_lib.a` - 复制自 build 目录

## 修改内容 ✅

### 1. DetectionResult 结构体
- [x] 在 `Antenna_Visioner.h` 中添加 `box[4]` 成员
- [x] 在 `Antenna_visioner.cpp` 中保存边界框信息

### 2. 编译错误修复
- [x] 修复 `im2d.h` 缺失问题（更新包含路径）
- [x] 修复 `show_img` 变量作用域问题
- [x] 修复类型比较警告（uint32_t）
- [x] 修复语法错误（`#include` 误写为 `include`）

### 3. 项目配置
- [x] 更新所有包含路径
- [x] 添加静态库引用
- [x] 移除不存在的库引用（如 ThreadSafeQueue）

### 4. 文件路径更新
- [x] 更新源文件中的包含路径
- [x] 确保所有路径使用相对路径

## 验证项目 ✅

### 编译测试
- [ ] 基础编译测试
- [ ] OpenCV 集成测试
- [ ] Antenna_Visioner 集成测试
- [ ] 相机功能集成测试

### 功能测试
- [ ] 模型加载功能
- [ ] 单图检测功能
- [ ] 实时检测功能
- [ ] 结果显示功能

## 下一步计划

### 性能优化
- [ ] 优化 DMA 内存使用
- [ ] 实现 RGA 加速
- [ ] 减少图像拷贝

### 错误处理
- [ ] 完善异常处理机制
- [ ] 添加重连逻辑
- [ ] 实现状态监控

### 用户体验
- [ ] 优化界面布局
- [ ] 添加进度显示
- [ ] 实现参数调整