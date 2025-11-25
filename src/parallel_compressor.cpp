#include "parallel_compressor.h"
#include "registry.h"

#include <algorithm>
#include <queue>
#include <stdexcept>

namespace compressup {

// ThreadPool 实现
ThreadPool::ThreadPool(std::size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }
    
    for (std::size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });
                    
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// ParallelCompressor 实现
ParallelCompressor::ParallelCompressor(
    std::unique_ptr<ICompressor> base_compressor,
    std::size_t block_size,
    std::size_t num_threads)
    : base_compressor_(std::move(base_compressor))
    , block_size_(block_size)
    , num_threads_(num_threads) {
    
    if (num_threads_ == 0) {
        num_threads_ = std::thread::hardware_concurrency();
        if (num_threads_ == 0) num_threads_ = 4;
    }
}

std::string ParallelCompressor::name() const {
    return "parallel_" + base_compressor_->name();
}

std::vector<Byte> ParallelCompressor::compress(std::string_view input) {
    return compress_with_progress(input, nullptr);
}

std::vector<Byte> ParallelCompressor::compress_with_progress(
    std::string_view input, ProgressCallback callback) {
    
    if (input.empty()) {
        return {};
    }
    
    // 如果数据太小，直接使用单线程
    if (input.size() <= block_size_) {
        auto result = base_compressor_->compress(input);
        
        std::vector<Byte> output;
        
        // 写入魔数和版本
        output.push_back(0xC4);  // 并行压缩魔数
        output.push_back(0x01);  // 版本号
        
        // 写入原始总长度
        std::uint64_t total_size = input.size();
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<Byte>(total_size >> (i * 8)));
        }
        
        // 写入块数量
        std::uint32_t block_count = 1;
        for (int i = 0; i < 4; ++i) {
            output.push_back(static_cast<Byte>(block_count >> (i * 8)));
        }
        
        // 写入块头
        std::uint64_t orig_size = input.size();
        std::uint64_t comp_size = result.size();
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<Byte>(orig_size >> (i * 8)));
        }
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<Byte>(comp_size >> (i * 8)));
        }
        
        output.insert(output.end(), result.begin(), result.end());
        
        if (callback) callback(input.size(), input.size());
        return output;
    }
    
    // 分块
    std::vector<std::string_view> blocks;
    for (std::size_t i = 0; i < input.size(); i += block_size_) {
        std::size_t len = std::min(block_size_, input.size() - i);
        blocks.push_back(input.substr(i, len));
    }
    
    // 并行压缩
    ThreadPool pool(num_threads_);
    std::vector<std::future<std::vector<Byte>>> futures;
    std::atomic<std::size_t> processed{0};
    
    for (const auto& block : blocks) {
        auto compressor = create_compressor(base_compressor_->name());
        futures.push_back(pool.submit([this, block, &processed, &callback, 
                                       total = input.size(),
                                       comp = std::move(compressor)]() mutable {
            auto result = comp->compress(block);
            if (callback) {
                std::size_t done = processed.fetch_add(block.size()) + block.size();
                callback(done, total);
            }
            return result;
        }));
    }
    
    // 收集结果
    std::vector<std::vector<Byte>> compressed_blocks;
    for (auto& f : futures) {
        compressed_blocks.push_back(f.get());
    }
    
    // 构建输出
    std::vector<Byte> output;
    
    // 写入魔数和版本
    output.push_back(0xC4);
    output.push_back(0x01);
    
    // 写入原始总长度
    std::uint64_t total_size = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<Byte>(total_size >> (i * 8)));
    }
    
    // 写入块数量
    std::uint32_t block_count = static_cast<std::uint32_t>(blocks.size());
    for (int i = 0; i < 4; ++i) {
        output.push_back(static_cast<Byte>(block_count >> (i * 8)));
    }
    
    // 写入每个块的头和数据
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        std::uint64_t orig_size = blocks[i].size();
        std::uint64_t comp_size = compressed_blocks[i].size();
        
        for (int j = 0; j < 8; ++j) {
            output.push_back(static_cast<Byte>(orig_size >> (j * 8)));
        }
        for (int j = 0; j < 8; ++j) {
            output.push_back(static_cast<Byte>(comp_size >> (j * 8)));
        }
        
        output.insert(output.end(), 
                     compressed_blocks[i].begin(), 
                     compressed_blocks[i].end());
    }
    
    return output;
}

std::string ParallelCompressor::decompress(const std::vector<Byte>& input) {
    return decompress_with_progress(input, nullptr);
}

std::string ParallelCompressor::decompress_with_progress(
    const std::vector<Byte>& input, ProgressCallback callback) {
    
    if (input.empty()) {
        return {};
    }
    
    if (input.size() < 14) {
        throw std::runtime_error("ParallelCompressor: input too short");
    }
    
    const Byte* data = input.data();
    const Byte* end = data + input.size();
    
    // 验证魔数和版本
    if (*data++ != 0xC4) {
        throw std::runtime_error("ParallelCompressor: invalid magic number");
    }
    if (*data++ != 0x01) {
        throw std::runtime_error("ParallelCompressor: unsupported version");
    }
    
    // 读取原始总长度
    std::uint64_t total_size = 0;
    for (int i = 0; i < 8; ++i) {
        total_size |= static_cast<std::uint64_t>(*data++) << (i * 8);
    }
    
    // 读取块数量
    std::uint32_t block_count = 0;
    for (int i = 0; i < 4; ++i) {
        block_count |= static_cast<std::uint32_t>(*data++) << (i * 8);
    }
    
    // 读取所有块
    std::vector<std::pair<std::uint64_t, std::vector<Byte>>> blocks;
    for (std::uint32_t i = 0; i < block_count; ++i) {
        if (data + 16 > end) {
            throw std::runtime_error("ParallelCompressor: incomplete block header");
        }
        
        std::uint64_t orig_size = 0;
        for (int j = 0; j < 8; ++j) {
            orig_size |= static_cast<std::uint64_t>(*data++) << (j * 8);
        }
        
        std::uint64_t comp_size = 0;
        for (int j = 0; j < 8; ++j) {
            comp_size |= static_cast<std::uint64_t>(*data++) << (j * 8);
        }
        
        if (data + comp_size > end) {
            throw std::runtime_error("ParallelCompressor: incomplete block data");
        }
        
        blocks.emplace_back(orig_size, std::vector<Byte>(data, data + comp_size));
        data += comp_size;
    }
    
    // 并行解压
    ThreadPool pool(num_threads_);
    std::vector<std::future<std::string>> futures;
    std::atomic<std::size_t> processed{0};
    
    for (const auto& [orig_size, comp_data] : blocks) {
        auto compressor = create_compressor(base_compressor_->name());
        futures.push_back(pool.submit([this, orig_size, &comp_data, 
                                       &processed, &callback, total_size,
                                       comp = std::move(compressor)]() mutable {
            auto result = comp->decompress(comp_data);
            if (callback) {
                std::size_t done = processed.fetch_add(orig_size) + orig_size;
                callback(done, total_size);
            }
            return result;
        }));
    }
    
    // 收集结果
    std::string output;
    output.reserve(total_size);
    for (auto& f : futures) {
        output += f.get();
    }
    
    return output;
}

// 简化接口实现
std::vector<Byte> parallel_compress(std::string_view input,
                                    const std::string& algorithm,
                                    std::size_t block_size,
                                    std::size_t num_threads) {
    auto base = create_compressor(algorithm);
    ParallelCompressor parallel(std::move(base), block_size, num_threads);
    return parallel.compress(input);
}

std::string parallel_decompress(const std::vector<Byte>& input,
                                const std::string& algorithm) {
    auto base = create_compressor(algorithm);
    ParallelCompressor parallel(std::move(base));
    return parallel.decompress(input);
}

} // namespace compressup
