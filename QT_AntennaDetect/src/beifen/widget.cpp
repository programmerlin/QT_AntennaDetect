#include "../include/widget.h"
#include "../ui_widget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QDir>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    imageLoaded(false),
    camera(nullptr),
    visioner(nullptr),
    cameraTimer(new QTimer(this)),
    isDetecting(false),
    modelLoaded(false),
    brightnessValue(0),
    contrastValue(0)
{
    ui->setupUi(this);

    // 设置初始状态
    ui->stopBtn->setEnabled(false);
    ui->saveImageBtn->setEnabled(false);
    ui->processImageBtn->setEnabled(false);
    ui->yoloDetectBtn->setEnabled(false);
    ui->loadModelBtn->setEnabled(true);

    // 连接原有按钮信号
    connect(ui->startBtn, &QPushButton::clicked, this, &Widget::on_startBtn_clicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &Widget::on_stopBtn_clicked);
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &Widget::on_loadImageBtn_clicked);
    connect(ui->saveImageBtn, &QPushButton::clicked, this, &Widget::on_saveImageBtn_clicked);
    connect(ui->processImageBtn, &QPushButton::clicked, this, &Widget::on_processImageBtn_clicked);

    // 连接YOLO检测相关信号
    connect(ui->yoloDetectBtn, &QPushButton::clicked, this, &Widget::on_yoloDetectBtn_clicked);
    connect(ui->loadModelBtn, &QPushButton::clicked, this, &Widget::on_loadModelBtn_clicked);

    // 连接相机定时器
    connect(cameraTimer, &QTimer::timeout, this, &Widget::processCameraFrame);

    // 连接图像处理控件
    connect(ui->grayscaleCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->blurCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->edgeCheck, &QCheckBox::toggled, this, &Widget::applyFilters);
    connect(ui->brightnessSlider, &QSlider::valueChanged, this, &Widget::adjustBrightness);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &Widget::adjustContrast);

    // 设置滑块范围
    ui->brightnessSlider->setRange(-100, 100);
    ui->contrastSlider->setRange(-100, 100);

    // 设置定时器间隔（约30fps）
    cameraTimer->setInterval(33);

    // 初始化组件
    // initComponents();

    // 设置初始状态文本
    ui->statusLabel->setText("就绪 - 请加载YOLO模型");
    ui->resultsTextEdit->setText("系统功能：\n"
                                "1. OpenCV图像处理\n"
                                "2. YOLO天线检测\n"
                                "3. USB相机实时采集\n\n"
                                "使用步骤：\n"
                                "1. 点击'加载模型'按钮\n"
                                "2. 选择YOLO模型文件\n"
                                "3. 加载图片或启动相机\n"
                                "4. 点击'YOLO检测'开始识别");

    // 确保 imageLabel 在 ScrollArea 中正确显示
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setMinimumSize(1, 1);  // 确保至少有1x1的大小
    ui->imageLabel->setScaledContents(false);  // 允许缩放而不拉伸内容
}

Widget::~Widget()
{
    // 停止相机
    stopCamera();

    // 释放组件
    if (camera) {
        delete camera;
    }
    if (visioner) {
        delete visioner;
    }

    delete ui;
    delete cameraTimer;
}

void Widget::resizeEvent(QResizeEvent *event)
{
    // 调用基类的resizeEvent
    QWidget::resizeEvent(event);

    // 如果有图像已加载，重新调整图像显示
    if (imageLoaded) {
        displayImage(processedImage);
    }
}

void Widget::initComponents()
{
    try {
        updateStatus("正在初始化系统组件...");

        // 1. 创建相机对象（索引0）
        camera = new HikCamera(0);

        // 2. 设置模型路径（默认路径）
        modelPath = "./yolov8_model/best.rknn";
        labelPath = "./yolov8_model/antenna.txt";

        // 检查模型文件是否存在
        if (!QFile::exists(modelPath) || !QFile::exists(labelPath)) {
            QMessageBox::warning(this, "警告",
                QString("模型文件可能不存在：\n模型: %1\n标签: %2\n请检查路径或使用'加载模型'按钮重新指定").arg(modelPath).arg(labelPath));
        }

        updateStatus("系统组件初始化完成");

    } catch (const std::exception& e) {
        QString errorMsg = QString("初始化失败: %1").arg(e.what());
        QMessageBox::critical(this, "错误", errorMsg);
        updateStatus("系统初始化失败");
    }
}

bool Widget::loadYOLOModel()
{
    if (!camera) {
        QMessageBox::warning(this, "警告", "相机未初始化！");
        return false;
    }

    try {
        // 选择模型文件（如果默认不存在）
        if (!QFile::exists(modelPath)) {
            QString selectedFile = QFileDialog::getOpenFileName(
                this,
                "选择YOLO模型文件",
                "",
                "RKNN模型文件 (*.rknn)"
            );

            if (selectedFile.isEmpty()) {
                return false;
            }
            modelPath = selectedFile;
        }

        // 选择标签文件
        if (!QFile::exists(labelPath)) {
            QString selectedFile = QFileDialog::getOpenFileName(
                this,
                "选择标签文件",
                "",
                "标签文件 (*.txt)"
            );

            if (selectedFile.isEmpty()) {
                return false;
            }
            labelPath = selectedFile;
        }

        // 3. 创建天线检测器
        visioner = new Antenna_Visioner(
            modelPath.toStdString(),
            labelPath.toStdString(),
            640,  // YOLO 输入宽度
            640,  // YOLO 输入高度
            camera
        );

        // 4. 初始化检测系统
        if (!visioner->init_system()) {
            QMessageBox::critical(this, "错误", "YOLO系统初始化失败！");
            delete visioner;
            visioner = nullptr;
            return false;
        }

        modelLoaded = true;
        ui->loadModelBtn->setEnabled(false);
        ui->yoloDetectBtn->setEnabled(true);

        updateStatus(QString("YOLO模型加载成功：%1").arg(QFileInfo(modelPath).fileName()));
        QMessageBox::information(this, "成功", "YOLO模型加载成功！");

        return true;

    } catch (const std::exception& e) {
        QString errorMsg = QString("加载模型失败: %1").arg(e.what());
        QMessageBox::critical(this, "错误", errorMsg);
        updateStatus("模型加载失败");
        return false;
    }
}

void Widget::on_loadModelBtn_clicked()
{
    updateStatus("正在加载YOLO模型...");

    if (loadYOLOModel()) {
        // 成功加载模型，启用检测按钮
        ui->statusLabel->setText("模型加载完成 - 可以开始检测");
    }
}

void Widget::on_yoloDetectBtn_clicked()
{
    if (!modelLoaded || !visioner) {
        QMessageBox::warning(this, "警告", "请先加载YOLO模型！");
        return;
    }

    if (!imageLoaded) {
        QMessageBox::warning(this, "警告", "请先加载图片！");
        return;
    }

    try {
        updateStatus("正在进行YOLO检测...");

        // 执行检测
        cv::Mat& dmaImage = visioner->get_rga_mat();
        processedImage.copyTo(dmaImage);

        if (visioner->detect_once(dmaImage, detectionResults)) {
            // 显示检测结果
            displayYOLOResults();
            drawDetections(dmaImage);

            // 更新显示
            displayImage(dmaImage);

            ui->statusLabel->setText(QString("YOLO检测完成 - 发现 %1 个目标").arg(detectionResults.size()));
        } else {
            QMessageBox::warning(this, "警告", "YOLO检测失败！");
            updateStatus("检测失败");
        }

    } catch (const std::exception& e) {
        QString errorMsg = QString("检测失败: %1").arg(e.what());
        QMessageBox::critical(this, "错误", errorMsg);
        updateStatus("检测失败");
    }
}

void Widget::performYOLODetection(const cv::Mat& image)
{
    if (!modelLoaded || !visioner) {
        return;
    }

    try {
        // 复制图像到DMA内存
        cv::Mat& dmaImage = visioner->get_rga_mat();
        image.copyTo(dmaImage);

        // 执行检测
        if (visioner->detect_once(dmaImage, detectionResults)) {
            // 更新显示
            displayImage(dmaImage);
            displayYOLOResults();
        }

    } catch (const std::exception& e) {
        qDebug() << "YOLO检测出错:" << e.what();
    }
}

void Widget::displayYOLOResults()
{
    QString resultsText;
    if (detectionResults.empty()) {
        resultsText = "未检测到目标\n";
    } else {
        resultsText = "检测结果：\n";
        for (size_t i = 0; i < detectionResults.size(); ++i) {
            const auto& result = detectionResults[i];
            resultsText += QString("[%1] %2 - 置信度: %3%\n")
                .arg(i + 1)
                .arg(QString::fromStdString(result.className))
                .arg(result.prop * 100, 0, 'f', 1);
        }
    }

    // 在结果文本框中显示
    ui->resultsTextEdit->setText(resultsText);
}

void Widget::drawDetections(cv::Mat& image)
{
    if (image.empty() || detectionResults.empty()) {
        return;
    }

    // 颜色定义
    cv::Scalar color(0, 255, 0); // 绿色

    for (const auto& result : detectionResults) {
        // 获取边界框坐标
        int x1 = result.box[0];
        int y1 = result.box[1];
        int x2 = result.box[2];
        int y2 = result.box[3];

        // 绘制边界框
        cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), color, 2);

        // 准备标签文本
        QString label = QString("%1 %.1f%").arg(
            QString::fromStdString(result.className)).arg(result.prop * 100, 0, 'f', 1);

        // 绘制标签背景
        int baseline = 0;
        cv::Size labelSize = cv::getTextSize(label.toStdString(),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        cv::rectangle(image,
            cv::Point(x1, y1 - labelSize.height - baseline),
            cv::Point(x1 + labelSize.width, y1),
            color, -1);

        // 绘制标签文本
        cv::putText(image, label.toStdString(),
            cv::Point(x1, y1 - baseline),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

void Widget::startCamera()
{
    if (!camera) {
        QMessageBox::warning(this, "警告", "相机未初始化！");
        return;
    }

    try {
        // 打开相机
        if (!camera->Camera_Open()) {
            QMessageBox::warning(this, "警告", "相机打开失败！");
            return;
        }

        // 启动定时器
        cameraTimer->start();
        isDetecting = true;

        // 更新UI状态
        ui->startBtn->setEnabled(false);
        ui->stopBtn->setEnabled(true);
        ui->loadImageBtn->setEnabled(false);
        ui->processImageBtn->setEnabled(false);
        ui->yoloDetectBtn->setEnabled(false);

        updateStatus("相机已启动 - 实时检测中...");

    } catch (const std::exception& e) {
        QString errorMsg = QString("启动相机失败: %1").arg(e.what());
        QMessageBox::critical(this, "错误", errorMsg);
    }
}

void Widget::stopCamera()
{
    // 停止定时器
    cameraTimer->stop();
    isDetecting = false;

    // 关闭相机
    if (camera) {
        camera->Camera_Close();
    }

    // 更新UI状态
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->loadImageBtn->setEnabled(true);
    ui->processImageBtn->setEnabled(imageLoaded);
    ui->yoloDetectBtn->setEnabled(modelLoaded && imageLoaded);

    updateStatus("相机已停止");
}

void Widget::processCameraFrame()
{
    if (!camera || !visioner || !isDetecting) {
        return;
    }

    try {
        // 获取DMA内存中的图像
        cv::Mat& dmaImage = visioner->get_rga_mat();

        // 执行检测
        if (visioner->detect_once(dmaImage, detectionResults)) {
            // 绘制检测结果
            drawDetections(dmaImage);

            // 更新显示
            displayImage(dmaImage);

            // 更新状态
            ui->statusLabel->setText(QString("实时检测中... - 发现 %1 个目标").arg(detectionResults.size()));
        }

    } catch (const std::exception& e) {
        qDebug() << "处理相机帧时出错:" << e.what();
    }
}

// ===== 原有功能保留 =====
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
    if (image.empty()) {
        ui->imageLabel->clear();
        ui->imageLabel->setText("等待图像...");
        return;
    }

    cv::Mat rgbImage;
    QImage::Format format;

    if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
        format = QImage::Format_RGB888;
    } else {
        rgbImage = image; // 灰度图
        format = QImage::Format_Indexed8;
    }

    // 构造 QImage，使用 .copy() 确保内存安全
    QImage qImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, format);
    //QPixmap pixmap = QPixmap::fromImage(qImage.copy());
    if (qImage.isNull()) {
        qDebug() << "Error: Failed to create QImage from OpenCV Mat";
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(qImage);
    if (pixmap.isNull()) {
        qDebug() << "Error: Failed to create QPixmap from QImage";
        return;
    }

    // 优化缩放逻辑：使用 ScrollArea 的视口大小作为参考
    QSize viewportSize = ui->scrollArea->viewport()->size();
    QSize originalSize = pixmap.size();

    // 计算合适的缩放尺寸，保持宽高比
    QSize scaledSize = originalSize;  // 默认使用原始大小

    if (viewportSize.width() > 0 && viewportSize.height() > 0) {
        // 计算缩放比例，留出一些边距
        int margin = 20;
        int availableWidth = viewportSize.width() - margin;
        int availableHeight = viewportSize.height() - margin;

        double scaleWidth = (double)availableWidth / originalSize.width();
        double scaleHeight = (double)availableHeight / originalSize.height();
        double scale = qMin(scaleWidth, scaleHeight);

        // 确保不会过度放大（除非图片很小）
        scale = qMin(scale, 1.0);

        // 如果缩放后的大小小于 label 的最小大小，使用最小大小
        int minSize = 100;
        scaledSize = QSize(
            qMax(qRound(originalSize.width() * scale), minSize),
            qMax(qRound(originalSize.height() * scale), minSize)
        );
    } else {
        // 如果视口大小不可用，使用合理的默认大小
        int maxSize = 800;
        if (originalSize.width() > maxSize || originalSize.height() > maxSize) {
            double scale = qMin((double)maxSize / originalSize.width(),
                              (double)maxSize / originalSize.height());
            scaledSize = QSize(originalSize.width() * scale, originalSize.height() * scale);
        }
    }

    // 设置缩放后的图片
    ui->imageLabel->setPixmap(pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 确保图片居中显示
    ui->imageLabel->setAlignment(Qt::AlignCenter);

    // 确保 ScrollArea 可以滚动
    ui->imageLabel->setMinimumSize(scaledSize);
    ui->imageLabel->adjustSize();

    // 确保滚动区域更新
    ui->scrollArea->ensureVisible(
        ui->imageLabel->width() / 2,
        ui->imageLabel->height() / 2,
        50, 50
    );
}

void Widget::updateButtonsState()
{
    ui->saveImageBtn->setEnabled(imageLoaded);
    ui->processImageBtn->setEnabled(imageLoaded);
    ui->yoloDetectBtn->setEnabled(modelLoaded && imageLoaded);
}

void Widget::updateStatus(const QString& status)
{
    ui->statusLabel->setText(status);
}

void Widget::on_startBtn_clicked()
{
    startCamera();
}

void Widget::on_stopBtn_clicked()
{
    stopCamera();
}

void Widget::on_loadImageBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择图片文件",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff)"
    );

    if (!filePath.isEmpty()) {
        if (loadImage(filePath)) {
            ui->statusLabel->setText(QString("已加载图片: %1").arg(QFileInfo(filePath).fileName()));
        } else {
            QMessageBox::warning(this, "警告", "图片加载失败！");
        }
    }
}

void Widget::on_saveImageBtn_clicked()
{
    if (!imageLoaded) {
        QMessageBox::warning(this, "警告", "没有图片可保存！");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "保存图片",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp)"
    );

    if (!filePath.isEmpty()) {
        try {
            if (!cv::imwrite(filePath.toStdString(), processedImage)) {
                QMessageBox::warning(this, "警告", "保存图片失败！");
            } else {
                QMessageBox::information(this, "成功", "图片保存成功！");
                ui->statusLabel->setText(QString("图片已保存: %1").arg(QFileInfo(filePath).fileName()));
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "错误", QString("保存图片时出错: %1").arg(e.what()));
        }
    }
}

void Widget::on_processImageBtn_clicked()
{
    if (!imageLoaded) {
        QMessageBox::warning(this, "警告", "请先加载图片！");
        return;
    }

    applyFilters();
}

void Widget::applyGrayscale()
{
    if (!imageLoaded) return;

    if (ui->grayscaleCheck->isChecked()) {
        cv::cvtColor(processedImage, processedImage, cv::COLOR_BGR2GRAY);
        cv::cvtColor(processedImage, processedImage, cv::COLOR_GRAY2BGR);
    }
}

void Widget::applyBlur()
{
    if (!imageLoaded) return;

    if (ui->blurCheck->isChecked()) {
        cv::GaussianBlur(processedImage, processedImage, cv::Size(5, 5), 0);
    }
}

void Widget::applyEdgeDetection()
{
    if (!imageLoaded) return;

    if (ui->edgeCheck->isChecked()) {
        cv::Mat gray;
        cv::cvtColor(processedImage, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, processedImage, 50, 150);
        cv::cvtColor(processedImage, processedImage, cv::COLOR_GRAY2BGR);
    }
}

void Widget::adjustBrightness(int value)
{
    brightnessValue = value;
    if (imageLoaded) {
        applyFilters();
    }
}

void Widget::adjustContrast(int value)
{
    contrastValue = value;
    if (imageLoaded) {
        applyFilters();
    }
}

void Widget::applyFilters()
{
    if (!imageLoaded) return;

    // 恢复原始图像
    processedImage = originalImage.clone();

    // 应用滤镜
    applyGrayscale();
    applyBlur();
    applyEdgeDetection();

    // 调整亮度和对比度
    if (brightnessValue != 0 || contrastValue != 0) {
        double alpha = 1.0 + (double)contrastValue / 100.0;
        int beta = brightnessValue;

        cv::Mat adjusted;
        processedImage.convertTo(adjusted, -1, alpha, beta);
        processedImage = adjusted;
    }

    // 显示处理后的图像
    displayImage(processedImage);

    // 更新状态
    showProcessingTime();
}

void Widget::showProcessingTime()
{
    static QElapsedTimer timer;
    static bool firstTime = true;

    if (firstTime) {
        timer.start();
        firstTime = false;
    } else {
        qint64 elapsed = timer.elapsed();
        timer.restart();
        ui->statusLabel->setText(QString("处理完成 - 耗时: %1 ms").arg(elapsed));
    }
}

void Widget::updateImageInfo()
{
    if (!imageLoaded) return;

    QString info = QString("图片信息：\n");
    info += QString("尺寸: %1 x %2\n").arg(originalImage.cols).arg(originalImage.rows);
    info += QString("通道: %1\n").arg(originalImage.channels());
    info += QString("深度: %1").arg(originalImage.depth());

    ui->statusLabel->setText(info);
}

cv::Mat Widget::convertToQtFormat(const cv::Mat& image)
{
    if (image.empty()) return cv::Mat();

    cv::Mat rgbImage;
    if (image.channels() == 3) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGRA2RGB);
    } else {
        rgbImage = image;
    }

    return rgbImage;
}
