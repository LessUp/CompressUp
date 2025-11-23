#include "file_io.h"
#include "registry.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace compressup;

namespace {

struct BenchResult {
    std::string algorithm;
    std::string file;
    std::size_t original_size{};
    std::size_t compressed_size{};
    double ratio{};
    double compress_ms{};
    double decompress_ms{};
    double compress_mb_s{};
    double decompress_mb_s{};
};

BenchResult run_bench(const std::string& algorithm,
                      const std::string& file_path,
                      int repeats) {
    BenchResult result;
    result.algorithm = algorithm;
    result.file = file_path;

    std::string text = read_text_file(file_path);
    result.original_size = text.size();

    if (result.original_size == 0) {
        result.compressed_size = 0;
        result.ratio = 0.0;
        result.compress_ms = 0.0;
        result.decompress_ms = 0.0;
        result.compress_mb_s = 0.0;
        result.decompress_mb_s = 0.0;
        return result;
    }

    std::vector<Byte> compressed_once;
    {
        auto compressor = create_compressor(algorithm);
        compressed_once = compressor->compress(text);
    }

    result.compressed_size = compressed_once.size();
    result.ratio = static_cast<double>(result.compressed_size) /
                   static_cast<double>(result.original_size);

    using clock = std::chrono::steady_clock;

    double total_compress_ms = 0.0;
    for (int i = 0; i < repeats; ++i) {
        auto compressor = create_compressor(algorithm);

        auto start = clock::now();
        auto tmp = compressor->compress(text);
        auto end = clock::now();

        std::chrono::duration<double, std::milli> diff = end - start;
        total_compress_ms += diff.count();

        if (i == 0 && tmp.size() != compressed_once.size()) {
            // 使用第一次压缩结果作为基准
            compressed_once = std::move(tmp);
            result.compressed_size = compressed_once.size();
            result.ratio = static_cast<double>(result.compressed_size) /
                           static_cast<double>(result.original_size);
        }
    }

    result.compress_ms = total_compress_ms / static_cast<double>(repeats);

    double total_decompress_ms = 0.0;
    for (int i = 0; i < repeats; ++i) {
        auto compressor = create_compressor(algorithm);

        auto start = clock::now();
        std::string decompressed = compressor->decompress(compressed_once);
        auto end = clock::now();

        std::chrono::duration<double, std::milli> diff = end - start;
        total_decompress_ms += diff.count();

        if (i == 0 && decompressed.size() != result.original_size) {
            std::cerr << "Warning: decompressed size mismatch for algorithm="
                      << algorithm << " file=" << file_path << "\n";
        }
    }

    result.decompress_ms = total_decompress_ms / static_cast<double>(repeats);

    double mb = static_cast<double>(result.original_size) / (1024.0 * 1024.0);
    if (result.compress_ms > 0.0) {
        result.compress_mb_s = mb / (result.compress_ms / 1000.0);
    }
    if (result.decompress_ms > 0.0) {
        result.decompress_mb_s = mb / (result.decompress_ms / 1000.0);
    }

    return result;
}

void print_header() {
    std::cout << std::left
              << std::setw(10) << "Algo"
              << std::setw(20) << "File"
              << std::right
              << std::setw(12) << "Orig(KB)"
              << std::setw(12) << "Comp(KB)"
              << std::setw(10) << "Ratio"
              << std::setw(12) << "Cmp(ms)"
              << std::setw(12) << "Dec(ms)"
              << std::setw(12) << "CmpMB/s"
              << std::setw(12) << "DecMB/s"
              << '\n';
}

void print_result(const BenchResult& r) {
    double orig_kb = static_cast<double>(r.original_size) / 1024.0;
    double comp_kb = static_cast<double>(r.compressed_size) / 1024.0;

    std::cout << std::left
              << std::setw(10) << r.algorithm
              << std::setw(20) << r.file
              << std::right
              << std::setw(12) << std::fixed << std::setprecision(2) << orig_kb
              << std::setw(12) << std::fixed << std::setprecision(2) << comp_kb
              << std::setw(10) << std::fixed << std::setprecision(3) << r.ratio
              << std::setw(12) << std::fixed << std::setprecision(2) << r.compress_ms
              << std::setw(12) << std::fixed << std::setprecision(2) << r.decompress_ms
              << std::setw(12) << std::fixed << std::setprecision(2) << r.compress_mb_s
              << std::setw(12) << std::fixed << std::setprecision(2) << r.decompress_mb_s
              << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: compressup_bench <file1> [file2 ...]\n";
        return 1;
    }

    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        files.emplace_back(argv[i]);
    }

    std::vector<std::string> algorithms = available_algorithms();

    const int repeats = 5;

    print_header();

    for (const auto& file : files) {
        for (const auto& algo : algorithms) {
            try {
                BenchResult r = run_bench(algo, file, repeats);
                print_result(r);
            } catch (const std::exception& ex) {
                std::cerr << "Error while benchmarking algo=" << algo
                          << " file=" << file << ": " << ex.what() << "\n";
            }
        }
    }

    return 0;
}
