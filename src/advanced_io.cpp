#include "advanced_io.h"
#include "registry.h"
#include "container.h"
#include "file_io.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <future>
#include <stdexcept>
#include <thread>

namespace compressup {

// MappedFile 实现
MappedFile::MappedFile(const std::filesystem::path& path) {
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("MappedFile: failed to open file: " + path.string());
    }
    
    struct stat st;
    if (::fstat(fd_, &st) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("MappedFile: failed to get file size: " + path.string());
    }
    
    size_ = static_cast<std::size_t>(st.st_size);
    
    if (size_ > 0) {
        void* addr = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (addr == MAP_FAILED) {
            ::close(fd_);
            fd_ = -1;
            throw std::runtime_error("MappedFile: mmap failed: " + path.string());
        }
        data_ = static_cast<Byte*>(addr);
        
        // 建议内核顺序读取
        ::madvise(data_, size_, MADV_SEQUENTIAL);
    }
}

MappedFile::~MappedFile() {
    close();
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_)
    , size_(other.size_)
    , fd_(other.fd_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        close();
        data_ = other.data_;
        size_ = other.size_;
        fd_ = other.fd_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

void MappedFile::close() {
    if (data_ && size_ > 0) {
        ::munmap(data_, size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
    data_ = nullptr;
    size_ = 0;
    fd_ = -1;
}

// BufferedWriter 实现
BufferedWriter::BufferedWriter(const std::filesystem::path& path, 
                               std::size_t buffer_size)
    : buffer_(buffer_size) {
    
    fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("BufferedWriter: failed to open file: " + path.string());
    }
}

BufferedWriter::~BufferedWriter() {
    if (fd_ >= 0) {
        try {
            flush();
        } catch (...) {
            // 析构函数中不抛出异常
        }
        ::close(fd_);
    }
}

void BufferedWriter::write(const Byte* data, std::size_t size) {
    while (size > 0) {
        std::size_t available = buffer_.size() - buffer_pos_;
        std::size_t to_copy = std::min(size, available);
        
        std::memcpy(buffer_.data() + buffer_pos_, data, to_copy);
        buffer_pos_ += to_copy;
        data += to_copy;
        size -= to_copy;
        
        if (buffer_pos_ == buffer_.size()) {
            flush();
        }
    }
}

void BufferedWriter::write(ByteSpan data) {
    write(data.data(), data.size());
}

void BufferedWriter::write(const std::vector<Byte>& data) {
    write(data.data(), data.size());
}

void BufferedWriter::write(std::string_view data) {
    write(reinterpret_cast<const Byte*>(data.data()), data.size());
}

void BufferedWriter::flush() {
    if (buffer_pos_ > 0) {
        ssize_t written = ::write(fd_, buffer_.data(), buffer_pos_);
        if (written < 0 || static_cast<std::size_t>(written) != buffer_pos_) {
            throw std::runtime_error("BufferedWriter: write failed");
        }
        total_written_ += buffer_pos_;
        buffer_pos_ = 0;
    }
}

// StreamReader 实现
StreamReader::StreamReader(const std::filesystem::path& path,
                           std::size_t buffer_size)
    : buffer_(buffer_size) {
    
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("StreamReader: failed to open file: " + path.string());
    }
    
    struct stat st;
    if (::fstat(fd_, &st) == 0) {
        file_size_ = static_cast<std::size_t>(st.st_size);
    }
    
    // 预读取
    fill_buffer();
}

StreamReader::~StreamReader() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void StreamReader::fill_buffer() {
    if (eof_) return;
    
    ssize_t bytes = ::read(fd_, buffer_.data(), buffer_.size());
    if (bytes < 0) {
        throw std::runtime_error("StreamReader: read failed");
    }
    
    buffer_pos_ = 0;
    buffer_end_ = static_cast<std::size_t>(bytes);
    
    if (bytes == 0) {
        eof_ = true;
    }
}

std::size_t StreamReader::read(Byte* buffer, std::size_t size) {
    std::size_t total = 0;
    
    while (size > 0 && !eof_) {
        if (buffer_pos_ >= buffer_end_) {
            fill_buffer();
            if (eof_) break;
        }
        
        std::size_t available = buffer_end_ - buffer_pos_;
        std::size_t to_copy = std::min(size, available);
        
        std::memcpy(buffer, buffer_.data() + buffer_pos_, to_copy);
        buffer_pos_ += to_copy;
        buffer += to_copy;
        size -= to_copy;
        total += to_copy;
    }
    
    total_read_ += total;
    return total;
}

std::vector<Byte> StreamReader::read(std::size_t size) {
    std::vector<Byte> result(size);
    std::size_t actual = read(result.data(), size);
    result.resize(actual);
    return result;
}

// async_io 命名空间实现
namespace async_io {

std::future<std::vector<Byte>> read_async(const std::filesystem::path& path) {
    return std::async(std::launch::async, [path]() {
        return read_binary_file(path.string());
    });
}

std::future<void> write_async(const std::filesystem::path& path,
                              std::vector<Byte> data) {
    return std::async(std::launch::async, [path, data = std::move(data)]() {
        write_binary_file(path.string(), data);
    });
}

std::future<std::vector<Byte>> compress_file_async(
    const std::filesystem::path& path,
    const std::string& algorithm) {
    
    return std::async(std::launch::async, [path, algorithm]() {
        MappedFile file(path);
        auto compressor = create_compressor(algorithm);
        auto compressed = compressor->compress(file.as_string_view());
        
        auto id = algorithm_id_from_name(algorithm);
        return pack_container(id, file.size(), compressed);
    });
}

std::future<std::string> decompress_file_async(const std::filesystem::path& path) {
    return std::async(std::launch::async, [path]() {
        auto data = read_binary_file(path.string());
        auto [id, orig_size, payload] = unpack_container(data);
        auto compressor = create_compressor(id);
        return compressor->decompress(payload);
    });
}

} // namespace async_io

} // namespace compressup
