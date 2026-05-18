#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <thread>
#include "../3rdparty/libhikvision/include/MvCameraControl.h"
#include "HikCamera.h"
#include "im2d.h"
#include "../dma_buffer_pool/include/dma_buffer_pool.h"
#include "drmrga.h"

HikCamera::HikCamera(int cameraIndex)
    : cameraDeviceIndex_(cameraIndex),  //私有成员变量初始化列表
      deviceHandle_(nullptr),
      nPayloadSize_(0),
      cameraStatus_(CameraStatus::WAIT_FOR_INIT),
      openGrabCallback_(false) {}

HikCamera::~HikCamera()
{
    Camera_Close();

    /* 析构函数 */
    if (deviceHandle_) {
        MV_CC_DestroyHandle(deviceHandle_);
        deviceHandle_ = nullptr;
    }
}

bool HikCamera::Camera_Init()
{  
    int nRet = MV_OK;
    
    if (cameraStatus_ != CameraStatus::WAIT_FOR_INIT) {
        printf("Camera has been initialled.\n");
        return false;
    }

    /* 设备枚举 */
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet)
    {
        printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
        return false;
    }

    /* 显示设备信息 */
    if (stDeviceList.nDeviceNum > 0)
    {
        for (int i = 0; i < stDeviceList.nDeviceNum; i++)
        {
            printf("[device %d]:\n", i);
            MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
            if (NULL == pDeviceInfo)
            {
                printf("Fail to get current device info.\n");
                return false;
            } 
           // PrintDeviceInfo(pDeviceInfo);            
        }  
    } 
    else
    {
        printf("Find No Devices!\n");
        return false;
    }

    /* 检查设备是否可访问 */
    if (cameraDeviceIndex_ >= 0 && cameraDeviceIndex_ < stDeviceList.nDeviceNum) {
        if (false == MV_CC_IsDeviceAccessible(stDeviceList.pDeviceInfo[cameraDeviceIndex_], 
                                                MV_ACCESS_Exclusive)) {
            printf("Can't connect! Please check the camera index value entered during instantiation.\n"); 
            return false;
        }
    } else {
        printf("The camera index is invalid.\n");
        return false;
    }

    // 选择设备并创建句柄
    nRet = MV_CC_CreateHandle(&deviceHandle_, stDeviceList.pDeviceInfo[cameraDeviceIndex_]);
    if (MV_OK != nRet)
    {
        printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
        return false;
    }

    cameraStatus_ = CameraStatus::INITED;   //强制设置枚举为初始化状态
    return true;
}

bool HikCamera::Camera_Open()
{
    int nRet = MV_OK;

    if(cameraStatus_ == CameraStatus::OPENED) {
        printf("Camera has been opened. No need to open again.\n");
        return true;
    }

    if (cameraStatus_ != CameraStatus::INITED) {
        printf("Camera needs to be initialled.\n");
        return false;
    }

    /* 打开设备 */
    nRet = MV_CC_OpenDevice(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("OpenDevice fail! nRet [%x]\n", nRet);
        return false;
    }

    /*设置设备像素格式*/
    //设置图像像素格式为YUV422_YUYV_Packed格式
    nRet = MV_CC_SetEnumValue(deviceHandle_, "PixelFormat", PixelType_Gvsp_YUV422_YUYV_Packed);
    if (MV_OK != nRet)
    {
        printf("Set PixelFormat fail! nRet [%x]\n", nRet);
    }
    else
    {
        printf("[Config] Set PixelFormat to YUV422_YUYV_Packed success!\n");
    }

    //设置自动增益
    nRet = MV_CC_SetEnumValue(deviceHandle_, "GainAuto", 2);//设置自动增益，2表示Continuous（持续自动增益）
    if(MV_OK != nRet)
    {
        printf("[Warning] Set GainAuto fail! , trying manual gain 24dB...\n");
        nRet = MV_CC_SetEnumValue(deviceHandle_, "GainAuto", 0);//关闭自动增益
        if (MV_OK != nRet) {
            printf("[Warning] Set GainAuto to Off fail! nRet [%x]\n", nRet);
        } else {
            nRet = MV_CC_SetFloatValue(deviceHandle_, "Gain", 24.0f); //手动设置增益为24dB
            if (MV_OK != nRet) {
                printf("[Warning] Set manual Gain to 24dB fail! nRet [%x]\n", nRet);
            } else {
                printf("[Config] Set manual Gain to 24dB success.\n");
            }
        }
    }
    else{
        printf("[Config] Set GainAuto to true success!\n");
    }

    //设置自动曝光
    nRet = MV_CC_SetEnumValue(deviceHandle_, "ExposureAuto", 2);//设置自动曝光模式，2表示Continuous（持续自动曝光）
    if(MV_OK != nRet)
    {
        printf("[Warning] Set ExposureAuto fail! , trying manual exposure 50000...\n");
        nRet = MV_CC_SetEnumValue(deviceHandle_, "ExposureAuto", 0);//关闭自动曝光
        if (MV_OK != nRet) {
            printf("[Warning] Set ExposureAuto to Off fail! nRet [%x]\n", nRet);
        } else {
            nRet = MV_CC_SetFloatValue(deviceHandle_, "ExposureTime", 50000.0f); //手动设置曝光时间为50000微秒
            if (MV_OK != nRet) {
                printf("[Warning] Set manual ExposureTime to 50000us fail! nRet [%x]\n", nRet);
            } else {
                printf("[Config] Set manual ExposureTime to 50000us success.\n");
            }
        }
    }
    else
    {
        printf("[Config] Set ExposureAuto success!\n");
    }

    //稍微等待一下让自动算法收敛（变亮）
    usleep(500000); //等待500ms

    //关闭触发模式
    nRet = MV_CC_SetEnumValue(deviceHandle_, "TriggerMode", 0);
    if (MV_OK != nRet)
    {
        printf("[Warning] Set TriggerMode off fail! nRet [%x]\n", nRet);
        return false;
    }
    else
    {
        printf("[Config] Set TriggerMode to Off success!\n");
    }

    //设置Binning（2x2）来降低分辨率和提高帧率，适合YOLO实时检测的需求
    nRet = MV_CC_SetEnumValue(deviceHandle_, "BinningHorizontal", 2);
    if (MV_OK != nRet)    {
        printf("[Warning] Set BinningHorizontal fail! nRet [%x]\n", nRet);
        return false;
    }
    
    nRet = MV_CC_SetEnumValue(deviceHandle_, "BinningVertical", 2);
    if (MV_OK != nRet)    {
        printf("[Warning] Set BinningVertical fail! nRet [%x]\n", nRet);
        return false;
    }

    /* 获取图像大小 */
    MVCC_INTVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_INTVALUE));
    nRet = MV_CC_GetIntValue(deviceHandle_, "PayloadSize", &stParam);
    if (MV_OK != nRet)
    {
        printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
        return false;
    }
    nPayloadSize_ = stParam.nCurValue;
    printf("PayloadSize = %d.\n", nPayloadSize_);//打印当前图像数据大小

    // 获取图像宽高信息，后续分配内存和转换格式时需要用到，注意必须在打开设备后获取，因为有些参数（如Binning）会影响最终的图像尺寸
    MVCC_INTVALUE stWidth, stHeight;
    memset(&stWidth, 0, sizeof(MVCC_INTVALUE));
    memset(&stHeight, 0, sizeof(MVCC_INTVALUE));
    nRet = MV_CC_GetIntValue(deviceHandle_, "Width", &stWidth);
    if (MV_OK != nRet)
    {
        printf("Get Width fail! nRet [0x%x]\n", nRet);
        return false;
    }
    nRet = MV_CC_GetIntValue(deviceHandle_, "Height", &stHeight);
    if (MV_OK != nRet)
    {
        printf("Get Height fail! nRet [0x%x]\n", nRet);
        return false;
    }
    printf("Image Real Width = %d, Height = %d.\n", stWidth.nCurValue, stHeight.nCurValue);

    /* 初始化图像信息结构体 */
    memset(&stImageInfo_, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    // 默认置零
    pData_ = std::make_unique<uint8_t[]>(nPayloadSize_); 
    if (nullptr == pData_) {
        printf("Allocate memory failed.\n");
        return false;
    }

    // 注册图像回调函数和预分配DMA内存池（仅回调模式）
    if(openGrabCallback_){
        if(!sourcePool.alloc_pool(4, stWidth.nCurValue, stHeight.nCurValue, BufferFormat::YUV422)) return false;
        if(!yoloPool.alloc_pool(4, 640, 640, BufferFormat::RGB888)) return false;

        // 注意: RGA线程在 start_grabbing() 中启动, 此处仅注册回调
        nRet = MV_CC_RegisterImageCallBackEx2(deviceHandle_, HikCamera::image_callback_ex2, this, true);
        if(MV_OK != nRet) {
            printf("Register image callback failed! nRet [0x%x]\n", nRet);
            sourcePool.destroy_pool();
            yoloPool.destroy_pool();
            MV_CC_CloseDevice(deviceHandle_);
            return false;
        }
    }

    cameraStatus_ = CameraStatus::OPENED;   //强制设置枚举为打开状态
    printf("[Camera_Open] Camera ready (grabbing not started yet)\n");
    return true;
}

//函数用于从RGA转换队列中获取最新的RGB888图像帧，适用于回调模式
bool HikCamera::get_converted_frame(cv::Mat& rgbImage, unsigned int timeoutMs)
{
    if (cameraStatus_ != CameraStatus::OPENED) {
        return false;
    }

    DmaBuffer_t* latest = nullptr;
    DmaBuffer_t* temp = nullptr;

    // 获取最新的一帧（丢弃队列中积压的旧帧，保证实时性）
    if (!yoloTaskQueue.try_pop(latest)) {
        return false;  // 当前无可用帧
    }

    // 继续弹出队列中剩余的旧帧并释放，只保留最新的一帧
    while (yoloTaskQueue.try_pop(temp)) {
        yoloPool.release_buffer(latest);  // 释放旧帧
        latest = temp;                     // 更新为较新的帧
    }

    if (!latest || !latest->virtAddr) {
        if (latest) yoloPool.release_buffer(latest);
        return false;
    }

    // 将 DMA 缓冲区的 RGB 数据深拷贝到输出 Mat
    cv::Mat dmaMat(latest->height, latest->width, CV_8UC3, latest->virtAddr);
    dmaMat.copyTo(rgbImage);

    // 释放 DMA 缓冲区回内存池
    yoloPool.release_buffer(latest);

    return true;
}

//图像获取函数，适用于同步获取图像的场景（不使用回调模式时调用）
bool HikCamera::Image_Get(cv::Mat& img, unsigned int nMsec)
{
    int nRet;

    if (cameraStatus_ != CameraStatus::OPENED) {
        printf("Camera needs to been opened.\n");
        return false;
    }
    
    /* 获取一帧图像，超时时间为1000ms */
    nRet = MV_CC_GetOneFrameTimeout(deviceHandle_, pData_.get(), nPayloadSize_, &stImageInfo_, nMsec);
    if (MV_OK != nRet) {
        printf("Get Frame fail! nRet: [0x%x]\n.", nRet);
        return false;
    }
    printf("Get One Frame: Width[%d], Height[%d], FrameNum[%d]\n",
    stImageInfo_.nWidth, stImageInfo_.nHeight, stImageInfo_.nFrameNum);

    /* 格式转换 */
    return convert_to_mat(stImageInfo_, pData_, img);
}

bool HikCamera::convert_to_mat(MV_FRAME_OUT_INFO_EX& stImageInfo, std::unique_ptr<uint8_t[]>& pData, cv::Mat& dstImage)
{
    if (stImageInfo.nWidth <= 0 || stImageInfo.nHeight <= 0 || !pData) {
        printf("Invalid image info or data pointer.\n");
        return false;
    }
    
    dstImage.create(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3);
    unsigned int RGBDataSize = stImageInfo.nWidth * stImageInfo.nHeight * 3; // RGB888格式每个像素3字节

    // 调用SDK提供的像素格式转换函数，将YUYV格式转换为RGB888格式，转换后的数据直接存储在dstImage的内存中，避免了额外的内存拷贝，提高效率 
    MV_CC_PIXEL_CONVERT_PARAM_EX stConvertParam = {0};

    stConvertParam.nWidth = stImageInfo.nWidth;                
    stConvertParam.nHeight = stImageInfo.nHeight;               
    stConvertParam.pSrcData = pData.get();                        
    stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;         
    stConvertParam.enSrcPixelType = stImageInfo.enPixelType;    
    stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;                         
    stConvertParam.pDstBuffer = dstImage.data;                               
    stConvertParam.nDstBufferSize = RGBDataSize;                      
    printf("RGBDataSize: [0x%x]\n.", RGBDataSize);

    int nRet = MV_CC_ConvertPixelTypeEx(deviceHandle_, &stConvertParam);
    if (MV_OK != nRet)
    {
        printf("Convert Pixel Type fail! nRet [0x%x]\n", nRet);
        return false;
    }

    return true;
}

bool HikCamera::Camera_Close()
{
    int nRet;
    if (cameraStatus_ == CameraStatus::CLOSED) {
        printf("Camera has been closed already.\n");
        return true;
    }

    if (cameraStatus_ != CameraStatus::OPENED) {
        printf("Camera needs to be opened.\n");
        return false;
    }

    // 停止取流（可能已被 stop_grabbing 停止，忽略重複停止的错误）
    nRet = MV_CC_StopGrabbing(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("[Camera_Close] MV_CC_StopGrabbing: nRet [0x%x] (may be already stopped)\n", nRet);
    }

    // 关闭设备
    nRet = MV_CC_CloseDevice(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("ClosDevice fail! nRet [0x%x]\n", nRet);
        return false;
    }

    if(openGrabCallback_){
        stop_rga_thread(); //停止RGA线程（如果已停止则无操作）
        /* 清空RGA任务队列 */
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!rgaTaskQueue_.empty())
            {
                rgaTaskQueue_.pop();
            }
        }

        /* 清空YOLO输出队列，将DMA Buffer归还内存池 */
        DmaBuffer_t* pendingBuf = nullptr;
        while (yoloTaskQueue.try_pop(pendingBuf)) {
            if (pendingBuf) yoloPool.release_buffer(pendingBuf);
        }

        sourcePool.destroy_pool(); //销毁源图像内存池
        yoloPool.destroy_pool(); //销毁YOLO内存池
    }

    cameraStatus_ = CameraStatus::CLOSED;   //强制设置相机状态为已关闭
    return true;
}

void HikCamera::open_grab_callback()
{
    openGrabCallback_ = true;
}

bool HikCamera::start_grabbing()
{
    if (cameraStatus_ != CameraStatus::OPENED) {
        printf("[start_grabbing] Camera not opened.\n");
        return false;
    }

    // 启动RGA线程（如果未运行）
    if (openGrabCallback_ && !isRunning_) {
        start_rga_thread();
    }

    // 开始取流
    int nRet = MV_CC_StartGrabbing(deviceHandle_);
    if (MV_OK != nRet) {
        printf("[start_grabbing] MV_CC_StartGrabbing fail! nRet [0x%x]\n", nRet);
        if (openGrabCallback_ && isRunning_) {
            stop_rga_thread();
        }
        return false;
    }

    printf("[start_grabbing] Grabbing started.\n");
    return true;
}

void HikCamera::stop_grabbing()
{
    if (cameraStatus_ != CameraStatus::OPENED) {
        return;
    }

    // 1. 停止取流（阻止新回调触发）
    int nRet = MV_CC_StopGrabbing(deviceHandle_);
    if (MV_OK != nRet) {
        printf("[stop_grabbing] MV_CC_StopGrabbing fail! nRet [0x%x]\n", nRet);
    }

    // 2. 停止RGA线程（会处理完rgaTaskQueue_中剩余项并释放source buffer）
    if (openGrabCallback_) {
        stop_rga_thread();

        // 3. 清空yoloTaskQueue中未被消费的帧
        DmaBuffer_t* buf = nullptr;
        int drained = 0;
        while (yoloTaskQueue.try_pop(buf)) {
            yoloPool.release_buffer(buf);
            drained++;
        }
        if (drained > 0) {
            printf("[stop_grabbing] Drained %d unconsumed frames from yoloTaskQueue.\n", drained);
        }
    }

    printf("[stop_grabbing] Grabbing stopped.\n");
}

bool HikCamera::start_rga_thread()
{
    if(isRunning_){
        printf("RGA thread is already running.\n");
        return true;
    }

    isRunning_ = true;
    rgaThread_ = std::thread(&HikCamera::rga_dispatch_thread_func, this);
    printf("RGA thread started.\n");
    fflush(stdout); // 确保立即刷新到终端
    return true;
}

void HikCamera::stop_rga_thread()
{
    if (!isRunning_) {
        printf("RGA thread is not running.\n");
        return;
    }

    isRunning_ = false;
    queueCv_.notify_all(); //通知RGA线程退出等待状态，尽快响应停止请求

    if (rgaThread_.joinable()) {
        rgaThread_.join();
        printf("RGA thread stopped.\n");
    }
}

//RGA线程函数体，持续监听RGA任务队列，执行图像格式转换，并将转换后的图像放入YOLO任务队列供引擎使用
void HikCamera::rga_dispatch_thread_func()
{
    while(isRunning_){
        DmaBuffer_t* srcBuffer = nullptr; //从RGA任务队列中获取一块待转换的源DMA Buffer，注意这里的等待机制是条件变量，只有当队列非空或者接收到停止信号时才会被唤醒，避免了无谓的CPU占用和忙等待
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this](){ return !rgaTaskQueue_.empty() || !isRunning_; });//rgaTaskQueue_非空或者RGA线程停止标志被置位时唤醒线程继续执行
            if (!isRunning_ && rgaTaskQueue_.empty()) break; //如果线程被要求停止且队列已经空了，直接退出线程循环
            srcBuffer = rgaTaskQueue_.front();
            rgaTaskQueue_.pop();
        }

        if (srcBuffer != nullptr) {
            DmaBuffer_t* dstBuffer = yoloPool.get_buffer(); //从YOLO内存池中获取一块空闲的目标DMA Buffer用于存储RGA转换后的RGB图像，注意这里没有等待机制，如果池中没有可用的Buffer了，就直接丢弃当前帧的数据，避免了阻塞RGA线程和占用过多内存资源的风险
            if (dstBuffer != nullptr) {
                auto start_time = std::chrono::high_resolution_clock::now();
                //  调用RGA硬件加速的函数将YUYV格式的源图像转换为RGB888格式的目标图像，转换后的数据直接存储在dstBuffer指向的内存中，避免了额外的内存拷贝，提高效率
                bool rgaSuccess = rga_YUV422_to_RGB888(srcBuffer, dstBuffer);
                if (rgaSuccess) {
                    printf("[RGA] Hardware conversion OK -> push to yoloTaskQueue\n");
                    fflush(stdout);
                    yoloTaskQueue.push(dstBuffer); //将转换后的RGB图像放入YOLO任务队列，等待引擎推理
                } else {
                    printf("[RGA] Hardware conversion FAILED, trying CPU fallback...\n");
                    fflush(stdout);
                    // CPU fallback: SDK软件YUYV→RGB + OpenCV letterbox缩放
                    try {
                        int fullRgbSize = srcBuffer->width * srcBuffer->height * 3;
                        std::vector<uint8_t> fullRgb(fullRgbSize);

                        MV_CC_PIXEL_CONVERT_PARAM_EX stConvertParam = {0};
                        stConvertParam.nWidth = srcBuffer->width;
                        stConvertParam.nHeight = srcBuffer->height;
                        stConvertParam.pSrcData = (unsigned char*)srcBuffer->virtAddr;
                        stConvertParam.nSrcDataLen = srcBuffer->bufferSize;
                        stConvertParam.enSrcPixelType = PixelType_Gvsp_YUV422_YUYV_Packed;
                        stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
                        stConvertParam.pDstBuffer = fullRgb.data();
                        stConvertParam.nDstBufferSize = fullRgbSize;

                        int nRet = MV_CC_ConvertPixelTypeEx(deviceHandle_, &stConvertParam);
                        if (MV_OK == nRet) {
                            // Letterbox resize to dstBuffer dimensions
                            cv::Mat fullRgbMat(srcBuffer->height, srcBuffer->width, CV_8UC3, fullRgb.data());
                            cv::Mat dstMat(dstBuffer->height, dstBuffer->width, CV_8UC3, dstBuffer->virtAddr);
                            dstMat = cv::Scalar(114, 114, 114);

                            float scale = std::min((float)dstBuffer->width / srcBuffer->width,
                                                    (float)dstBuffer->height / srcBuffer->height);
                            int new_w = srcBuffer->width * scale;
                            int new_h = srcBuffer->height * scale;
                            int offset_x = (dstBuffer->width - new_w) / 2;
                            int offset_y = (dstBuffer->height - new_h) / 2;

                            cv::Mat resized;
                            cv::resize(fullRgbMat, resized, cv::Size(new_w, new_h));
                            resized.copyTo(dstMat(cv::Rect(offset_x, offset_y, new_w, new_h)));

                            printf("[CPU Fallback] SDK conversion succeeded.\n");
                            fflush(stdout);
                            yoloTaskQueue.push(dstBuffer);
                        } else {
                            printf("[CPU Fallback] SDK conversion failed! nRet [0x%x]\n", nRet);
                            fflush(stdout);
                            yoloPool.release_buffer(dstBuffer);
                        }
                    } catch (const std::exception& e) {
                        printf("CPU fallback exception: %s\n", e.what());
                        yoloPool.release_buffer(dstBuffer);
                    }
                }
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                printf("RGA conversion time: %llums\n", duration.count());
            } else {
                printf("[RGA] No available buffer in YOLO pool, dropping current frame.\n");
                fflush(stdout);
            }
            sourcePool.release_buffer(srcBuffer); //释放源DMA Buffer回内存池，避免内存泄漏
        }
    }
}

/*RGA格式转换*/
bool HikCamera::rga_YUV422_to_RGB888(DmaBuffer_t* srcBuf, DmaBuffer_t* dstBuf)
{
    if (!srcBuf || srcBuf->dmaFd <= 0 || !dstBuf || dstBuf->dmaFd <= 0) {
        std::cerr << "[RGA Error] Invalid DMA fd!" << std::endl;
        return false;
    }

    bool ret = true;
    IM_STATUS ret_rga = IM_STATUS_NOERROR;
    
    /* RGA 句柄*/
    rga_buffer_handle_t rga_handle_src = 0;
    rga_buffer_handle_t rga_handle_dst = 0;

    /* 格式定义 */
    int srcFmt = RK_FORMAT_YUYV_422; 
    int dstFmt = RK_FORMAT_RGB_888;

    /* 1. DMA Fd RGA Handle */
    im_handle_param_t in_param = { srcBuf->width, srcBuf->height, srcFmt };
    rga_handle_src = importbuffer_fd(srcBuf->dmaFd, &in_param);
    if (rga_handle_src <= 0) {
        std::cerr << "[RGA Error] Failed to import src fd!" << std::endl;
        return false;
    }

    im_handle_param_t dst_param = { dstBuf->width, dstBuf->height, dstFmt };
    rga_handle_dst = importbuffer_fd(dstBuf->dmaFd, &dst_param);
    if (rga_handle_dst <= 0) {
        std::cerr << "[RGA Error] Failed to import dst fd!" << std::endl;
        // 错误处理：释放已经成功导入的源句柄，避免资源泄漏
        releasebuffer_handle(rga_handle_src); 
        return false;
    }

    /* 2. 包装 RGA Buffer - 使用 wrapbuffer_handle_t(C链接, 参数确定: format在最后) 而非C++重载, 避免参数错位 */
    rga_buffer_t rga_buf_src = wrapbuffer_handle_t(rga_handle_src, srcBuf->width, srcBuf->height,
                                                   srcBuf->width, srcBuf->height, srcFmt);
    rga_buffer_t rga_buf_dst = wrapbuffer_handle_t(rga_handle_dst, dstBuf->width, dstBuf->height,
                                                   dstBuf->width, dstBuf->height, dstFmt);

    /* 3. 计算 Letterbox (等比例缩放) 参数 */
    float scale = std::min((float)dstBuf->width / srcBuf->width, 
                            (float)dstBuf->height / srcBuf->height);
    int scaled_w = srcBuf->width * scale;
    int scaled_h = srcBuf->height * scale;
    int offset_x = (dstBuf->width - scaled_w) / 2;
    int offset_y = (dstBuf->height - scaled_h) / 2;

    im_rect srect = {0, 0, srcBuf->width, srcBuf->height};
    im_rect drect = {offset_x, offset_y, scaled_w, scaled_h};

    /* 4. 背景填充 (Padding) */
    if (scaled_w != dstBuf->width || scaled_h != dstBuf->height) {
        im_rect dst_whole_rect = {0, 0, dstBuf->width, dstBuf->height};
        ret_rga = imfill(rga_buf_dst, dst_whole_rect, 0xFF727272); 
        if (ret_rga <= 0 && dstBuf->virtAddr != nullptr)
            memset(dstBuf->virtAddr, 114, dstBuf->bufferSize);
    }

    /* 5. RGA 终极处理：瞬间完成 格式转换 + 等比例缩放 + 偏移写入 */
    rga_buffer_t pat; memset(&pat, 0, sizeof(rga_buffer_t));
    im_rect prect; memset(&prect, 0, sizeof(im_rect));
    
    ret_rga = improcess(rga_buf_src, rga_buf_dst, pat, srect, drect, prect, 0);
    if (ret_rga <= 0) {
        std::cerr << "[RGA Error] improcess failed: " << imStrError((IM_STATUS)ret_rga) << std::endl;
        ret = false;
    }

    /* 6. 释放 RGA 句柄 (必须执行，否则内核内存泄漏！) */
    releasebuffer_handle(rga_handle_src);
    releasebuffer_handle(rga_handle_dst);

    return ret;
}

void __stdcall HikCamera::image_callback_ex2(MV_FRAME_OUT* pstFrame, void *pUser, bool bAutoFree) 
{
    HikCamera* pThis = static_cast<HikCamera*>(pUser); 

    DmaBuffer_t* buffer = pThis->sourcePool.get_buffer();
    if (buffer == nullptr) return;

    auto start_time = std::chrono::high_resolution_clock::now();
    memcpy(buffer->virtAddr, pstFrame->pBufAddr, pstFrame->stFrameInfo.nFrameLen);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "[time]: Data copy time: " << duration.count() << "ms." << std::endl;
    std::cout << std::flush; // 确保回调输出立即刷新

    {
        std::lock_guard<std::mutex> lock(pThis->queueMutex_);
        pThis->rgaTaskQueue_.push(buffer);
    }
    pThis->queueCv_.notify_one(); 
}
