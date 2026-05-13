#ifndef CAMERA_UTILS_H
#define CAMERA_UTILS_H

#include <QString>
#include <QDateTime>

class CameraUtils
{
public:
    CameraUtils();
    ~CameraUtils();

    // 相机工具函数
    static QString generateCameraID();
    static QString formatTimestamp(const QDateTime& timestamp);
    static bool validateCameraSettings(int width, int height, int fps);
    static QString getDefaultCameraPath();

    // 图像处理工具
    static QString formatImageSize(int width, int height);
    static QString formatFileSize(int64_t sizeInBytes);

    // 性能监控
    static void startPerformanceCounter();
    static double getElapsedTimeMs();
    static void logPerformance(const QString& operation, double elapsedMs);

private:
    static QDateTime m_startTime;
    static bool m_counterRunning;
};

#endif // CAMERA_UTILS_H