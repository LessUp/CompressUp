#pragma once

#include <string>

namespace compressup {

void compress_file(const std::string& input_path,
                   const std::string& output_path,
                   const std::string& algorithm_name);

void decompress_file(const std::string& input_path,
                     const std::string& output_path);

} // namespace compressup
