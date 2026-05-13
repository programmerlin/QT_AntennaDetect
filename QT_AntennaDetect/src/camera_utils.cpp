#include "../include/camera_utils.h"
#include <QDateTime>
#include <QDebug>
#include <chrono>

// 静态成员初始化
QDateTime CameraUtils::m_startTime = QDateTime::currentDateTime();
bool CameraUtils::m_counterRunning = false;

CameraUtils::CameraUtils()
{
}

CameraUtils::~CameraUtils()
{
}

QString CameraUtils::generateCameraID()
{
    // 生成唯一的相机ID
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString random = QString::number(qrand() % 1000, 10).rightJustified(3, '0');
    return QString("CAM_%1_%2").arg(timestamp).arg(random);
}

QString CameraUtils::formatTimestamp(const QDateTime& timestamp)
{
    return timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

bool CameraUtils::validateCameraSettings(int width, int height, int fps)
{
    // 验证相机设置是否有效
    if (width <= 0 || height <= 0 || fps <= 0) {
        return false;
    }

    // 常见的分辨率限制
    if (width > 3840 || height > 2160) {
        qWarning() << "警告: 请求的分辨率过高:" << width << "x" << height;
        return false;
    }

    // 最大帧率限制
    if (fps > 120) {
        qWarning() << "警告: 请求的帧率过高:" << fps;
        return false;
    }

    return true;
}

QString CameraUtils::getDefaultCameraPath()
{
    // 返回默认的相机路径
    return "/dev/video0";
}

QString CameraUtils::formatImageSize(int width, int height)
{
    return QString("%1 x %2").arg(width).arg(height);
}

QString CameraUtils::formatFileSize(int64_t sizeInBytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (sizeInBytes >= GB) {
        return QString::number(static_cast<double>(sizeInBytes) / GB, 'f', 2) + " GB";
    } else if (sizeInBytes >= MB) {
        return QString::number(static_cast<double>(sizeInBytes) / MB, 'f', 2) + " MB";
    } else if (sizeInBytes >= KB) {
        return QString::number(static_cast<double>(sizeInBytes) / KB, 'f', 2) + " KB";
    } else {
        return QString::number(sizeInBytes) + " B";
    }
}

void CameraUtils::startPerformanceCounter()
{
    m_startTime = QDateTime::currentDateTime();
    m_counterRunning = true;
}

double CameraUtils::getElapsedTimeMs()
{
    if (!m_counterRunning) {
        return 0.0;
    }

    QDateTime endTime = QDateTime::currentDateTime();
    qint64 elapsed = m_startTime.msecsTo(endTime);
    return static_cast<double>(elapsed);
}

void CameraUtils::logPerformance(const QString& operation, double elapsedMs)
{
    qDebug() << "Performance -" << operation << ":" << elapsedMs << "ms";
}