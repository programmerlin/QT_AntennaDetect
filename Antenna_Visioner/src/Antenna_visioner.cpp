#include "Antenna_Visioner.h"
#include <iostream>
#include <fstream>
#include <chrono> // C++11/14 时间库

// Rockchip 硬件加速库
#include "im2d.h"
#include "RgaUtils.h"
#include "rga.h"
#include "opencv2/core.hpp"
#include "opencv2/videoio.hpp" // VideoCapture
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/opencv.hpp"
#include "postprocess.h"


Antenna_Visioner::Antenna_Visioner(const std::string& modelPath, 
                                const std::string& labelPath, 
                                int width,
                                int height,
                                HikCamera* camera)
    : modelPath_(modelPath), 
      labelPath_(labelPath), 
      width_(width),
      height_(height),
      dmaFd_(-1),
      virtAddr_(nullptr),
      HikCamera_(camera)
      {
        /* 成员变量已有默认初始化值 (C++11/14 特性) */
        memset(&rknnAppCtx_, 0, sizeof(rknn_app_context_t));
      }

Antenna_Visioner::~Antenna_Visioner() 
{
    int ret;

    deinit_post_process();
    ret = release_yolov8_model(&rknnAppCtx_);
    if (ret != 0)
    {
        printf("release_yolov8_model fail! ret=%d\n", ret);
    }

    if(virtAddr_ != nullptr) {
        dma_buf_free(bufSize_, &dmaFd_, virtAddr_);
        printf("DMA buffer freed successfully.\n");
    }
}

bool Antenna_Visioner::init_system()
{
    int ret;

    /* 设置模型路径并初始化后处理资源 */
    set_label_path(labelPath_);
    ret = init_post_process();
    if (ret != 0) {
        printf("Fail to initial post process.\n");
        return false;
    }

    /* 初始化 RKNN 模型 */
    ret = init_yolov8_model(modelPath_.c_str(), &rknnAppCtx_);
    if(ret != 0){
        printf("Initial yolov8 model failed! ret = %d model_path = %s\n", ret, modelPath_);
        goto fail_to_init_model;
    }

    bufSize_ = width_ * height_ * 3;
    ret = dma_buf_alloc(DMA_HEAP_DMA32_UNCACHE_PATCH, bufSize_, &dmaFd_, &virtAddr_);
    if(ret < 0 || virtAddr_ == nullptr) {
        std::cerr << "Error: Failed to allocate DMA buffer!" << std::endl;
        return false;
    }

    rgaMat_ = cv::Mat(height_, width_, CV_8UC3, virtAddr_);
    printf("RGA Mat initialized successfully.\n");

    /* 初始化并打开相机（只分配DMA池和注册回调，不启动取流）*/
    HikCamera_->open_grab_callback();
    if (!HikCamera_->Camera_Init() || !HikCamera_->Camera_Open()) {
        printf("Camera Init Failed!\n");
        return false;
    }

    printf("[init_system] Camera ready. (grabbing will start when user clicks Start)\n");
    return true;

fail_to_init_model:
    deinit_post_process();
    return false;

}

bool Antenna_Visioner::detect_once(cv::Mat& outputFrame, std::vector<DetectionResult>& outResults) 
{
    int ret;

    // [关键步骤] 每次进入函数先清空上一帧的检测结果，防止累积
    outResults.clear();

    #if TIME_CONM_CALC
    /* C++14 使用 std::chrono 进行高精度计时 */
    auto start_time = std::chrono::high_resolution_clock::now();
    #endif
    
    cv::Mat origImg;
    image_buffer_t ibImage;

    memset(&ibImage, 0, sizeof(image_buffer_t));

    origImg = outputFrame;
    if (origImg.empty()) {
        std::cerr << "Read pic data failed." << std::endl;
        return false;
    }

    /* 格式转换: OpenCV 默认是 BGR，RKNN/YOLO 通常需要 RGB */
    //cv::cvtColor(origImg, origImg, cv::COLOR_BGR2RGB);

    /* 填充 src_image 结构体信息 */
    ibImage.width = origImg.cols;
    ibImage.height = origImg.rows;
    ibImage.format = IMAGE_FORMAT_RGB888;
    ibImage.size = origImg.total() * origImg.elemSize(); // 宽*高*通道数

    /* [普通路径]: 直接使用 OpenCV 的内存指针 */
    // 注意：origImg 必须在 inference 结束前保持存活
    ibImage.virt_addr = origImg.data;
    ibImage.fd = dmaFd_; // 有 dma fd

    /* 推理与后处理 */
    object_detect_result_list odResults;
    ret = inference_yolov8_model(&rknnAppCtx_, &ibImage, &odResults);
    if (ret != 0) {
        printf("inference fail! ret=%d\n", ret);
        return false; // 跳过本帧后续处理，开始处理下一帧
    }

    

    #if TIME_CONM_CALC
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "detect time: " << duration.count() << "ms." << std::endl;
    #endif

    /* 处理检测结果 */
    for (int i = 0; i < odResults.count; i++) {
        object_detect_result *detResult = &(odResults.results[i]);
        
        DetectionResult resultItem;
        resultItem.className = std::string(coco_cls_to_name(detResult->cls_id));
        resultItem.classId = detResult->cls_id;
        resultItem.prop = detResult->prop;
        // 填充边界框坐标（后处理已映射到源图像空间）
        resultItem.box[0] = detResult->box.left;
        resultItem.box[1] = detResult->box.top;
        resultItem.box[2] = detResult->box.right;
        resultItem.box[3] = detResult->box.bottom;
        outResults.push_back(resultItem);

        /* 简单的终端打印 */
        printf("%s @ (%.2f) \n", coco_cls_to_name(detResult->cls_id), detResult->prop);

        int x1 = detResult->box.left;
        int y1 = detResult->box.top;
        int x2 = detResult->box.right;
        int y2 = detResult->box.bottom;

        /* 调用你原本的画图函数 (依然保留画图功能) */
        draw_rectangle(&ibImage, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);
        
        char text[256];
        sprintf(text, "%s %.1f%%", coco_cls_to_name(detResult->cls_id), detResult->prop * 100);
        draw_text(&ibImage, text, x1, y1 - 20, COLOR_RED, 10);
    }

    #if TIME_CONM_CALC
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "detect + drawing time: " << duration.count() << "ms." << std::endl;
    #endif
    
    return true;
}

cv::Mat& Antenna_Visioner::get_rga_mat(void)
{
    return rgaMat_;
}
