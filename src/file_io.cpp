#include "file_io.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace compressup {

std::string read_text_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }

    std::string data;
    in.seekg(0, std::ios::end);
    std::streampos size = in.tellg();
    in.seekg(0, std::ios::beg);

    if (size > 0) {
        data.resize(static_cast<std::size_t>(size));
        in.read(data.data(), size);
    }

    if (!in.good() && !in.eof()) {
        throw std::runtime_error("Failed to read file: " + path);
    }

    return data;
}

std::vector<Byte> read_binary_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open file for reading: " + path);
    }

    in.seekg(0, std::ios::end);
    std::streampos size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<Byte> buffer;
    if (size > 0) {
        buffer.resize(static_cast<std::size_t>(size));
        in.read(reinterpret_cast<char*>(buffer.data()), size);
    }

    if (!in.good() && !in.eof()) {
        throw std::runtime_error("Failed to read file: " + path);
    }

    return buffer;
}

void write_text_file(const std::string& path, std::string_view content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out.good()) {
        throw std::runtime_error("Failed to write file: " + path);
    }
}

void write_binary_file(const std::string& path, const std::vector<Byte>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }

    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }

    if (!out.good()) {
        throw std::runtime_error("Failed to write file: " + path);
    }
}

} // namespace compressup
