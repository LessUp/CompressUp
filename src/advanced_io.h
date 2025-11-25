#pragma once

#include "types.h"

#include <cstddef>
#include <filesystem>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace compressup {

// 内存映射文件 (只读)
class MappedFile {
public:
    MappedFile() = default;
    explicit MappedFile(const std::filesystem::path& path);
    ~MappedFile();
    
    // 移动语义
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;
    
    // 禁止拷贝
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    
    // 访问数据
    const Byte* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool is_open() const { return data_ != nullptr; }
    
    // 作为string_view
    std::string_view as_string_view() const {
        return {reinterpret_cast<const char*>(data_), size_};
    }
    
    // 作为span
    ByteSpan as_span() const { return {data_, size_}; }

private:
    void close();
    
    Byte* data_ = nullptr;
    std::size_t size_ = 0;
    int fd_ = -1;
};

// 缓冲写入器
class BufferedWriter {
public:
    explicit BufferedWriter(const std::filesystem::path& path, 
                           std::size_t buffer_size = 64 * 1024);
    ~BufferedWriter();
    
    // 写入数据
    void write(const Byte* data, std::size_t size);
    void write(ByteSpan data);
    void write(const std::vector<Byte>& data);
    void write(std::string_view data);
    
    // 刷新缓冲区
    void flush();
    
    // 获取已写入字节数
    std::size_t bytes_written() const { return total_written_; }

private:
    std::vector<Byte> buffer_;
    std::size_t buffer_pos_ = 0;
    std::size_t total_written_ = 0;
    int fd_ = -1;
};

// 流式读取器
class StreamReader {
public:
    explicit StreamReader(const std::filesystem::path& path,
                         std::size_t buffer_size = 64 * 1024);
    ~StreamReader();
    
    // 读取数据
    std::size_t read(Byte* buffer, std::size_t size);
    std::vector<Byte> read(std::size_t size);
    
    // 是否到达文件末尾
    bool eof() const { return eof_; }
    
    // 获取已读取字节数
    std::size_t bytes_read() const { return total_read_; }
    
    // 获取文件大小
    std::size_t file_size() const { return file_size_; }

private:
    void fill_buffer();
    
    std::vector<Byte> buffer_;
    std::size_t buffer_pos_ = 0;
    std::size_t buffer_end_ = 0;
    std::size_t total_read_ = 0;
    std::size_t file_size_ = 0;
    bool eof_ = false;
    int fd_ = -1;
};

// 异步IO操作 (独立函数)
namespace async_io {

// 异步读取文件
std::future<std::vector<Byte>> read_async(const std::filesystem::path& path);

// 异步写入文件
std::future<void> write_async(const std::filesystem::path& path,
                              std::vector<Byte> data);

// 异步压缩文件
std::future<std::vector<Byte>> compress_file_async(
    const std::filesystem::path& path,
    const std::string& algorithm);

// 异步解压文件
std::future<std::string> decompress_file_async(const std::filesystem::path& path);

} // namespace async_io

} // namespace compressup
