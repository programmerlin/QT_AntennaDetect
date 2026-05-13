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
    : cameraDeviceIndex_(cameraIndex),  //��˽�б������г�ʼ���б���ֵ
      deviceHandle_(nullptr),
      nPayloadSize_(0),
      cameraStatus_(CameraStatus::WAIT_FOR_INIT),
      openGrabCallback_(false) {}

HikCamera::~HikCamera()
{
    Camera_Close();

    /* ���پ�� */
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

    /* �豸ö�� */
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet)
    {
        printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
        return false;
    }

    /* ��ʾ�豸��Ϣ */
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

    /* ����豸�Ƿ�ɷ��� */
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

    // ѡ���豸���������
    nRet = MV_CC_CreateHandle(&deviceHandle_, stDeviceList.pDeviceInfo[cameraDeviceIndex_]);
    if (MV_OK != nRet)
    {
        printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
        return false;
    }

    cameraStatus_ = CameraStatus::INITED;   //ǿö�����ͱ�������
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

    /* ���豸 */
    nRet = MV_CC_OpenDevice(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("OpenDevice fail! nRet [%x]\n", nRet);
        return false;
    }

    /*���豸�����������*/
    //����ͼ�������ʽΪYUV422_YUYV_Packed��������
    nRet = MV_CC_SetEnumValue(deviceHandle_, "PixelFormat", PixelType_Gvsp_YUV422_YUYV_Packed);
    if (MV_OK != nRet)
    {
        printf("Set PixelFormat fail! nRet [%x]\n", nRet);
    }
    else
    {
        printf("[Config] Set PixelFormat to YUV422_YUYV_Packed success!\n");
    }

    //�����Զ�����
    nRet = MV_CC_SetEnumValue(deviceHandle_, "GainAuto", 2);//�����Զ�����ģʽ��Continuous��ʾ�����Զ�����
    if(MV_OK != nRet)
    {
        printf("[Warning] Set GainAuto fail! , trying manual gain 24dB...\n");
        nRet = MV_CC_SetEnumValue(deviceHandle_, "GainAuto", 0);//�ر��Զ�����
        if (MV_OK != nRet) {
            printf("[Warning] Set GainAuto to Off fail! nRet [%x]\n", nRet);
        } else {
            nRet = MV_CC_SetFloatValue(deviceHandle_, "Gain", 24.0f); //�ֶ���������Ϊ10dB
            if (MV_OK != nRet) {
                printf("[Warning] Set manual Gain to 10dB fail! nRet [%x]\n", nRet);
            } else {
                printf("[Config] Set manual Gain to 10dB success.\n");
            }
        }
    }
    else{
        printf("[Config] Set GainAuto to true success!\n");
    }

    //�����Զ��ع�
    nRet = MV_CC_SetEnumValue(deviceHandle_, "ExposureAuto", 2);//�ع��Զ�����ģʽ��Continuous��ʾ�����Զ��ع�
    if(MV_OK != nRet)
    {
        printf("[Warning] Set ExposureAuto fail! , trying manual exposure 50000...\n");
        nRet = MV_CC_SetEnumValue(deviceHandle_, "ExposureAuto", 0);//�ر��Զ��ع�
        if (MV_OK != nRet) {
            printf("[Warning] Set ExposureAuto to Off fail! nRet [%x]\n", nRet);
        } else {
            nRet = MV_CC_SetFloatValue(deviceHandle_, "ExposureTime", 50000.0f); //�ֶ��ع�����Ϊ50000us
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

    //��΢�ȴ�һ�����Զ��㷨������������
    usleep(500000); // �ȴ� 0.5 ��

    //���ô���ģʽΪoff
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

    //���� Binning (���ͷֱ��ʣ����֡��)(���غϲ�)
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

    /* ��ȡͼ���С����ǰ�����ڴ���䣬Ϊ֮���ͼ������׼�� */
    MVCC_INTVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_INTVALUE));
    nRet = MV_CC_GetIntValue(deviceHandle_, "PayloadSize", &stParam);
    if (MV_OK != nRet)
    {
        printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
        return false;
    }
    nPayloadSize_ = stParam.nCurValue;
    printf("PayloadSize = %d.\n", nPayloadSize_);//��ӡ��ǰ������������µ�ÿ֡���ݴ�С

    // ��ȡ�����ǰͼƬ����ʵ������
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

    /* ͼ���ڴ�Ԥ���䣬�ڿ�ʼ�ص�֮ǰ������ڴ� */
    memset(&stImageInfo_, 0, sizeof(MV_FRAME_OUT_INFO_EX));
    // ����ָ������ڴ棬����ʵ��������ʱ���Զ��ͷ��ڴ棬ͬʱ�������ݸ�ʽΪuint8_t���飬�ʺϴ洢ͼ��Ҷ�ֵ�ֽ�����
    pData_ = std::make_unique<uint8_t[]>(nPayloadSize_); 
    if (nullptr == pData_) {
        printf("Allocate memory failed.\n");
        return false;
    }

    // ע��ץͼ�ص�
    if(openGrabCallback_){
        if(!sourcePool.alloc_pool(4, stWidth.nCurValue, stHeight.nCurValue, BufferFormat::YUV422)) return false;//Ԥ����4��Դͼ�ڴ��
        if(!yoloPool.alloc_pool(4, 640, 640, BufferFormat::RGB888)) return false;//Ԥ����4��YOLO�ڴ��

        start_rga_thread(); //����RGA�߳�

        nRet = MV_CC_RegisterImageCallBackEx2(deviceHandle_, HikCamera::image_callback_ex2, this, true);
        if(MV_OK != nRet) {
            printf("Register image callback failed! nRet [0x%x]\n", nRet);
            stop_rga_thread(); //ע��ص�ʧ�ܣ�ֹͣRGA�߳�
            sourcePool.destroy_pool(); //����Դͼ�ڴ��
            yoloPool.destroy_pool(); //����YOLO�ڴ��
            MV_CC_CloseDevice(deviceHandle_); //�ر��豸
            return false;
        }
    }

        // ��ʼȡ��
        nRet = MV_CC_StartGrabbing(deviceHandle_);
        if (MV_OK != nRet)
        {
            printf("MV_CC_StartGrabbing fail! nRet [%x]\n", nRet);
            if(openGrabCallback_){
                stop_rga_thread(); //ȡ��ʧ�ܣ�ֹͣRGA�߳�
                sourcePool.destroy_pool(); //����Դͼ�ڴ��
                yoloPool.destroy_pool(); //����YOLO�ڴ��
                MV_CC_CloseDevice(deviceHandle_); //�ر��豸
            }
            return false;
        }
        cameraStatus_ = CameraStatus::OPENED;   //ǿö�����ͱ�������
        return true;
}

//��YUV����ͨ��OpenCVת��ΪMat��ʽ�����������������������Ƿ�����������ȡ��ģʽ����֮ǰ��ȡ��ģʽ���⣩
bool HikCamera::Image_Get(cv::Mat& img, unsigned int nMsec)
{
    int nRet;

    if (cameraStatus_ != CameraStatus::OPENED) {
        printf("Camera needs to been opened.\n");
        return false;
    }
    
    /* ��ȡһ֡ͼ�񣬳�ʱʱ��1000ms */
    nRet = MV_CC_GetOneFrameTimeout(deviceHandle_, pData_.get(), nPayloadSize_, &stImageInfo_, nMsec);
    if (MV_OK != nRet) {
        printf("Get Frame fail! nRet: [0x%x]\n.", nRet);
        return false;
    }
    printf("Get One Frame: Width[%d], Height[%d], FrameNum[%d]\n",
    stImageInfo_.nWidth, stImageInfo_.nHeight, stImageInfo_.nFrameNum);

    /* ��ʽת�� */
    return convert_to_mat(stImageInfo_, pData_, img);
}

bool HikCamera::convert_to_mat(MV_FRAME_OUT_INFO_EX& stImageInfo, std::unique_ptr<uint8_t[]>& pData, cv::Mat& dstImage)
{
    if (stImageInfo.nWidth <= 0 || stImageInfo.nHeight <= 0 || !pData) {
        printf("Invalid image info or data pointer.\n");
        return false;
    }
    
    dstImage.create(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3);
    unsigned int RGBDataSize = stImageInfo.nWidth * stImageInfo.nHeight * 3; // RGB888��ʽÿ����3�ֽ�

    // ch:���ظ�ʽת�� | en:Convert pixel format 
    MV_CC_PIXEL_CONVERT_PARAM_EX stConvertParam = {0};

    stConvertParam.nWidth = stImageInfo.nWidth;                 //ch:ͼ��� | en:image width
    stConvertParam.nHeight = stImageInfo.nHeight;               //ch:ͼ��� | en:image height
    stConvertParam.pSrcData = pData.get();                         //ch:�������ݻ��� | en:input data buffer
    stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;         //ch:�������ݴ�С | en:input data size
    stConvertParam.enSrcPixelType = stImageInfo.enPixelType;    //ch:�������ظ�ʽ | en:input pixel format
    stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;                         //ch:������ظ�ʽ | en:output pixel format
    stConvertParam.pDstBuffer = dstImage.data;                               //ch:������ݻ��� | en:output data buffer
    stConvertParam.nDstBufferSize = RGBDataSize;                       //ch:��������С | en:output buffer size
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
    if (cameraStatus_ != CameraStatus::CLOSED) {
        printf("Camera has been closed already.\n");
        return true;
    }

    if(cameraStatus_ != CameraStatus::WAIT_FOR_INIT || cameraStatus_ != CameraStatus::INITED) {
        printf("Camera needs to been opened.\n");
        return false;
    }

    // ch:ֹͣȡ�� | en:Stop grab image
    nRet = MV_CC_StopGrabbing(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        return false;
    }

    // ch:�ر��豸 | Close device
    nRet = MV_CC_CloseDevice(deviceHandle_);
    if (MV_OK != nRet)
    {
        printf("ClosDevice fail! nRet [0x%x]\n", nRet);
        return false;
    }

    if(openGrabCallback_){
        stop_rga_thread(); //ֹͣRGA�߳�
        /*��ղ���ָ��*/
        while (!rgaTaskQueue_.empty())
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            rgaTaskQueue_.pop(); //���RGA�̵߳�������У�����δ������ͼ������
        }
        
        sourcePool.destroy_pool(); //����Դͼ�ڴ��
        yoloPool.destroy_pool(); //����YOLO�ڴ��
    }

    cameraStatus_ = CameraStatus::CLOSED;   //ǿö�����ͱ�������
    return true;
}

void HikCamera::open_grab_callback()
{
    openGrabCallback_ = true;
}

bool HikCamera::start_rga_thread()
{
    if(RGAisRunning_){
        printf("RGA thread is already running.\n");
        return true;
    }

    RGAisRunning_ = true;
    rgaThread_ = std::thread(&HikCamera::rga_dispatch_thread_func, this);
    printf("RGA �����߳������ɹ���\n");
    return true;
}

void HikCamera::stop_rga_thread()
{
    if (!RGAisRunning_) {
        printf("RGA thread is not running.\n");
        return;
    }

    RGAisRunning_ = false;
    queueCv_.notify_all(); //�����̣߳�ȷ���߳��ܼ�ʱ��Ӧֹͣ�ź�

    if (rgaThread_.joinable()) {
        rgaThread_.join();
        printf("RGA �����߳��Ѱ�ȫֹͣ�����ա�\n");
    }
}

//RGA�̴߳�����������Դͼ���ݽ�������ת��
void HikCamera::rga_dispatch_thread_func()
{
    while(RGAisRunning_){
        DmaBuffer_t* srcBuffer = nullptr; //��Դͼ�ڴ�ػ�ȡһ����е�DMA��������׼�������������
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this](){ return !rgaTaskQueue_.empty() || !RGAisRunning_; });//rgaTaskQueue_��װ���ԭʼ���ݵĶ��У����Ե����зǿջ����߳�ֹͣ��־������ʱ������
            if (!RGAisRunning_ && rgaTaskQueue_.empty()) break; //�߳�ֹͣ��־λ������ͬʱ������������������ݣ��˳��߳�
            srcBuffer = rgaTaskQueue_.front();
            rgaTaskQueue_.pop();
        }

        if (srcBuffer != nullptr) {
            DmaBuffer_t* dstBuffer = yoloPool.get_buffer(); //��YOLO�ڴ�ػ�ȡһ����е�DMA��������׼���洢ת���������
            if (dstBuffer != nullptr) {
                auto start_time = std::chrono::high_resolution_clock::now();
                //ִ��RGA����ת������srcBuffer->dstBuffer
                bool rgaSuccess = rga_YUV422_to_RGB888(srcBuffer, dstBuffer);
                if (rgaSuccess) {
                    std::lock_guard<std::mutex> lock(queueMutex_); 
                    rgaTaskQueue_.push(dstBuffer); //��ת��������ݷ���RGA�̵߳�������У��ȴ������㷨����
                } else {
                    printf("RGA conversion failed for current buffer.\n");
                    yoloPool.release_buffer(dstBuffer); //ת��ʧ�ܣ��ͷ�Ŀ�껺�������ڴ��
                }
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                printf("RGA conversion time: %llums\n", duration.count());
            } else {
                printf("No available buffer in YOLO pool, dropping current frame.\n");
            }
            sourcePool.release_buffer(srcBuffer); //�����굱ǰԴͼ���ݣ��ͷ�Դ���������ڴ��
        }
    }
}

/*RGA����ת������*/
bool HikCamera::rga_YUV422_to_RGB888(DmaBuffer_t* srcBuf, DmaBuffer_t* dstBuf)
{
    if (!srcBuf || srcBuf->dmaFd <= 0 || !dstBuf || dstBuf->dmaFd <= 0) {
        std::cerr << "[RGA Error] Invalid DMA fd!" << std::endl;
        return false;
    }

    bool ret = true;
    IM_STATUS ret_rga = IM_STATUS_NOERROR;
    
    /* RGA ��� */
    rga_buffer_handle_t rga_handle_src = 0;
    rga_buffer_handle_t rga_handle_dst = 0;

    /* ��ʽ���壺Դͷ�Ǻ����� YUYV��Ŀ���� YOLO �� RGB888 */
    int srcFmt = RK_FORMAT_YUYV_422; 
    int dstFmt = RK_FORMAT_RGB_888;

    /* 1. ���� DMA Fd ���� RGA Handle */
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
        // ���������ͷ��������Դ�����ִ�е�����˵��rga_handle_src�����뵽����rga_handle_dst����ʧ���ˣ�����ֻ��Ҫ�ͷ�rga_handle_src�������ں��ڴ�й©
        releasebuffer_handle(rga_handle_src); 
        return false;
    }

    /* 2. ��װ RGA Buffer */
    rga_buffer_t rga_buf_src = wrapbuffer_handle(rga_handle_src, srcBuf->width, srcBuf->height, 
                                                srcFmt, srcBuf->width, srcBuf->height);
    rga_buffer_t rga_buf_dst = wrapbuffer_handle(rga_handle_dst, dstBuf->width, dstBuf->height,
                                                dstFmt, dstBuf->width, dstBuf->height);

    /* 3. ���� Letterbox (�ȱ�������) ���� */
    float scale = std::min((float)dstBuf->width / srcBuf->width, 
                            (float)dstBuf->height / srcBuf->height);
    int scaled_w = srcBuf->width * scale;
    int scaled_h = srcBuf->height * scale;
    int offset_x = (dstBuf->width - scaled_w) / 2;
    int offset_y = (dstBuf->height - scaled_h) / 2;

    im_rect srect = {0, 0, srcBuf->width, srcBuf->height};
    im_rect drect = {offset_x, offset_y, scaled_w, scaled_h};

    /* 4. ������� (Padding) */
    if (scaled_w != dstBuf->width || scaled_h != dstBuf->height) {
        im_rect dst_whole_rect = {0, 0, dstBuf->width, dstBuf->height};
        ret_rga = imfill(rga_buf_dst, dst_whole_rect, 0xFF727272); 
        if (ret_rga <= 0 && dstBuf->virtAddr != nullptr)
            memset(dstBuf->virtAddr, 114, dstBuf->bufferSize);
    }

    /* 5. RGA �ռ�������˲����� ��ʽת�� + �ȱ������� + ƫ��д�� */
    rga_buffer_t pat; memset(&pat, 0, sizeof(rga_buffer_t));
    im_rect prect; memset(&prect, 0, sizeof(im_rect));
    
    ret_rga = improcess(rga_buf_src, rga_buf_dst, pat, srect, drect, prect, 0);
    if (ret_rga <= 0) {
        std::cerr << "[RGA Error] improcess failed: " << imStrError((IM_STATUS)ret_rga) << std::endl;
        ret = false;
    }

    /* 6. �ͷ� RGA ��� (����ִ�У������ں��ڴ�й©��) */
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
    std::cout << "[time]: ��������һ֡ԭʼ���ݵ�ʱ��Ϊ: " << duration.count() << "ms." << std::endl;

    {
        std::lock_guard<std::mutex> lock(pThis->queueMutex_);
        pThis->rgaTaskQueue_.push(buffer);
    }
    pThis->queueCv_.notify_one(); 
}
