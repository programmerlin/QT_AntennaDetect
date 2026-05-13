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
 * @brief ���״̬ǿ����ö��
 */
enum class CameraStatus {
    WAIT_FOR_INIT = 0, ///< �ȴ���ʼ��
    INITED,            ///< �ѳ�ʼ��
    OPENED,            ///< �Ѵ򿪲�����ȡ��
    CLOSED             ///< �ѹر�
};

class HikCamera
{
public:
    /*
        * @brief: ���캯��
        * @param cameraIndex: ����豸�ţ�Ĭ����0
    */
    explicit HikCamera(int cameraIndex);
    /*
        * @brief: ��������
    */
    ~HikCamera();

    /*ʹ��Rule of Five�淶����Ϊ�漰���ײ�Ӳ��������̣߳���ֹ�������ƶ�*/
    HikCamera(const HikCamera&) = delete;
    HikCamera& operator=(const HikCamera&) = delete;
    HikCamera(HikCamera&&) = delete;
    HikCamera& operator=(HikCamera&&) = delete;

    /*
        * @brief: ��ʼ����������ò�����
        * @return: ��ʼ���ɹ�����true��ʧ�ܷ���false
    */
    bool Camera_Init();

    /*
        * @brief: ������豸��׼����ʼ�ɼ�
        * @return: �򿪳ɹ�����true��ʧ�ܷ���false
    */
    bool Camera_Open();

    /*
        * @brief: ��ȡͼ�����ݣ�������Ҫ���ڲ������ɼ��߳�
        * @return: ��ȡ�ɹ�����true��ʧ�ܷ���false
    */
    bool Image_Get(cv::Mat& img, unsigned int nMsec); //ͨ���������ݻ�ȡ��ͼ�����ݣ����ⲻ��Ҫ�Ŀ���

    /*
        * @brief: ��ͼ������ת��ΪOpenCV��Mat��ʽ
        * @param stImageInfo: ͼ����Ϣ
        * @param pData: ͼ������ָ��
        * @param dstImage: Ŀ��Matͼ��
        * @return: ת���ɹ�����true��ʧ�ܷ���false
    */
    bool convert_to_mat(MV_FRAME_OUT_INFO_EX& stImageInfo, std::unique_ptr<uint8_t[]>& pData, cv::Mat& dstImage);
    
    /*
        * @brief: �ر�����豸���ͷ���Դ
        * @return: �رճɹ�����true��ʧ�ܷ���false
    */
    bool Camera_Close();

    /**
     * @brief �����첽ץͼ�ص�ģʽ��ʹ�ܱ�־
     */
    void open_grab_callback();

    /**
     * @brief ���� RGA Ӳ�����ٵĺ�̨�ַ��߳�
     * @return true �ɹ� | false ʧ��
     */
    bool start_rga_thread();

    /**
     * @brief ��ȫֹͣ������ RGA �ַ��߳�
     */
    void stop_rga_thread();

    /* --- �����Ķ������ڴ����Դ --- */
    // �����ⲿҵ��㣨�� AntennaVisioner��ֱ�ӽ����Ļ�����ʩ
    ThreadSafeQueue<DmaBuffer_t*> yoloTaskQueue; ///< ��������� YOLO ����������� (װ�� RGB ����)
    DmaBufferPool sourcePool;                    ///< Դͼ�ڴ�� (������ YUYV ԭʼ����)
    DmaBufferPool yoloPool;                      ///< YOLO �ڴ�� (��� RGA ת����� RGB ����)

private:
    /**
     * @brief ���� SDK �ײ�ץͼ�ص����� (��̬��Ա)
     * @param pstFrame ֡������Ϣ
     * @param pUser ͸�����û�ָ�� (���� USBHikvisioner ʵ���� this ָ��)
     * @param bAutoFree �Ƿ��ɵײ� SDK �Զ��ͷ��ڴ�
     */
    static void __stdcall image_callback_ex2(MV_FRAME_OUT* pstFrame, void *pUser, bool bAutoFree);

    /**
     * @brief RGA �첽������ת�����̱߳��庯��
     */
    void rga_dispatch_thread_func();

    /**
     * @brief �ڲ����� RGA Ӳ��ʵ�� YUYV �� RGB ��ת��������
     * @param srcBuf Դ�� DMA Buffer
     * @param dstBuf Ŀ��� DMA Buffer
     * @return true ת���ɹ� | false ת��ʧ��
     */
    bool rga_YUV422_to_RGB888(DmaBuffer_t* srcBuf, DmaBuffer_t* dstBuf);

    /* --- ˽�г�Ա���� --- */
    int cameraDeviceIndex_;                   ///< ����豸����
    void* deviceHandle_;                      ///< ���� SDK �豸���
    std::unique_ptr<uint8_t[]> pData_;        ///< ��ѯģʽ�µ����ݻ��� (��ռ����ָ��)
    unsigned int nPayloadSize_;               ///< ��֡��������غɴ�С(��ǰ������ÿ֡���ݴ�С����λ���ֽ�)
    MV_FRAME_OUT_INFO_EX stImageInfo_;        ///< ͼ�������Ϣ�ṹ��
    CameraStatus cameraStatus_;               ///< ��ǰ���״̬
    bool openGrabCallback_;                   ///< �Ƿ������˻ص�ģʽ���

    std::queue<DmaBuffer_t*> rgaTaskQueue_;   ///< RGA �̵߳��ڲ��������
    std::mutex queueMutex_;                   ///< ���ж�д������
    std::condition_variable queueCv_;         ///< ���л�����������
    std::atomic<bool> RGAisRunning_{false};      ///< RGA �߳����б�־λ
    std::thread rgaThread_;                   ///< RGA �������̶߳���
};
