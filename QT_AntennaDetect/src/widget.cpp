#include "../include/widget.h"
#include "../ui_widget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QResizeEvent>
#include <QApplication>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    imageLoaded(false),
    brightnessValue(0),
    contrastValue(0),
    camera(nullptr),
    visioner(nullptr),
    cameraTimer(nullptr),
    isDetecting(false),
    modelLoaded(false)
{
    ui->setupUi(this);

    // 固定窗口大小为 1024x600
    setFixedSize(1024, 600);

    // 设置初始状态
    ui->stopBtn->setEnabled(false);
    ui->saveImageBtn->setEnabled(false);
    ui->processImageBtn->setEnabled(false);
    ui->yoloDetectBtn->setEnabled(false);
    ui->yoloDetectMiniBtn->setEnabled(false);

    // --- 连接按钮信号 ---
    connect(ui->startBtn, &QPushButton::clicked, this, &Widget::on_startBtn_clicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &Widget::on_stopBtn_clicked);
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &Widget::on_loadImageBtn_clicked);
    connect(ui->saveImageBtn, &QPushButton::clicked, this, &Widget::on_saveImageBtn_clicked);
    connect(ui->processImageBtn, &QPushButton::clicked, this, &Widget::on_processImageBtn_clicked);

    // YOLO 按钮信号 (主工具栏)
    connect(ui->loadModelBtn, &QPushButton::clicked, this, &Widget::on_loadModelBtn_clicked);
    connect(ui->yoloDetectBtn, &QPushButton::clicked, this, &Widget::on_yoloDetectBtn_clicked);
    // 右侧面板快捷按钮 (loadModelMiniBtn, yoloDetectMiniBtn) 的 clicked 信号
    // 已在 .ui 文件的 <connections> 中关联到对应的槽函数

    // --- 连接图像处理控件 ---
    // 注意: grayscaleCheck, blurCheck, edgeCheck 的 toggled 信号已在 .ui 的
    // <connections> 中关联到 applyFilters() 槽, 此处不再重复连接
    connect(ui->brightnessSlider, &QSlider::valueChanged, this, &Widget::adjustBrightness);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &Widget::adjustContrast);

    // 设置滑块范围
    ui->brightnessSlider->setRange(-100, 100);
    ui->contrastSlider->setRange(-100, 100);

    // 连接滑块值变化信号以更新显示值
    connect(ui->brightnessSlider, &QSlider::valueChanged, this, &Widget::updateBrightnessValue);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &Widget::updateContrastValue);

    // 设置初始状态文本
    ui->statusLabel->setText("就绪 - 请加载模型和图片开始检测");
    ui->resultsTextEdit->setText(
        "天线检测系统 v2.0\n"
        "━━━━━━━━━━━━━━━━━━\n"
        "功能说明：\n"
        "1. [加载模型] - 选择 YOLO 模型(.rknn)和标签文件(.txt)\n"
        "2. [加载图片] - 选择待检测的图片\n"
        "3. [YOLO检测] - 执行天线目标检测\n"
        "4. [开始检测] - 启动USB相机实时检测\n"
        "5. 右侧面板支持 OpenCV 图像处理"
    );

    // 设置图像标签的初始状态
    ui->imageLabel->setMinimumSize(1, 1);
    ui->imageLabel->setScaledContents(false);
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setText("等待图像加载...");

    // 初始化相机定时器（用于实时检测）
    cameraTimer = new QTimer(this);
    cameraTimer->setInterval(100); // ~10fps
    connect(cameraTimer, &QTimer::timeout, this, &Widget::processCameraFrame);
}

Widget::~Widget()
{
    // 停止定时器
    if (cameraTimer && cameraTimer->isActive()) {
        cameraTimer->stop();
    }

    // 先释放 visioner (其析构会释放模型和DMA内存, 但不关闭相机)
    if (visioner) {
        delete visioner;
        visioner = nullptr;
    }

    // 再关闭并释放相机
    if (camera) {
        camera->Camera_Close();
        delete camera;
        camera = nullptr;
    }

    delete ui;
}

// ============================================================
//  YOLO 模型加载
// ============================================================

void Widget::on_loadModelBtn_clicked()
{
    // 选择 RKNN 模型文件
    QString selectedModelPath = QFileDialog::getOpenFileName(
        this,
        "选择 YOLO 模型文件",
        "",
        "RKNN 模型 (*.rknn);;所有文件 (*)"
    );

    if (selectedModelPath.isEmpty()) {
        return;
    }

    // 选择标签文件
    QString selectedLabelPath = QFileDialog::getOpenFileName(
        this,
        "选择标签文件",
        QFileInfo(selectedModelPath).absolutePath(),
        "标签文件 (*.txt);;所有文件 (*)"
    );

    if (selectedLabelPath.isEmpty()) {
        return;
    }

    modelPath = selectedModelPath;
    labelPath = selectedLabelPath;

    updateStatus("正在加载模型...");
    ui->statusLabel->setText("正在加载YOLO模型，请稍候...");

    // 处理界面事件，让状态标签刷新
    QApplication::processEvents();

    // 加载模型 (内部会创建 camera 对象并初始化)
    if (loadYOLOModel()) {
        modelLoaded = true;
        ui->yoloDetectBtn->setEnabled(true);
        ui->yoloDetectMiniBtn->setEnabled(true);

        // 更新模型状态指示
        ui->modelStatusLabel->setText("模型已加载");
        ui->modelStatusIcon->setStyleSheet(
            "QLabel { background-color: #27ae60; border-radius: 8px; }"
        );

        QString modelName = QFileInfo(modelPath).fileName();
        QString labelName = QFileInfo(labelPath).fileName();
        updateStatus(QString("模型加载成功: %1 | 标签: %2").arg(modelName, labelName));
        ui->statusLabel->setText("YOLO模型加载成功");
        ui->resultsTextEdit->setText(
            QString("✓ YOLO模型加载成功\n\n"
                    "模型文件: %1\n"
                    "标签文件: %2\n\n"
                    "请加载图片后点击 [YOLO检测] 或启动相机实时检测")
                .arg(modelName, labelName)
        );
    } else {
        modelLoaded = false;
        updateStatus("模型加载失败！");
        QMessageBox::warning(this, "模型加载失败",
            "无法加载YOLO模型，请检查:\n"
            "1. 模型文件是否为有效的 RKNN 格式\n"
            "2. 标签文件格式是否正确\n"
            "3. 文件路径是否包含中文或特殊字符");
    }
}

bool Widget::loadYOLOModel()
{
    if (modelPath.isEmpty() || labelPath.isEmpty()) {
        return false;
    }

    try {
        // 先释放旧的 visioner
        if (visioner) {
            delete visioner;
            visioner = nullptr;
        }

        // 重要: 创建相机对象 (但先不初始化)
        // Antenna_Visioner::init_system() 内部会调用 Camera_Init() + Camera_Open()
        // 所以此处只需 new HikCamera (状态为 WAIT_FOR_INIT), 不调用 init/open
        if (!camera) {
            camera = new HikCamera(0);
        }

        visioner = new Antenna_Visioner(
            modelPath.toStdString(),
            labelPath.toStdString(),
            640,
            640,
            camera
        );

        // init_system() 内部会:
        //   1. 加载模型
        //   2. 分配 DMA 缓冲区
        //   3. 初始化并打开相机 (Camera_Init + Camera_Open)
        return visioner->init_system();
    } catch (const std::exception& e) {
        qDebug() << "loadYOLOModel exception:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "loadYOLOModel unknown exception";
        return false;
    }
}

// ============================================================
//  YOLO 检测
// ============================================================

void Widget::on_yoloDetectBtn_clicked()
{
    if (!modelLoaded || !visioner) {
        QMessageBox::warning(this, "提示", "请先加载YOLO模型！");
        return;
    }

    if (!imageLoaded || processedImage.empty()) {
        QMessageBox::warning(this, "提示", "请先加载需要检测的图片！");
        return;
    }

    performYOLODetection(processedImage);
}

void Widget::performYOLODetection(const cv::Mat& image)
{
    if (!visioner || image.empty()) {
        return;
    }

    updateStatus("正在执行YOLO检测...");
    QApplication::processEvents();

    detectionResults.clear();

    // 创建检测用图像副本
    cv::Mat detectImage = image.clone();

    // 执行检测
    QElapsedTimer timer;
    timer.start();

    // detect_once() 内部会:
    //   1. 执行 RKNN 推理
    //   2. 用 image_utils 的 draw_rectangle/draw_text 直接在图像上绘制检测框和标签
    //   3. 填充 detectionResults (className, classId, prop; 但不填充 box[4])
    bool success = visioner->detect_once(detectImage, detectionResults);

    qint64 elapsed = timer.elapsed();

    if (success) {
        // detect_once 已绘制检测框, 直接显示
        displayImage(detectImage);

        // 更新 processedImage 为带检测框的图像
        processedImage = detectImage.clone();

        // 显示检测结果信息
        displayYOLOResults();

        updateStatus(QString("YOLO检测完成，耗时 %1 ms").arg(elapsed));
    } else {
        // 检测失败或未检测到目标, 显示原图
        displayImage(detectImage);
        updateStatus("YOLO检测未检测到目标");
        ui->detectResultLabel->setText("未检测到目标");
        ui->resultsTextEdit->setText("YOLO检测完成\n未检测到目标");
    }
}

void Widget::displayYOLOResults()
{
    if (detectionResults.empty()) {
        ui->detectResultLabel->setText("未检测到目标");
        ui->resultsTextEdit->setText("YOLO检测完成\n未检测到目标");
        return;
    }

    // 更新检测结果标签
    QString resultSummary = QString("检测到 %1 个目标").arg(detectionResults.size());
    ui->detectResultLabel->setText(resultSummary);

    // 更新详细结果文本框
    // 注意: detect_once() 不填充 box[4] 坐标, 所以结果中不显示位置信息
    QString detail = "YOLO检测结果\n";
    detail += "━━━━━━━━━━━━━━━━━━\n\n";
    detail += QString("检测到 %1 个目标:\n\n").arg(detectionResults.size());

    for (size_t i = 0; i < detectionResults.size(); ++i) {
        const auto& result = detectionResults[i];
        detail += QString("目标 %1:\n").arg(i + 1);
        detail += QString("  类别: %1\n").arg(QString::fromStdString(result.className));
        detail += QString("  置信度: %1%\n").arg(result.prop * 100, 0, 'f', 1);
        detail += "\n";
    }

    ui->resultsTextEdit->setText(detail);
}

// drawDetections 保留但不被 performYOLODetection 调用,
// 因为 detect_once() 已使用 image_utils 在图像上绘制了检测框.
// detect_once() 不填充 DetectionResult.box[4], 因此此函数使用 box 值前需确保其有效.
void Widget::drawDetections(cv::Mat& image)
{
    if (image.empty()) return;

    for (const auto& result : detectionResults) {
        int x1 = result.box[0];
        int y1 = result.box[1];
        int x2 = result.box[2];
        int y2 = result.box[3];

        cv::Scalar color(
            (result.classId * 50) % 255,
            (result.classId * 100) % 255,
            (result.classId * 150) % 255
        );

        cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), color, 2);

        std::string label = result.className + " " +
            std::to_string(static_cast<int>(result.prop * 100)) + "%";

        int baseline = 0;
        cv::Size labelSize = cv::getTextSize(
            label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        int labelY = y1;
        if (labelY - labelSize.height - 5 < 0) {
            labelY = y2 + labelSize.height + 5;
        }

        cv::rectangle(
            image,
            cv::Point(x1, labelY - labelSize.height - 5),
            cv::Point(x1 + labelSize.width + 10, labelY + baseline),
            color,
            cv::FILLED
        );

        cv::putText(
            image,
            label,
            cv::Point(x1 + 5, labelY - 5),
            cv::FONT_HERSHEY_SIMPLEX,
            0.5,
            cv::Scalar(255, 255, 255),
            1
        );
    }
}

// ============================================================
//  相机控制
// ============================================================

void Widget::startCamera()
{
    if (isDetecting) return;

    if (modelLoaded && visioner && camera) {
        // 模型已加载: visioner->init_system() 已初始化并打开了相机
        // 只需启动定时器即可开始采集和检测
        isDetecting = true;
        cameraTimer->start();
        updateStatus("相机已启动，实时检测中...");
        return;
    }

    // 未加载模型: 手动初始化并打开相机（纯预览模式）
    updateStatus("正在初始化相机...");
    QApplication::processEvents();

    if (!camera) {
        camera = new HikCamera(0);
    }

    if (!camera->Camera_Init()) {
        QMessageBox::warning(this, "相机错误", "相机初始化失败！请检查相机连接。");
        updateStatus("相机初始化失败");
        return;
    }

    if (camera->Camera_Open()) {
        isDetecting = true;
        cameraTimer->start();
        updateStatus("相机已启动");
    } else {
        QMessageBox::warning(this, "相机错误", "无法打开相机设备！");
        updateStatus("相机打开失败");
    }
}

void Widget::stopCamera()
{
    if (cameraTimer && cameraTimer->isActive()) {
        cameraTimer->stop();
    }

    isDetecting = false;

    // 注意: 不关闭相机设备, 因为:
    //   - 若模型已加载, 相机由 visioner 管理生命周期
    //   - 相机资源在 Widget 析构时统一释放
    updateStatus("相机已停止");
}

void Widget::processCameraFrame()
{
    if (!camera || !isDetecting) return;

    cv::Mat frame;
    if (camera->Image_Get(frame, 100)) {
        if (frame.empty()) return;

        originalImage = frame.clone();
        imageLoaded = true;

        // 如果模型已加载, 执行自动 YOLO 检测
        if (modelLoaded && visioner) {
            detectionResults.clear();

            // detect_once 内部会绘制检测框
            if (visioner->detect_once(frame, detectionResults)) {
                displayImage(frame);
                processedImage = frame.clone();
            } else {
                displayImage(frame);
                processedImage = frame.clone();
            }
        } else {
            // 纯相机预览模式
            displayImage(frame);
            processedImage = frame.clone();
        }
    }
}

// ============================================================
//  开始/停止 按钮
// ============================================================

void Widget::on_startBtn_clicked()
{
    startCamera();

    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->loadImageBtn->setEnabled(false);
    ui->loadModelBtn->setEnabled(false);
    ui->loadModelMiniBtn->setEnabled(false);
}

void Widget::on_stopBtn_clicked()
{
    stopCamera();

    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->loadImageBtn->setEnabled(true);
    ui->loadModelBtn->setEnabled(true);
    ui->loadModelMiniBtn->setEnabled(true);
}

// ============================================================
//  图片加载与保存
// ============================================================

void Widget::on_loadImageBtn_clicked()
{
    QString imagePath = QFileDialog::getOpenFileName(
        this,
        "选择图片",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff)"
    );

    if (imagePath.isEmpty()) {
        return;
    }

    if (loadImage(imagePath)) {
        ui->resultsTextEdit->setText(
            "图片加载成功！\n"
            "可使用右侧面板进行图像处理。\n\n"
            "如果已加载YOLO模型，点击 [YOLO检测] 进行目标检测。\n\n"
            "支持的格式：JPG, JPEG, PNG, BMP, TIFF"
        );
    }
}

bool Widget::loadImage(const QString& filePath)
{
    originalImage = cv::imread(filePath.toStdString(), cv::IMREAD_COLOR);

    if (originalImage.empty()) {
        QMessageBox::warning(this, "警告", "无法读取图片！");
        return false;
    }

    processedImage = originalImage.clone();
    imageLoaded = true;

    displayImage(processedImage);
    updateButtonsState();
    updateImageInfo();

    ui->statusLabel->setText(QString("已加载图片: %1").arg(QFileInfo(filePath).fileName()));

    return true;
}

void Widget::on_saveImageBtn_clicked()
{
    if (!imageLoaded) return;

    QString savePath = QFileDialog::getSaveFileName(
        this,
        "保存图片",
        "",
        "图片文件 (*.jpg *.jpeg *.png *.bmp)"
    );

    if (savePath.isEmpty()) {
        return;
    }

    QString ext = QFileInfo(savePath).suffix().toLower();
    if (ext != "jpg" && ext != "jpeg" && ext != "png" && ext != "bmp") {
        savePath += ".jpg";
    }

    bool success = cv::imwrite(savePath.toStdString(), processedImage);

    if (success) {
        ui->statusLabel->setText(QString("图片已保存: %1").arg(QFileInfo(savePath).fileName()));
        QMessageBox::information(this, "成功", "图片保存成功！");
    } else {
        QMessageBox::warning(this, "警告", "图片保存失败！");
    }
}

// ============================================================
//  图像处理
// ============================================================

void Widget::on_processImageBtn_clicked()
{
    if (!imageLoaded) return;

    processedImage = originalImage.clone();
    applyFilters();

    ui->statusLabel->setText("图像处理完成");
    ui->resultsTextEdit->setText("图像处理完成");
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

    processedImage = originalImage.clone();

    applyGrayscale();
    applyBlur();
    applyEdgeDetection();

    if (brightnessValue != 0 || contrastValue != 0) {
        double alpha = 1.0 + (double)contrastValue / 100.0;
        int beta = brightnessValue;

        processedImage.convertTo(processedImage, -1, alpha, beta);
    }

    displayImage(processedImage);
    showProcessingTime();
}

void Widget::updateBrightnessValue(int value)
{
    ui->brightnessValueLabel->setText(QString::number(value));
    if (imageLoaded) {
        applyFilters();
    }
}

void Widget::updateContrastValue(int value)
{
    ui->contrastValueLabel->setText(QString::number(value));
    if (imageLoaded) {
        applyFilters();
    }
}

// ============================================================
//  图像显示
// ============================================================

void Widget::displayImage(const cv::Mat& image)
{
    if (image.empty()) {
        ui->imageLabel->clear();
        ui->imageLabel->setText("等待图像加载...");
        return;
    }

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

    // 创建深拷贝，确保 QImage 持有独立的数据
    QImage imgCopy = qImage.copy();
    QPixmap pixmap = QPixmap::fromImage(imgCopy);

    // 缩放图片以适应显示区域
    QSize viewportSize = ui->scrollArea->viewport()->size();
    if (viewportSize.width() > 0 && viewportSize.height() > 0) {
        int margin = 10;
        int availableWidth = viewportSize.width() - margin;
        int availableHeight = viewportSize.height() - margin;

        double scaleWidth = (double)availableWidth / pixmap.width();
        double scaleHeight = (double)availableHeight / pixmap.height();
        double scale = qMin(scaleWidth, scaleHeight);

        scale = qMin(scale, 1.0);

        if (scale > 0) {
            QSize scaledSize(
                qRound(pixmap.width() * scale),
                qRound(pixmap.height() * scale)
            );
            pixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    ui->imageLabel->setPixmap(pixmap);
}

void Widget::updateImageInfo()
{
    if (!imageLoaded) return;

    QString info = "=== 图像信息 ===\n\n";
    info += QString("原始尺寸: %1 x %2 像素\n").arg(originalImage.cols).arg(originalImage.rows);
    info += QString("通道数: %1\n").arg(originalImage.channels());
    info += QString("位深度: %1\n").arg(originalImage.depth());

    if (modelLoaded) {
        info += "\n=== YOLO模型 ===\n\n";
        info += QString("模型: %1\n").arg(QFileInfo(modelPath).fileName());
        info += QString("标签: %1\n").arg(QFileInfo(labelPath).fileName());
        info += QString("检测目标: %1 个\n").arg(detectionResults.size());
    }

    if (imageLoaded) {
        info += "\n=== 处理状态 ===\n\n";
        info += "亮度调整: " + QString::number(brightnessValue) + "\n";
        info += "对比度调整: " + QString::number(contrastValue) + "\n";
        info += QString("灰度效果: ") + (ui->grayscaleCheck->isChecked() ? "✓" : "✗") + "\n";
        info += QString("模糊效果: ") + (ui->blurCheck->isChecked() ? "✓" : "✗") + "\n";
        info += QString("边缘检测: ") + (ui->edgeCheck->isChecked() ? "✓" : "✗") + "\n";
    }

    ui->resultsTextEdit->setText(info);
}

// ============================================================
//  UI 更新辅助
// ============================================================

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

    // YOLO检测按钮仅在模型已加载且有图片时启用
    if (modelLoaded && hasImage) {
        ui->yoloDetectBtn->setEnabled(true);
        ui->yoloDetectMiniBtn->setEnabled(true);
    }
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

void Widget::updateStatus(const QString& status)
{
    ui->statusLabel->setText(status);
    qDebug() << "[Status]" << status;
}

void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (imageLoaded && !processedImage.empty()) {
        displayImage(processedImage);
    }
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

void Widget::initComponents()
{
    // 组件初始化 - 在需要时由具体功能触发初始化
    // 相机在 loadYOLOModel() 或 startCamera() 中延迟初始化
    // YOLO 模型在 loadYOLOModel() 中初始化
}
