#include "widget.h"
#include "ui_widget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    imageLoaded(false),
    brightnessValue(0),
    contrastValue(0)
{
    ui->setupUi(this);

    // 设置初始状态
    ui->stopBtn->setEnabled(false);
    ui->saveImageBtn->setEnabled(false);
    ui->processImageBtn->setEnabled(false);

    // 连接按钮信号
    connect(ui->startBtn, &QPushButton::clicked, this, &Widget::on_startBtn_clicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &Widget::on_stopBtn_clicked);
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &Widget::on_loadImageBtn_clicked);
    connect(ui->saveImageBtn, &QPushButton::clicked, this, &Widget::on_saveImageBtn_clicked);
    connect(ui->processImageBtn, &QPushButton::clicked, this, &Widget::on_processImageBtn_clicked);

    // 连接图像处理控件
    connect(ui->grayscaleCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->blurCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->edgeCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->brightnessSlider, &QSlider::valueChanged, this, &Widget::adjustBrightness);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &Widget::adjustContrast);

    // 设置滑块范围
    ui->brightnessSlider->setRange(-100, 100);
    ui->contrastSlider->setRange(-100, 100);

    // 设置初始状态文本
    ui->statusLabel->setText("就绪 - 请加载图片");
    ui->resultsTextEdit->setText("OpenCV 图像处理功能已启用\n支持以下操作：\n• 加载图片\n• 灰度转换\n• 模糊处理\n• 边缘检测\n• 亮度/对比度调整\n• 保存处理后的图片");
}

Widget::~Widget()
{
    delete ui;
}

bool Widget::loadImage(const QString& filePath)
{
    // 使用 OpenCV 读取图片
    originalImage = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);

    if (originalImage.empty()) {
        QMessageBox::warning(this, "警告", "无法读取图片！");
        return false;
    }

    // 保存原图副本
    processedImage = originalImage.clone();
    imageLoaded = true;

    // 显示图片
    displayImage(processedImage);

    // 更新UI
    updateButtonsState();
    updateImageInfo();

    ui->statusLabel->setText(QString("已加载图片: %1").arg(QFileInfo(filePath).fileName()));

    return true;
}

void Widget::displayImage(const cv::Mat& image)
{
    if (image.empty()) return;

    // 转换为 Qt 格式
    cv::Mat rgbImage;
    if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    } else {
        rgbImage = image;
    }

    QImage qImage(
        rgbImage.data,
        rgbImage.cols,
        rgbImage.rows,
        rgbImage.step,
        QImage::Format_RGB888
    );

    QPixmap pixmap = QPixmap::fromImage(qImage);

    // 缩放图片以适应显示区域
    QPixmap scaledPixmap = pixmap.scaled(
        ui->imageLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    ui->imageLabel->setPixmap(scaledPixmap);
}

void Widget::updateImageInfo()
{
    if (!imageLoaded) return;

    QString info = QString("图像信息：\n");
    info += QString("尺寸: %1 x %2\n").arg(originalImage.cols).arg(originalImage.rows);
    info += QString("通道: %1\n").arg(originalImage.channels());
    info += QString("深度: %1\n").arg(originalImage.depth());
    info += QString("格式: %1\n").arg(originalImage.type());

    ui->resultsTextEdit->setText(info);
}

void Widget::updateButtonsState()
{
    bool hasImage = imageLoaded;
    ui->saveImageBtn->setEnabled(hasImage);
    ui->processImageBtn->setEnabled(hasImage);
    ui->grayscaleCheck->setEnabled(hasImage);
    ui->blurCheck->setEnabled(hasImage);
    ui->edgeCheck->setEnabled(hasImage);
    ui->brightnessSlider->setEnabled(hasImage);
    ui->contrastSlider->setEnabled(hasImage);
}

void Widget::showProcessingTime()
{
    static QElapsedTimer timer;
    static bool started = false;

    if (!started) {
        timer.start();
        started = true;
    } else {
        qint64 elapsed = timer.elapsed();
        ui->statusLabel->setText(QString("处理完成，耗时: %1 ms").arg(elapsed));
        started = false;
    }
}

void Widget::on_startBtn_clicked()
{
    ui->statusLabel->setText("实时检测功能待实现 - 需要添加 YOLO 和相机支持");
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->loadImageBtn->setEnabled(false);
}

void Widget::on_stopBtn_clicked()
{
    ui->statusLabel->setText("已停止");
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->loadImageBtn->setEnabled(true);
}

void Widget::on_loadImageBtn_clicked()
{
    // 选择图片文件
    QString imagePath = QFileDialog::getOpenFileName(
        this,
        "选择图片",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff)"
    );

    if (imagePath.isEmpty()) {
        return;
    }

    // 使用 OpenCV 加载图片
    if (loadImage(imagePath)) {
        ui->resultsTextEdit->setText("图片加载成功！\n可以使用右侧的控制面板进行图像处理。\n\n支持的格式：JPG, JPEG, PNG, BMP, TIFF");
    }
}

void Widget::on_saveImageBtn_clicked()
{
    if (!imageLoaded) return;

    // 选择保存路径
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "保存图片",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp)"
    );

    if (savePath.isEmpty()) {
        return;
    }

    // 根据扩展名确定保存格式
    QString ext = QFileInfo(savePath).suffix().toLower();
    int format;

    if (ext == "jpg" || ext == "jpeg") {
        format = cv::IMWRITE_JPEG_QUALITY;
    } else if (ext == "png") {
        format = cv::IMWRITE_PNG_COMPRESSION;
    } else if (ext == "bmp") {
        format = -1;
    } else {
        format = cv::IMWRITE_JPEG_QUALITY;
        savePath += ".jpg";
    }

    // 保存图片
    bool success = cv::imwrite(savePath.toStdString(), processedImage);

    if (success) {
        ui->statusLabel->setText(QString("图片已保存: %1").arg(QFileInfo(savePath).fileName()));
        QMessageBox::information(this, "成功", "图片保存成功！");
    } else {
        QMessageBox::warning(this, "警告", "图片保存失败！");
    }
}

void Widget::on_processImageBtn_clicked()
{
    if (!imageLoaded) return;

    // 重置为原图
    processedImage = originalImage.clone();

    // 应用所有选中的滤镜
    applyFilters();

    ui->statusLabel->setText("图像处理完成");
}

void Widget::applyGrayscale()
{
    if (!imageLoaded) return;

    if (ui->grayscaleCheck->isChecked()) {
        cv::cvtColor(processedImage, processedImage, cv::COLOR_BGR2GRAY);
        cv::cvtColor(processedImage, processedImage, cv::COLOR_GRAY2BGR);
    }

    displayImage(processedImage);
}

void Widget::applyBlur()
{
    if (!imageLoaded) return;

    if (ui->blurCheck->isChecked()) {
        cv::GaussianBlur(processedImage, processedImage, cv::Size(5, 5), 0);
    }

    displayImage(processedImage);
}

void Widget::applyEdgeDetection()
{
    if (!imageLoaded) return;

    if (ui->edgeCheck->isChecked()) {
        cv::Mat gray;
        cv::cvtColor(processedImage, gray, cv::COLOR_BGR2GRAY);

        cv::Mat edges;
        cv::Canny(gray, edges, 50, 150);

        // 将边缘转换为彩色
        cv::cvtColor(edges, processedImage, cv::COLOR_GRAY2BGR);
    }

    displayImage(processedImage);
}

void Widget::adjustBrightness(int value)
{
    brightnessValue = value;
    applyFilters();
}

void Widget::adjustContrast(int value)
{
    contrastValue = value;
    applyFilters();
}

void Widget::applyFilters()
{
    if (!imageLoaded) return;

    // 重置为原图
    processedImage = originalImage.clone();

    // 应用灰度
    applyGrayscale();

    // 应用模糊
    applyBlur();

    // 应用边缘检测
    applyEdgeDetection();

    // 应用亮度和对比度调整
    if (brightnessValue != 0 || contrastValue != 0) {
        double alpha = 1.0 + (double)contrastValue / 100.0;
        int beta = brightnessValue;

        cv::Mat adjusted = processedImage.clone();
        processedImage.convertTo(processedImage, -1, alpha, beta);
    }

    // 显示处理后的图像
    displayImage(processedImage);

    // 显示处理时间
    showProcessingTime();
}

cv::Mat Widget::convertToQtFormat(const cv::Mat& image)
{
    if (image.empty()) return image;

    cv::Mat rgbImage;
    if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    } else {
        rgbImage = image;
    }

    return rgbImage;
}