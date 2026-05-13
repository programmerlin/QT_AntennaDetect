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

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

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
    void displayYOLOResults();
    void drawDetections(cv::Mat& image);

    // 图像处理方法
    bool loadImage(const QString& filePath);
    void displayImage(const cv::Mat& image);
    void updateImageInfo();
    void applyFilters();
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