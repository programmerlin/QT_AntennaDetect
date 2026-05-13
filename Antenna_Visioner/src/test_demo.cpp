// #include "HikCamera.h"
// #include "Antenna_Visioner.h"
// #include <iostream>
// #include <chrono>


// int main ()
// {
//     bool ret;
//     std::vector<DetectionResult> results;

//     HikCamera uhv(0);
//     Antenna_Visioner anV("./yolov8_model/best.rknn", 
//                         "./yolov8_model/antenna.txt", 1024, 614, &uhv);

//     cv::Mat& img = anV.get_rga_mat();

//     anV.init_system();
//     uhv.open_grab_callback();
//     ret = uhv.Camera_Init();
//     if (!ret)
//         return 0;

//     ret = uhv.Camera_Open();
//     if (!ret)
//         return 0;

//     auto start_time = std::chrono::high_resolution_clock::now();    
//     // ret = uhv.read_img(img, 1000);

//     cv::Mat temp = cv::imread("./model/yolov8/antenna.jpg");
//     temp.copyTo(img);

//     if (!ret)
//         return 0;
//     auto end_time = std::chrono::high_resolution_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
//     std::cout << "采集一帧图像所需要的时间: " << duration.count() << "ms." << std::endl;

//     ret = anV.detect_once(img, results);
//     if (!ret)
//         return 0;

//     for (const auto& item : results) {
//         std::cout << "Detected: " << item.className 
//                   << ", Prob: " << item.prop << std::endl;
//     }

//     if (img.empty()) {
//         std::cout << "Image is empty, cannot save!" << std::endl;
//         return 0;
//     }

//     // 保存图片
//     bool result = cv::imwrite("output_image.jpg", img);
//     if (result)
//         std::cout << "Image saved successfully!" << std::endl;
//     else
//         std::cerr << "Failed to save image." << std::endl;

//     return 0;
// }

#include "HikCamera.h"
#include "Antenna_Visioner.h"
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>

int main() {
    bool ret;
    std::vector<DetectionResult> results;

    // 1. 实例化对象 (即使不用相机，Antenna_Visioner 构造函数可能也需要相机指针，传进去即可)
    HikCamera uhv(0); 
    // 这里的 640x640 是 YOLOv8 标准输入尺寸，建议根据你的 rknn 模型实际输入修改
    int target_w = 640;
    int target_h = 640;

    Antenna_Visioner anV("./yolov8_model/best.rknn", 
                        "./yolov8_model/antenna.txt", target_w, target_h, &uhv);

    // 2. 仅初始化推理系统 (跳过相机 Open/Grab 等逻辑)
    if (!anV.init_system()) {
        std::cerr << "Visioner System Init Failed!" << std::endl;
        return -1;
    }

    // 3. 加载本地图片
    std::string image_path = "./antenna.jpg"; // 确保路径正确
    cv::Mat src = cv::imread(image_path);
    if (src.empty()) {
        std::cerr << "Could not open or find the image: " << image_path << std::endl;
        return -1;
    }

    // 4. 获取推理用的 DMA 内存
    cv::Mat& img = anV.get_rga_mat(); // 获取你在 init_system 中绑定的那块内存

    // 直接将 src 缩放到这块 DMA 内存中
    // 这样 dmaImg.data 就指向了 DMA 地址，且数据已经被 resize 进去了
    cv::resize(src, img, cv::Size(target_w, target_h));

    // 5. 执行推理
    ret = anV.detect_once(img, results);

    // 6. 输出结果到终端
    if (results.empty()) {
        std::cout << "No objects detected." << std::endl;
    } else {
        for (const auto& item : results) {
            std::cout << "Detected: [ " << item.className 
                      << " ] Confidence: " << item.prop << std::endl;
        }
    }

    // 7. 保存结果图
    // 此时 img 已经被 detect_once 内部画上了框（如果你的代码逻辑里有画框操作）
    if (cv::imwrite("single_test_result.jpg", img)) {
        std::cout << "Result saved to: single_test_result.jpg" << std::endl;
    }

    return 0;
}
