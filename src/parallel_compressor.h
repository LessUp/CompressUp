#pragma once

#include "compressor.h"
#include "types.h"

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace compressup {

// 线程池，用于并行压缩
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = 0);
    ~ThreadPool();
    
    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // 提交任务
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
    
    std::size_t thread_count() const { return workers_.size(); }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};

// 并行压缩器包装器
class ParallelCompressor : public ICompressor {
public:
    ParallelCompressor(std::unique_ptr<ICompressor> base_compressor,
                       std::size_t block_size = 64 * 1024,
                       std::size_t num_threads = 0);
    
    std::string name() const override;
    std::vector<Byte> compress(std::string_view input) override;
    std::string decompress(const std::vector<Byte>& input) override;
    
    // 带进度回调的压缩/解压
    std::vector<Byte> compress_with_progress(std::string_view input, 
                                             ProgressCallback callback);
    std::string decompress_with_progress(const std::vector<Byte>& input,
                                         ProgressCallback callback);

private:
    std::unique_ptr<ICompressor> base_compressor_;
    std::size_t block_size_;
    std::size_t num_threads_;
    
    // 块头格式
    struct BlockHeader {
        std::uint64_t original_size;
        std::uint64_t compressed_size;
    };
};

// 简化的并行压缩接口
std::vector<Byte> parallel_compress(std::string_view input, 
                                    const std::string& algorithm,
                                    std::size_t block_size = 64 * 1024,
                                    std::size_t num_threads = 0);

std::string parallel_decompress(const std::vector<Byte>& input,
                                const std::string& algorithm);

} // namespace compressup

// 模板实现
namespace compressup {

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<std::invoke_result_t<F, Args...>> {
    
    using return_type = std::invoke_result_t<F, Args...>;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool: submit on stopped pool");
        }
        tasks_.emplace([task]() { (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

} // namespace compressup
