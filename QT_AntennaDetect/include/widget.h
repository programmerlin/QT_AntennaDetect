#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTimer>
#include <QString>
#include <QPixmap>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

// OpenCV 头文件
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

// Antenna_Visioner 头文件
#include "../Antenna_Visioner/include/Antenna_Visioner.h"
#include "../USB_HikCamera/include/HikCamera.h"

// ResMLP 残差多层感知机 (CPU 推理)
#include "ResMLP.h"

namespace Ui {
class Widget;
}

class Widget : public QWidget   //继承自 QWidget（QT中自带的写好的窗口类）的类Widget
{
    Q_OBJECT    //宏定义，启用QT的信号与槽机制

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    // 原有功能保留
    void on_startBtn_clicked();
    void on_stopBtn_clicked();
    void on_loadImageBtn_clicked();
    void on_saveImageBtn_clicked();
    void on_processImageBtn_clicked();

    // YOLO检测相关槽函数
    void on_yoloDetectBtn_clicked();
    void on_loadModelBtn_clicked();

    // 相机处理槽函数
    void processCameraFrame();

    // 图像处理相关槽函数
    void applyGrayscale();
    void applyBlur();
    void applyEdgeDetection();
    void applyFilters();
    void adjustBrightness(int value);
    void adjustContrast(int value);

    // 滑块数值显示更新
    void updateBrightnessValue(int value);
    void updateContrastValue(int value);

private:
    Ui::Widget *ui;

protected:
    // 重写resizeEvent以处理窗口大小变化
    void resizeEvent(QResizeEvent *event) override;

    // OpenCV 相关成员
    cv::Mat originalImage;    // 原始图像
    cv::Mat processedImage;   // 处理后的图像
    bool imageLoaded;         // 图像是否已加载标志

    // 图像处理参数 (必须声明在 YOLO 成员之前, 以匹配初始化列表顺序)
    int brightnessValue;
    int contrastValue;

    // YOLO检测相关成员
    HikCamera *camera;              // 相机对象
    Antenna_Visioner *visioner;     // 天线检测器
    QTimer *cameraTimer;            // 相机定时器
    std::vector<DetectionResult> detectionResults; // 检测结果
    bool isDetecting;              // 是否正在检测
    bool modelLoaded;              // 模型是否已加载
    QString modelPath;             // 模型文件路径
    QString labelPath;             // 标签文件路径

    // 初始化方法
    void initComponents();
    bool loadYOLOModel();

    // YOLO检测方法
    void performYOLODetection(const cv::Mat& image);
    void displayYOLOResults(int srcW, int srcH);
    void drawDetections(cv::Mat& image);

    // ResMLP 推理: 从真实世界坐标计算电压
    bool computeVoltages(float worldX, float worldY, float voltages[4]);

    // 真实世界坐标转换
    cv::Mat homographyMatrix_;           // 透视变换矩阵 (3x3)
    static constexpr int CAM_NATIVE_WIDTH = 2048;
    static constexpr int CAM_NATIVE_HEIGHT = 1230;
    static constexpr int MODEL_INPUT_SIZE = 640;
    bool initCoordinateTransform();
    bool pixelToRealWorld(float pixelX, float pixelY, int srcW, int srcH, float& worldX, float& worldY);

    // ResMLP 残差 MLP 推理
    resmlp::ResMLP resMLP_;              // 残差 MLP 模型实例
    bool resMLPLoaded_;                  // 模型是否加载成功

    // 图像处理方法
    bool loadImage(const QString& filePath);
    void displayImage(const cv::Mat& image);
    void updateImageInfo();
    cv::Mat convertToQtFormat(const cv::Mat& image);

    // 相机方法
    void startCamera();
    void stopCamera();

    // UI 更新方法
    void updateButtonsState();
    void showProcessingTime();
    void updateStatus(const QString& status);
};

#endif // WIDGET_H