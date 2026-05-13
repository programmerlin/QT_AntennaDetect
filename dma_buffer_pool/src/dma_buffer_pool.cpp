#include "dma_buffer_pool.h"
#include <iostream>
#include "dma_alloc.h"
#include <memory>

DmaBufferPool::DmaBufferPool() 
{
    head_ = nullptr;
}

DmaBufferPool::~DmaBufferPool() 
{
    destroy_pool();
}

bool DmaBufferPool::alloc_pool(int count, int width, int height, BufferFormat format)
{
    int bufferSize;
    
    /* param check */
    if (!count) {
        std::cerr << "[err]: Buffer number can't be 0!" << std::endl;
        return false;  
    }
    if (!width || !height) {
        std::cerr << "[err]: Buffer's length of side can't be 0!" << std::endl;
        return false;       
    }

    // 使用强类型枚举 (enum class) 的作用域解析
    if (format == BufferFormat::YUV422)
        bufferSize = width * height * 2;
    else if (format == BufferFormat::RGB888)
        bufferSize = width * height * 3;
    else if (format == BufferFormat::NV12)
        bufferSize = width * height * 3 / 2; // NV12 是 1.5 倍
    else
        bufferSize = width * height * 2; // 默认兜底
    
    for (int i = 0; i < count; ++i) {
        std::unique_ptr<DmaBuffer_t> newBuf = std::make_unique<DmaBuffer_t>();//引入智能指针分配count块DmaBuffer_t结构体内存
        
        /* alloc DMA physical buffer */
        newBuf->bufferSize = bufferSize;
        newBuf->width = width;
        newBuf->height = height;
        
        //不使用缓存写入，让CPU跳过缓存区直接写入物理内存，确保DMA硬件能够第一时间看到最新数据
        //dma_buf_alloc函数最终返回的dmaFd会被DMA硬件直接使用，virtAddr则是CPU访问这块内存的虚拟地址，两者都必须正确分配
        int ret = dma_buf_alloc(DMA_HEAP_DMA32_UNCACHE_PATCH, newBuf->bufferSize, 
                                &(newBuf->dmaFd), &(newBuf->virtAddr));
        if (ret < 0 || newBuf->virtAddr == nullptr) {
            std::cerr << "[err]: Failed to allocate DMA buffer!" << std::endl;
            destroy_pool(); // 申请失败，安全撤退
            return false;
        }
        
        /* 成功分配 DMA 内存后，剥夺 unique_ptr 的控制权，交给原始链表 */
        DmaBuffer_t* rawBuf = newBuf.release();
        
        /* 登记到全量花名册，保证销毁时不漏一个 */
        allBuffers_.push_back(rawBuf);

        /* 头插法：确保头节点指针指向正确 */
        rawBuf->next = head_; //新节点的next指向当前的head_，形成链表连接
        head_ = rawBuf; //head_更新为新节点，完成头插
    }
    return true;
}

void DmaBufferPool::destroy_pool()
{
    std::lock_guard<std::mutex> lock(poolMutex_); //确保销毁过程线程安全，不让其他线程访问
    
    /* 遍历全量花名册，无差别击杀所有分配过的内存 */
    for (DmaBuffer_t* current : allBuffers_) {
        if (current != nullptr) {
            if (current->dmaFd >= 0 || current->virtAddr != nullptr) {
                dma_buf_free(current->bufferSize, &(current->dmaFd), &(current->virtAddr));
                std::cout << "DMA buffer freed. Fd: " << current->dmaFd << std::endl;
            }
            delete current; 
        }
    }
    
    allBuffers_.clear();
    head_ = nullptr;
    std::cout << "内存池已彻底销毁！" << std::endl;    
}

DmaBuffer_t* DmaBufferPool::get_buffer() 
{
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    if (head_ == nullptr) return nullptr; 

    DmaBuffer_t* buf = head_;
    head_ = head_->next; 
    
    buf->ifUse.store(true); // 标记为已占用
    buf->next = nullptr;    // 断开它与链表的联系，防止外部乱指
    
    return buf;
}

void DmaBufferPool::release_buffer(DmaBuffer_t* buf) 
{
    if (buf == nullptr) return;
    
    buf->ifUse.store(false); // 恢复为空闲状态

    std::lock_guard<std::mutex> lock(poolMutex_);
        
    /* 头插法：将归还的节点重新放回链表头部 */
    buf->next = head_;
    head_ = buf;
}
