#pragma once

#include <string>
#include <vector>
#include "../USB_HikCamera/include/HikCamera.h"
#include "yolov8.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp> 
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include "../3rdparty/utils/image_drawing.h"
// RKNN 和 RGA 头文件
#include "postprocess.h"

#include "../3rdparty/allocator/include/dma_alloc.h"

/**
 * @brief 目标检测结果结构体
 **/
struct DetectionResult {
    std::string className; ///< 类名（如 "person", "car"）
    int classId;           ///< 类 ID
    float prop;            ///< 置信度 (Probability)
    int box[4];            ///< 边界框坐标 [x1, y1, x2, y2]
};

#define TIME_CONM_CALC 1// 计时标志位

class Antenna_Visioner
{
public:
    /**
    @brief: 构造函数
    @param modelPath: RKNN 模型文件路径
    @param labelPath: 标签文件路径
    @param width: 图像宽度
    @param height: 图像高度
    @param camera: 相机指针
    @return: 无
    **/
    Antenna_Visioner(const std::string& modelPath, 
                    const std::string& labelPath, 
                    int width,
                    int height,
                    HikCamera* camera);

    ~Antenna_Visioner();

    /*Rule of five��׼*/
    Antenna_Visioner(const Antenna_Visioner&) = delete;
    Antenna_Visioner& operator=(const Antenna_Visioner&) = delete;
    Antenna_Visioner(Antenna_Visioner&&) = delete;
    Antenna_Visioner& operator=(Antenna_Visioner&&) = delete;

    /**
    @brief: ��ʼ��ϵͳ��Դ
    @return: ��ʼ���ɹ�����true��ʧ�ܷ���false
    **/
    bool init_system();

    /** 
    @brief: ִ�е�������ʶ��
    @param outputFrame: ��������������˼�������ͼ�� (BGR��ʽ����ֱ�ӹ�ǰ��/Qt��ʾ)
    @param outResults: ���������ʶ�𵽵�Ŀ����Ϣ�б�
    @return: �����ɹ�����true��ʧ�ܷ���false
    **/
   bool detect_once(cv::Mat& outputFrame, std::vector<DetectionResult>& outResults);

   /** 
    @brief: ��ȡRGAͼ�����
    @return: RGAͼ���������
    **/
   cv::Mat& get_rga_mat(void);

private:
    /* --- ��Ա���� --- */
    std::string modelPath_;         ///< RKNN ģ��·��
    std::string labelPath_;         ///< ��ǩ�ļ�·��
    int videoDeviceIndex_;          ///< ��Ƶ�豸����

    rknn_app_context_t rknnAppCtx_; ///< RKNN ģ������������

    std::mutex mtx_;                ///< �̻߳�����

    cv::Mat rgaMat_;                ///< �� DMA �����ַ�� OpenCV Mat

    int width_;                     ///< ͼ�����
    int height_;                    ///< ͼ��߶�
    int bufSize_;                   ///< DMA ���������ֽڴ�С
    
    /* DMA �����Դ */
    int dmaFd_;                     ///< DMA �ڴ��ļ�������
    void* virtAddr_;                ///< DMA �����ַָ��
    
    cv::VideoCapture cap_;          ///< OpenCV ��ͨ����ͷ����

    HikCamera* HikCamera_ = nullptr; ///< �������ʵ��ָ��
};

