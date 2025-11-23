#pragma once

#include "types.h"

#include <string>
#include <string_view>
#include <vector>

namespace compressup {

std::string read_text_file(const std::string& path);
std::vector<Byte> read_binary_file(const std::string& path);
void write_text_file(const std::string& path, std::string_view content);
void write_binary_file(const std::string& path, const std::vector<Byte>& data);

} // namespace compressup
