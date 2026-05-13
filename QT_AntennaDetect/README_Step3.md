# QT_AntennaDetect - Step3: YOLO检测功能集成

## 功能概述

本步骤成功集成了 Antenna_Visioner 模块，实现了 YOLO 天线检测功能。系统现在支持：

### 新增功能
1. **YOLO模型加载**
   - 支持 RKNN 格式的 YOLO 模型
   - 自动加载对应的标签文件
   - 模型路径验证和错误提示

2. **YOLO检测功能**
   - 单张图片的YOLO检测
   - 实时视频流的YOLO检测（30fps）
   - 检测结果可视化（边界框、标签、置信度）

3. **相机集成**
   - 海康威视USB相机支持
   - DMA内存管理
   - 实时帧处理

### 保留功能
- OpenCV图像处理（灰度、模糊、边缘检测、亮度/对比度调整）
- 图片加载和保存
- 实时图像显示

## 文件说明

### 主要文件
- `QT_AntennaDetect_Step3.pro` - 第三步项目配置文件
- `widget.cpp` - 集成YOLO检测的主窗口实现
- `widget.h` - 更新的头文件声明
- `widget.ui` - 添加YOLO控件的用户界面

### 核心组件
- `HikCamera` - 海康威视相机接口
- `Antenna_Visioner` - YOLO检测核心
- `DetectionResult` - 检测结果结构

## 使用方法

### 1. 编译
```bash
chmod +x compile_step3.sh
./compile_step3.sh
```

### 2. 运行
```bash
./QT_AntennaDetect_Step3
```

### 3. 使用步骤
1. 点击"加载模型"按钮
   - 选择 YOLO 模型文件（.rknn）
   - 选择标签文件（.txt）

2. 加载图片或启动相机
   - "加载图片" - 选择本地图片文件
   - "开始检测" - 启动USB相机

3. 执行检测
   - 点击"YOLO检测"进行单次检测
   - 或启动相机后自动进行实时检测

## 关键代码实现

### 模型加载
```cpp
bool Widget::loadYOLOModel() {
    // 创建 Antenna_Visioner 实例
    visioner = new Antenna_Visioner(
        modelPath.toStdString(),
        labelPath.toStdString(),
        640, 640,  // 输入尺寸
        camera
    );
    
    // 初始化系统
    return visioner->init_system();
}
```

### 检测执行
```cpp
void Widget::on_yoloDetectBtn_clicked() {
    // 获取DMA内存中的图像
    cv::Mat& dmaImage = visioner->get_rga_mat();
    
    // 执行检测
    if (visioner->detect_once(dmaImage, detectionResults)) {
        // 绘制检测结果
        drawDetections(dmaImage);
        // 更新显示
        displayImage(dmaImage);
        displayYOLOResults();
    }
}
```

### 结果可视化
```cpp
void Widget::drawDetections(cv::Mat& image) {
    for (const auto& result : detectionResults) {
        // 绘制边界框
        cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), color, 2);
        // 绘制标签
        cv::putText(image, label, cv::Point(x1, y1), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}
```

## 配置要求

### 模型文件
- 模型路径: `./yolov8_model/best.rknn`
- 标签路径: `./yolov8_model/antenna.txt`

### 依赖库
- RKNN API
- RGA (Rockchip Graphic Acceleration)
- OpenCV
- 海康威视相机 SDK

## 性能优化
- 使用 DMA 内存减少图像拷贝
- RGA 硬件加速图像处理
- 定时器控制帧率（约30fps）

## 下一步计划
1. 性能优化和内存管理改进
2. 错误处理和异常情况完善
3. 用户界面优化
4. 系统稳定性测试