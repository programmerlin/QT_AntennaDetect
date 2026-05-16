#pragma once

#include <cstdio>
#include <cstring>
#include <memory>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include "../3rdparty/libhikvision/include/MvCameraControl.h"
#include "../dma_buffer_pool/include/dma_buffer_pool.h"
#include "ThreadSafeQueue.h"

/**
 * @brief 相机状态枚举
 */
enum class CameraStatus {
    WAIT_FOR_INIT = 0, ///< 等待初始化
    INITED,            ///< 已初始化
    OPENED,            ///< 已打开并开始读取
    CLOSED             ///< 已关闭
};

class HikCamera
{
public:
    /*
        * @brief: 构造函数
        * @param cameraIndex: 相机设备号，默认为0
    */
    explicit HikCamera(int cameraIndex);
    /*
        * @brief: 析构函数
    */
    ~HikCamera();

    /*使用Rule of Five规范，因为涉及底层硬件资源且涉及多线程，防止资源被意外移动或复制*/
    HikCamera(const HikCamera&) = delete;
    HikCamera& operator=(const HikCamera&) = delete;
    HikCamera(HikCamera&&) = delete;
    HikCamera& operator=(HikCamera&&) = delete;

    /*
        * @brief: 初始化相机并配置参数
        * @return: 初始化成功返回true，失败返回false
    */
    bool Camera_Init();

    /*
        * @brief: 打开设备并准备开始采集
        * @return: 打开成功返回true，失败返回false
    */
    bool Camera_Open();

    /*
        * @brief: 获取图像数据，通常需要在内部采集线程中调用
        * @return: 获取成功返回true，失败返回false
    */
    bool Image_Get(cv::Mat& img, unsigned int nMsec); //通过同步方式获取图像数据，此不需要调用

    /*
        * @brief: 将图像数据转换为OpenCV的Mat格式
        * @param stImageInfo: 图像信息
        * @param pData: 图像数据指针
        * @param dstImage: 目标Mat图像
        * @return: 转换成功返回true，失败返回false
    */
    bool convert_to_mat(MV_FRAME_OUT_INFO_EX& stImageInfo, std::unique_ptr<uint8_t[]>& pData, cv::Mat& dstImage);
    
    /*
        * @brief: 关闭设备并释放资源
        * @return: 关闭成功返回true，失败返回false
    */
    bool Camera_Close();

    /**
     * @brief 设置异步抓取图像的回调模式使用标志
     */
    void open_grab_callback();

    /**
     * @brief 从RGA转换队列获取最新的RGB888图像帧（回调模式专用）
     * @param rgbImage 输出图像（RGB888格式，640x640）
     * @param timeoutMs 超时时间（毫秒，当前实现为非阻塞轮询）
     * @return 成功获取到帧返回true，无可用帧返回false
     */
    bool get_converted_frame(cv::Mat& rgbImage, unsigned int timeoutMs);

    /**
     * @brief 创建RGA硬件加速的后台转换线程
     * @return true 成功 | false 失败
     */
    bool start_rga_thread();

    /**
     * @brief 完全停止并清理RGA转换线程
     */
    void stop_rga_thread();

    /**
     * @brief 开始图像采集（启动RGA线程 + 开始取流）
     * 与 stop_grabbing() 配对使用，实现管线的按需启停
     * @return true 成功 | false 失败
     */
    bool start_grabbing();

    /**
     * @brief 停止图像采集（停止取流 + 停止RGA线程 + 清空队列）
     * 暂停管线，释放CPU资源，保留DMA内存池和回调注册
     */
    void stop_grabbing();

    /* --- 引擎使用的内存资源 --- */
    // 留给外部业务（如 AntennaVisioner）直接进行处理的接口
    ThreadSafeQueue<DmaBuffer_t*> yoloTaskQueue; ///< 引擎生成的 YOLO 任务队列 (封装 RGB 图像)
    DmaBufferPool sourcePool;                    ///< 源图像内存 (存储 YUYV 原始图像)
    DmaBufferPool yoloPool;                      ///< YOLO 内存 (经 RGA 转换的 RGB 图像)

private:
    /**
     * @brief SDK底层抓取图像的回调函数 (静态成员)
     * @param pstFrame 帧数据信息
     * @param pUser 用户传入指针 (指向 USBHikvisioner 实例的 this 指针)
     * @param bAutoFree 是否由底层SDK自动释放内存
     */
    static void __stdcall image_callback_ex2(MV_FRAME_OUT* pstFrame, void *pUser, bool bAutoFree);

    /**
     * @brief RGA异步格式转换的线程函数体
     */
    void rga_dispatch_thread_func();

    /**
     * @brief 内部使用RGA硬件实现YUYV转RGB的转换函数
     * @param srcBuf 源 DMA Buffer
     * @param dstBuf 目标 DMA Buffer
     * @return true 转换成功 | false 转换失败
     */
    bool rga_YUV422_to_RGB888(DmaBuffer_t* srcBuf, DmaBuffer_t* dstBuf);

    /* --- 私有成员变量 --- */
    int cameraDeviceIndex_;                   ///< 相机设备索引
    void* deviceHandle_;                      ///< 海康 SDK 设备句柄
    std::unique_ptr<uint8_t[]> pData_;        ///< 轮询模式下的数据缓存 (独占智能指针)
    unsigned int nPayloadSize_;               ///< 单帧数据最大载荷大小
    MV_FRAME_OUT_INFO_EX stImageInfo_;        ///< 图像输出信息结构体
    CameraStatus cameraStatus_;               ///< 当前相机状态
    bool openGrabCallback_;                   ///< 是否启用了回调模式标记

    std::queue<DmaBuffer_t*> rgaTaskQueue_;   ///< RGA 线程的内部输入队列
    std::mutex queueMutex_;                   ///< 队列读写互斥锁
    std::condition_variable queueCv_;         ///< 队列唤醒条件变量
    std::atomic<bool> isRunning_{false};      ///< RGA 线程运行标志位
    std::thread rgaThread_;                   ///< RGA 消费者线程对象
};
