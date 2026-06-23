#include "../include/widget.h"
#include "ui_widget.h"
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
    modelLoaded(false),
    videoCapture_(nullptr),
    videoTimer_(nullptr),
    videoPlaying_(false),
    videoTotalFrames_(0),
    videoCurrentFrame_(0),
    modbusComm_(nullptr),
    modbusOpened_(false)
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
    ui->loadVideoBtn->setEnabled(false);

    // --- 连接按钮信号 --- 
    //QObject::connect(sender, &SenderClass::signalName, 
    //             receiver, &ReceiverClass::slotName);
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

    // 初始化视频定时器（用于视频文件检测）
    videoTimer_ = new QTimer(this);
    videoTimer_->setInterval(33); // ~30fps
    connect(videoTimer_, &QTimer::timeout, this, &Widget::processVideoFrame);

    // 初始化真实世界坐标转换（标定数据）
    initCoordinateTransform();

    // 初始化 ResMLP 模型（加载权重）
    resMLPLoaded_ = false;
    // 优先查找 install 部署目录 (与可执行文件同级)
    QString resMLPWeightPath = QApplication::applicationDirPath() + "/model/ResMLP.weights";
    if (resMLP_.load(resMLPWeightPath.toStdString())) {
        resMLPLoaded_ = true;
        qDebug() << "[ResMLP] 模型加载成功:" << resMLPWeightPath;
        ui->statusLabel->setText("就绪 - 模型已加载, ResMLP 就绪");
    } else {
        // 备用路径: 开发时从 build 目录运行 (源码目录下)
        resMLPWeightPath = QApplication::applicationDirPath() + "/../ResMLP/model/ResMLP.weights";
        if (resMLP_.load(resMLPWeightPath.toStdString())) {
            resMLPLoaded_ = true;
            qDebug() << "[ResMLP] 模型加载成功:" << resMLPWeightPath;
            ui->statusLabel->setText("就绪 - 模型已加载, ResMLP 就绪");
        } else {
            qWarning() << "[ResMLP] 模型加载失败，电压计算功能不可用:"
                       << QApplication::applicationDirPath() + "/model/ResMLP.weights";
            // 不阻塞启动，电压计算将在检测时不可用
        }
    }

    // 初始化 Modbus 串口通信 (RS232 → 电源)
    modbusOpened_ = false;
    // 尝试打开默认串口设备 /dev/ttyUSB0 (USB转RS232线)
    modbusComm_ = new modbus::ModbusComm("/dev/ttyUSB0");
    if (modbusComm_->open()) {
        modbusOpened_ = true;
        qDebug() << "[Modbus] 串口初始化成功, 设备: /dev/ttyUSB0 @ 9600-8-N-1";
    } else {
        // 备用: 尝试板载 UART4 (/dev/ttyS4)
        modbusComm_->close();
        delete modbusComm_;
        modbusComm_ = new modbus::ModbusComm("/dev/ttyS4");
        if (modbusComm_->open()) {
            modbusOpened_ = true;
            qDebug() << "[Modbus] 串口初始化成功, 设备: /dev/ttyS4 @ 9600-8-N-1";
        } else {
            qWarning() << "[Modbus] 串口初始化失败，电压输出功能不可用:"
                       << "/dev/ttyUSB0 和 /dev/ttyS4 均无法打开";
            delete modbusComm_;
            modbusComm_ = nullptr;
        }
    }
}

Widget::~Widget()
{
    // 停止视频播放
    stopVideoPlayback();
    if (videoTimer_) {
        delete videoTimer_;
        videoTimer_ = nullptr;
    }

    // 关闭 Modbus 串口
    if (modbusComm_) {
        modbusComm_->close();
        delete modbusComm_;
        modbusComm_ = nullptr;
    }

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
        updateButtonsState();

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

        // 创建相机对象 (但先不初始化)
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

        // init_system(): 加载模型 + 分配DMA缓冲区 + 初始化相机(只注册回调不启动取流)
        bool ret = visioner->init_system();
        if (ret) {
            // 暂停管线(已分配DMA池但未取流), 等点击"开始检测"才启动
            camera->stop_grabbing();
        }
        return ret;
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
    //   3. 填充 detectionResults (className, classId, prop, box[4])
    bool success = visioner->detect_once(detectImage, detectionResults);

    qint64 elapsed = timer.elapsed();

    if (success) {
        // detect_once 已绘制检测框, 直接显示
        displayImage(detectImage);

        // 更新 processedImage 为带检测框的图像
        processedImage = detectImage.clone();

        // 显示检测结果信息（包含真实世界坐标转换）
        displayYOLOResults(detectImage.cols, detectImage.rows);

        updateStatus(QString("YOLO检测完成，耗时 %1 ms").arg(elapsed));
    } else {
        // 检测失败或未检测到目标, 显示原图
        displayImage(detectImage);
        updateStatus("YOLO检测未检测到目标");
        ui->detectResultLabel->setText("未检测到目标");
        ui->resultsTextEdit->setText("YOLO检测完成\n未检测到目标");
    }
}

void Widget::displayYOLOResults(int srcW, int srcH)
{
    if (detectionResults.empty()) {
        ui->detectResultLabel->setText("未检测到目标");
        ui->resultsTextEdit->setText("YOLO检测完成\n未检测到目标");
        return;
    }

    // 更新检测结果标签（显示第一个目标的真实坐标）
    float firstCx = (detectionResults[0].box[0] + detectionResults[0].box[2]) / 2.0f;
    float firstCy = (detectionResults[0].box[1] + detectionResults[0].box[3]) / 2.0f;
    float wX, wY;
    QString coordStr;
    bool hasCoord = pixelToRealWorld(firstCx, firstCy, srcW, srcH, wX, wY);
    if (hasCoord) {
        coordStr = QString(" | 真实坐标: (%1, %2) mm").arg(wX, 0, 'f', 1).arg(wY, 0, 'f', 1);
    }

    // ResMLP 电压计算
    QString voltageStr;
    float voltages[4] = {0};
    if (hasCoord && computeVoltages(wX, wY, voltages)) {
        voltageStr = QString(" | 电压: %1/%2/%3/%4 V")
                         .arg(voltages[0], 0, 'f', 2)
                         .arg(voltages[1], 0, 'f', 2)
                         .arg(voltages[2], 0, 'f', 2)
                         .arg(voltages[3], 0, 'f', 2);
    }

    QString resultSummary = QString("检测到 %1 个目标%2%3")
                                .arg(detectionResults.size())
                                .arg(coordStr)
                                .arg(voltageStr);
    ui->detectResultLabel->setText(resultSummary);

    // 更新详细结果文本框，包含真实世界坐标和电压信息
    QString detail = "YOLO检测结果\n";
    detail += "━━━━━━━━━━━━━━━━━━\n\n";
    detail += QString("检测到 %1 个目标:\n\n").arg(detectionResults.size());

    for (size_t i = 0; i < detectionResults.size(); ++i) {
        const auto& result = detectionResults[i];
        detail += QString("目标 %1:\n").arg(i + 1);
        detail += QString("  类别: %1\n").arg(QString::fromStdString(result.className));
        detail += QString("  置信度: %1%\n").arg(result.prop * 100, 0, 'f', 1);

        // 检测框中心坐标（在当前源图像坐标系中）
        float cx = (result.box[0] + result.box[2]) / 2.0f;
        float cy = (result.box[1] + result.box[3]) / 2.0f;
        detail += QString("  图像坐标: (%1, %2)\n").arg(cx, 0, 'f', 1).arg(cy, 0, 'f', 1);

        // 真实物理世界坐标
        float worldX, worldY;
        if (pixelToRealWorld(cx, cy, srcW, srcH, worldX, worldY)) {
            detail += QString("  真实坐标: (%1, %2) mm\n").arg(worldX, 0, 'f', 1).arg(worldY, 0, 'f', 1);

            // ResMLP 电压输出
            float v[4] = {0};
            if (computeVoltages(worldX, worldY, v)) {
                detail += QString("  移相电压:\n");
                detail += QString("    通道1: %1 V\n").arg(v[0], 0, 'f', 3);
                detail += QString("    通道2: %1 V\n").arg(v[1], 0, 'f', 3);
                detail += QString("    通道3: %1 V\n").arg(v[2], 0, 'f', 3);
                detail += QString("    通道4: %1 V\n").arg(v[3], 0, 'f', 3);
                float avg = (v[0] + v[1] + v[2] + v[3]) / 4.0f;
                detail += QString("  平均电压: %1 V\n").arg(avg, 0, 'f', 3);
            }
        } else {
            detail += QString("  真实坐标: (转换失败)\n");
        }
        detail += "\n";
    }

    ui->resultsTextEdit->setText(detail);
}

// drawDetections 保留但不被 performYOLODetection 调用,
// 因为 detect_once() 已使用 image_utils 在图像上绘制了检测框.
// 注: detect_once() 现已填充 DetectionResult.box[4], 可直接使用.
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
//  真实世界坐标转换（标定映射）
// ============================================================

bool Widget::initCoordinateTransform()
{
    // 4个角标定点：源图像坐标 (2048x1230 空间) → 真实物理世界坐标 (mm)
    // 使用 getPerspectiveTransform（opencv_imgproc）需要精确4个点
    cv::Point2f srcPts[4] = {
        cv::Point2f(579.0f,   183.0f),   // 左上 → (-300, -300)
        cv::Point2f(1234.5f,  183.0f),   // 右上 → (300, -300)
        cv::Point2f(1234.5f,  850.0f),   // 右下 → (300, 300)
        cv::Point2f(579.0f,   850.0f)    // 左下 → (-300, 300)
    };
    cv::Point2f dstPts[4] = {
        cv::Point2f(-300.0f, -300.0f),   // 左上
        cv::Point2f(300.0f,  -300.0f),   // 右上
        cv::Point2f(300.0f,   300.0f),   // 右下
        cv::Point2f(-300.0f,  300.0f)    // 左下
    };

    // 计算透视变换矩阵 (getPerspectiveTransform 在 opencv_imgproc 中，无需 calib3d)
    homographyMatrix_ = cv::getPerspectiveTransform(srcPts, dstPts);

    if (homographyMatrix_.empty()) {
        qWarning() << "透视变换矩阵计算失败！";
        return false;
    }

    qDebug() << "真实世界坐标转换初始化成功";
    return true;
}

bool Widget::pixelToRealWorld(float pixelX, float pixelY, int srcW, int srcH,
                               float& worldX, float& worldY)
{
    if (homographyMatrix_.empty()) {
        return false;
    }

    float nativeX, nativeY;

    // 判断检测框坐标所在的空间并进行相应转换
    if (srcW == MODEL_INPUT_SIZE && srcH == MODEL_INPUT_SIZE) {
        // 相机路径：坐标在 640x640 letterbox 空间中 → 还原到 2048x1230 原生空间
        // letterbox 参数：scale = 640/2048 = 0.3125, 垂直居中, y_pad/2 = 128
        float scale = (float)MODEL_INPUT_SIZE / CAM_NATIVE_WIDTH;
        float scaledH = CAM_NATIVE_HEIGHT * scale;
        float yPad = (MODEL_INPUT_SIZE - scaledH) / 2.0f;

        nativeX = pixelX / scale;
        nativeY = (pixelY - yPad) / scale;
    } else {
        // 文件加载路径：坐标已在原生分辨率空间（如 2048x1230）
        nativeX = pixelX;
        nativeY = pixelY;
    }

    // 应用透视变换得到真实物理世界坐标
    std::vector<cv::Point2f> srcVec = {cv::Point2f(nativeX, nativeY)};
    std::vector<cv::Point2f> dstVec;
    cv::perspectiveTransform(srcVec, dstVec, homographyMatrix_);

    if (dstVec.empty()) {
        return false;
    }

    worldX = dstVec[0].x;
    worldY = dstVec[0].y;
    return true;
}

// ============================================================
//  ResMLP 推理: 真实世界坐标 → 4 路电压值
// ============================================================

bool Widget::computeVoltages(float worldX, float worldY, float voltages[4])
{
    if (!resMLPLoaded_) {
        return false;
    }

    // worldX, worldY 单位为 mm, 模型需要米
    float xMeters = worldX / 1000.0f;
    float yMeters = worldY / 1000.0f;
    float zFixed  = 3.6f;  // 固定高度, 与训练数据一致

    // 方式1: 从 (x, y, z) 直接推理
    std::vector<float> result = resMLP_.predictFromXYZ(xMeters, yMeters, zFixed);

    if (result.size() < 4) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        voltages[i] = result[i];
    }

    // 通过 Modbus 串口发送电压到下位机电源
    if (modbusOpened_ && modbusComm_) {
        modbusComm_->writeAllVoltages(voltages[0], voltages[1],
                                       voltages[2], voltages[3]);
    }

    return true;
}

// ============================================================
//  视频文件检测
// ============================================================

void Widget::on_loadVideoBtn_clicked()
{
    // 必须先加载 YOLO 模型
    if (!modelLoaded || !visioner) {
        QMessageBox::warning(this, "提示", "请先加载YOLO模型！");
        return;
    }

    // 如果已有视频在播放，先停止
    if (videoPlaying_) {
        stopVideoPlayback();
    }

    // 选择视频文件
    QString videoPath = QFileDialog::getOpenFileName(
        this,
        "选择视频文件",
        "",
        "视频文件 (*.avi *.mp4 *.mov *.mkv);;所有文件 (*)"
    );

    if (videoPath.isEmpty()) {
        return;
    }

    // 如果相机正在检测，先停止
    if (isDetecting) {
        stopCamera();
        ui->startBtn->setEnabled(true);
        ui->stopBtn->setEnabled(false);
    }

    // 打开视频文件
    videoCapture_ = new cv::VideoCapture(videoPath.toStdString());
    if (!videoCapture_->isOpened()) {
        QMessageBox::warning(this, "视频加载失败",
            "无法打开视频文件，请检查:\n"
            "1. 文件格式是否支持\n"
            "2. 文件路径是否包含中文或特殊字符\n"
            "3. 视频编码是否为 H.264 或 MJPEG");
        delete videoCapture_;
        videoCapture_ = nullptr;
        return;
    }

    // 读取视频信息
    videoTotalFrames_ = (int)videoCapture_->get(cv::CAP_PROP_FRAME_COUNT);
    double fps = videoCapture_->get(cv::CAP_PROP_FPS);
    videoTimer_->setInterval(fps > 0 ? (int)(1000.0 / fps) : 33);

    videoCurrentFrame_ = 0;
    videoPlaying_ = true;

    // 更新按钮状态
    ui->loadVideoBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
    ui->loadImageBtn->setEnabled(false);
    ui->saveImageBtn->setEnabled(false);
    ui->loadModelBtn->setEnabled(false);
    ui->loadModelMiniBtn->setEnabled(false);
    ui->startBtn->setEnabled(false);

    ui->statusLabel->setText(QString("视频已加载: %1 (共%2帧, %.1f fps) - 正在播放...")
        .arg(QFileInfo(videoPath).fileName())
        .arg(videoTotalFrames_)
        .arg(fps));

    // 启动视频定时器
    videoTimer_->start();
}

void Widget::processVideoFrame()
{
    // 校验状态
    if (!videoCapture_ || !videoPlaying_ || !modelLoaded || !visioner) {
        return;
    }

    // 读取一帧
    cv::Mat frame;
    bool readOk = videoCapture_->read(frame);

    if (!readOk || frame.empty()) {
        // 视频播放完毕
        videoPlaying_ = false;
        videoTimer_->stop();
        ui->statusLabel->setText("视频播放完毕");
        ui->loadVideoBtn->setEnabled(true);
        ui->startBtn->setEnabled(true);
        ui->stopBtn->setEnabled(false);
        ui->loadImageBtn->setEnabled(true);
        ui->loadModelBtn->setEnabled(true);
        ui->loadModelMiniBtn->setEnabled(true);
        ui->saveImageBtn->setEnabled(true);
        return;
    }

    videoCurrentFrame_++;

    // 执行 YOLO 检测（复用单图检测路径）
    detectionResults.clear();
    cv::Mat detectImage = frame.clone();
    bool success = visioner->detect_once(detectImage, detectionResults);

    // 显示图像
    displayImage(detectImage);
    processedImage = detectImage.clone();
    imageLoaded = true;

    // 显示检测结果（含 ResMLP 电压）
    if (success && !detectionResults.empty()) {
        const auto& first = detectionResults[0];
        float cx = (first.box[0] + first.box[2]) / 2.0f;
        float cy = (first.box[1] + first.box[3]) / 2.0f;

        float worldX, worldY;
        bool hasCoord = pixelToRealWorld(cx, cy, detectImage.cols, detectImage.rows, worldX, worldY);

        QString coordInfo;
        if (hasCoord) {
            coordInfo = QString(" | 真实坐标: (%1, %2) mm")
                            .arg(worldX, 0, 'f', 1)
                            .arg(worldY, 0, 'f', 1);
        }

        // ResMLP 电压计算
        float voltages[4] = {0};
        bool hasVoltage = false;
        if (hasCoord) {
            hasVoltage = computeVoltages(worldX, worldY, voltages);
        }

        QString voltageInfo;
        if (hasVoltage) {
            voltageInfo = QString(" | 电压: %1/%2/%3/%4 V")
                              .arg(voltages[0], 0, 'f', 2)
                              .arg(voltages[1], 0, 'f', 2)
                              .arg(voltages[2], 0, 'f', 2)
                              .arg(voltages[3], 0, 'f', 2);
        }

        ui->detectResultLabel->setText(
            QString("%1 %2%%3%4")
                .arg(QString::fromStdString(first.className))
                .arg(first.prop * 100, 0, 'f', 1)
                .arg(coordInfo)
                .arg(voltageInfo));

        // 详细结果文本框
        QString detail = QString("视频检测结果 — 帧 %1/%2\n"
                                 "━━━━━━━━━━━━━━━━━━\n\n"
                                 "目标: %3\n"
                                 "置信度: %4%\n"
                                 "图像坐标: (%5, %6)\n")
                            .arg(videoCurrentFrame_)
                            .arg(videoTotalFrames_)
                            .arg(QString::fromStdString(first.className))
                            .arg(first.prop * 100, 0, 'f', 1)
                            .arg(cx, 0, 'f', 1)
                            .arg(cy, 0, 'f', 1);

        if (hasCoord) {
            detail += QString("真实坐标: (%1, %2) mm\n")
                          .arg(worldX, 0, 'f', 1)
                          .arg(worldY, 0, 'f', 1);
        }

        if (hasVoltage) {
            detail += QString("\n移相电压:\n");
            detail += QString("  通道1: %1 V\n").arg(voltages[0], 0, 'f', 3);
            detail += QString("  通道2: %1 V\n").arg(voltages[1], 0, 'f', 3);
            detail += QString("  通道3: %1 V\n").arg(voltages[2], 0, 'f', 3);
            detail += QString("  通道4: %1 V\n").arg(voltages[3], 0, 'f', 3);
            float avg = (voltages[0] + voltages[1] + voltages[2] + voltages[3]) / 4.0f;
            detail += QString("平均电压: %1 V\n").arg(avg, 0, 'f', 3);
        }

        if (detectionResults.size() > 1) {
            detail += QString("\n... 还有 %1 个目标\n").arg(detectionResults.size() - 1);
        }

        ui->resultsTextEdit->setText(detail);
    } else {
        ui->detectResultLabel->setText("未检测到目标");
    }

    // 更新状态栏进度
    QString progress = QString("视频播放中... 帧 %1/%2")
                           .arg(videoCurrentFrame_)
                           .arg(videoTotalFrames_);
    ui->statusLabel->setText(progress);
}

void Widget::stopVideoPlayback()
{
    if (videoTimer_ && videoTimer_->isActive()) {
        videoTimer_->stop();
    }

    videoPlaying_ = false;

    if (videoCapture_) {
        videoCapture_->release();
        delete videoCapture_;
        videoCapture_ = nullptr;
    }

    videoCurrentFrame_ = 0;
    videoTotalFrames_ = 0;
}

// ============================================================
//  相机控制
// ============================================================

void Widget::startCamera()
{
    if (isDetecting) return;

    if (modelLoaded && visioner && camera) {
        // 模型已加载: visioner->init_system() 已初始化并打开了相机
        // 启动取流管线（RGA线程 + SDK取流）
        if (!camera->start_grabbing()) {
            QMessageBox::warning(this, "相机错误", "无法启动相机取流！");
            updateStatus("相机启动失败");
            return;
        }
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

    // 如果尚未初始化，执行完整初始化流程
    if (!camera->Camera_Init()) {
        QMessageBox::warning(this, "相机错误", "相机初始化失败！请检查相机连接。");
        updateStatus("相机初始化失败");
        return;
    }

    camera->open_grab_callback();  // 启用回调+RGA模式
    if (!camera->Camera_Open()) {
        QMessageBox::warning(this, "相机错误", "无法打开相机设备！");
        updateStatus("相机打开失败");
        return;
    }

    // 启动取流
    if (camera->start_grabbing()) {
        isDetecting = true;
        cameraTimer->start();
        updateStatus("相机已启动");
    } else {
        QMessageBox::warning(this, "相机错误", "无法启动相机取流！");
        updateStatus("相机启动失败");
    }
}

void Widget::stopCamera()
{
    if (cameraTimer && cameraTimer->isActive()) {
        cameraTimer->stop();
    }

    isDetecting = false;

    // 暂停相机取流管线（停止RGA线程和SDK取流）
    if (camera) {
        camera->stop_grabbing();
    }

    updateStatus("相机已停止");
}

void Widget::processCameraFrame()
{
    static int debugCounter = 0; // 用于周期打印调试信息

    if (!camera || !isDetecting) return;

    cv::Mat frame;
    // 从RGA转换队列获取最新帧（已由硬件转换为RGB888格式）
    if (camera->get_converted_frame(frame, 100)) {
        if (frame.empty()) return;

        debugCounter = 0; // 取到帧，重置计数器

        // RGA输出为RGB888，OpenCV标准为BGR → 通道转换
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);

        originalImage = frame.clone();
        imageLoaded = true;

        // 如果模型已加载，执行自动 YOLO 检测
        if (modelLoaded && visioner) {
            // 将帧数据拷贝到NPU可访问的DMA缓冲区，实现零拷贝推理
            cv::Mat rgaMat = visioner->get_rga_mat();
            frame.copyTo(rgaMat);

            detectionResults.clear();

            // detect_once 内部会绘制检测框，同时填充 detectionResults（含 box[4] 坐标）
            visioner->detect_once(rgaMat, detectionResults);

            displayImage(rgaMat);
            processedImage = rgaMat.clone();

            // 实时显示检测结果中的真实世界坐标
            if (!detectionResults.empty()) {
                const auto& first = detectionResults[0];
                float cx = (first.box[0] + first.box[2]) / 2.0f;
                float cy = (first.box[1] + first.box[3]) / 2.0f;

                float worldX, worldY;
                bool hasCoord = pixelToRealWorld(cx, cy, rgaMat.cols, rgaMat.rows, worldX, worldY);

                QString coordInfo;
                if (hasCoord) {
                    coordInfo = QString(" | 真实坐标: (%1, %2) mm")
                                    .arg(worldX, 0, 'f', 1)
                                    .arg(worldY, 0, 'f', 1);
                }

                // ResMLP 电压计算
                float voltages[4] = {0};
                bool hasVoltage = false;
                if (hasCoord) {
                    hasVoltage = computeVoltages(worldX, worldY, voltages);
                }

                QString voltageInfo;
                if (hasVoltage) {
                    voltageInfo = QString(" | 电压: %1/%2/%3/%4 V")
                                      .arg(voltages[0], 0, 'f', 2)
                                      .arg(voltages[1], 0, 'f', 2)
                                      .arg(voltages[2], 0, 'f', 2)
                                      .arg(voltages[3], 0, 'f', 2);
                }

                ui->detectResultLabel->setText(
                    QString("%1 %2%3%4")
                        .arg(QString::fromStdString(first.className))
                        .arg(first.prop * 100, 0, 'f', 1)
                        .arg(coordInfo)
                        .arg(voltageInfo));

                // 在结果文本框中也显示详细信息（第一个目标）
                QString detail = QString("实时检测结果 — %1\n"
                                         "━━━━━━━━━━━━━━━━━━\n\n"
                                         "目标: %2\n"
                                         "置信度: %3%\n"
                                         "图像坐标: (%4, %5)\n")
                                    .arg(QString::fromStdString(first.className))
                                    .arg(QString::fromStdString(first.className))
                                    .arg(first.prop * 100, 0, 'f', 1)
                                    .arg(cx, 0, 'f', 1).arg(cy, 0, 'f', 1);

                if (hasCoord) {
                    detail += QString("真实坐标: (%1, %2) mm\n")
                                  .arg(worldX, 0, 'f', 1)
                                  .arg(worldY, 0, 'f', 1);
                }

                if (hasVoltage) {
                    detail += QString("\n移相电压:\n");
                    detail += QString("  通道1: %1 V\n").arg(voltages[0], 0, 'f', 3);
                    detail += QString("  通道2: %1 V\n").arg(voltages[1], 0, 'f', 3);
                    detail += QString("  通道3: %1 V\n").arg(voltages[2], 0, 'f', 3);
                    detail += QString("  通道4: %1 V\n").arg(voltages[3], 0, 'f', 3);
                    float avg = (voltages[0] + voltages[1] + voltages[2] + voltages[3]) / 4.0f;
                    detail += QString("平均电压: %1 V\n").arg(avg, 0, 'f', 3);
                }

                if (detectionResults.size() > 1) {
                    detail += QString("\n... 还有 %1 个目标\n").arg(detectionResults.size() - 1);
                }

                ui->resultsTextEdit->setText(detail);
            } else {
                ui->detectResultLabel->setText("未检测到目标");
            }
        } else {
            // 纯相机预览模式
            displayImage(frame);
            processedImage = frame.clone();
        }
    } else {
        // 未取到帧：每隔约2秒打印一次（仅调试用）
        debugCounter++;
        if (debugCounter % 20 == 1) {
            printf("[processCameraFrame] No frame available for ~2s\n");
        }
    }
}

// ============================================================
//  开始/停止 按钮
// ============================================================

void Widget::on_startBtn_clicked()
{
    // 如果视频正在播放，先停止
    if (videoPlaying_) {
        stopVideoPlayback();
    }

    startCamera();

    ui->startBtn->setEnabled(false);//设置相应按钮状态（可用/禁用）
    ui->stopBtn->setEnabled(true);
    ui->loadImageBtn->setEnabled(false);
    ui->loadVideoBtn->setEnabled(false);
    ui->loadModelBtn->setEnabled(false);
    ui->loadModelMiniBtn->setEnabled(false);
}

void Widget::on_stopBtn_clicked()
{
    // 如果视频正在播放，停止视频
    if (videoPlaying_) {
        stopVideoPlayback();
        ui->startBtn->setEnabled(true);
        ui->stopBtn->setEnabled(false);
        ui->loadImageBtn->setEnabled(true);
        ui->loadVideoBtn->setEnabled(true);
        ui->loadModelBtn->setEnabled(true);
        ui->loadModelMiniBtn->setEnabled(true);
        ui->statusLabel->setText("视频已停止");
        return;
    }

    stopCamera();

    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
    ui->loadImageBtn->setEnabled(true);
    ui->loadVideoBtn->setEnabled(true);
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

    // 视频加载按钮: 模型已加载且视频未在播放时启用
    ui->loadVideoBtn->setEnabled(modelLoaded && !videoPlaying_);
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
