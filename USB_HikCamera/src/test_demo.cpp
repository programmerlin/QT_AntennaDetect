#include "HikCamera.h"
#include <iostream>
#include <chrono>

int main ()
{
    bool ret;
    cv::Mat img;

    HikCamera uhv(0);
    ret = uhv.Camera_Init();
    if (!ret)
        return 0;

    ret = uhv.Camera_Open();
    if (!ret)
        return 0;

    auto start_time = std::chrono::high_resolution_clock::now();    
    ret = uhv.Image_Get(img, 1000);
    if (!ret)
        return 0;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Inference + Draw time: " << duration.count() << "ms" << std::endl;

    if (img.empty()) {
        std::cout << "Image is empty, cannot save!" << std::endl;
        return 0;
    }

    // Save the image
    bool result = cv::imwrite("hikvision_image.jpg", img);
    if (result)
        std::cout << "Image saved successfully!" << std::endl;
    else
        std::cerr << "Failed to save image." << std::endl;

    return 0;
}
