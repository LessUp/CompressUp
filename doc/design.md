# CompressUp 设计文档

## 1. 目标与约束

- **平台与语言**：Linux / Ubuntu，使用 **C++20** 与 CMake 构建。
- **功能目标**：
  - 针对文本文件提供压缩与解压功能。
  - 支持多种压缩算法，方便横向对比压缩率与性能。
- **非功能目标**：
  - 架构简单、易读、易扩展（KISS 原则）。
  - 多算法可插拔，新增算法改动最小。
  - 提供自动化测试与基准测试工具，生成可阅读的“测试报告”。


## 2. 总体架构

### 2.1 目录结构

项目核心目录结构如下：

- `CMakeLists.txt`：CMake 构建脚本。
- `src/`
  - `types.h`：通用类型别名（例如 `using Byte = std::uint8_t;`）。
  - `compressor.h`：压缩算法抽象接口 `ICompressor`。
  - `rle_compressor.{h,cpp}`：RLE（Run-Length Encoding）算法实现。
  - `lz77_compressor.{h,cpp}`：简化 LZ77 算法实现。
  - `registry.{h,cpp}`：算法注册与工厂，支持按名称或 ID 创建压缩器。
  - `file_io.{h,cpp}`：文件读写工具（文本/二进制）。
  - `container.{h,cpp}`：压缩文件容器格式（魔数 + 算法 ID + 原始长度 + 压缩数据）。
  - `api.{h,cpp}`：高层 API，封装文件压缩/解压逻辑。
  - `main.cpp`：命令行工具入口 `compressup_cli`。
- `tests/`
  - `test_main.cpp`：最小自定义测试程序，验证各算法正确性。
- `bench/`
  - `bench_main.cpp`：基准测试程序，测量压缩率与性能。
- `doc/`
  - `design.md`：本设计文档。
- `changelog/`
  - 若干 `*.md`：记录每次重要改动。


## 3. 压缩算法接口与实现

### 3.1 抽象接口 `ICompressor`

统一的压缩算法接口定义在 `src/compressor.h`：

- `std::string name() const`：返回算法名称（如 `"rle"`、`"lz77"`）。
- `std::vector<Byte> compress(std::string_view input)`：将输入文本压缩为字节序列。
- `std::string decompress(const std::vector<Byte>& input)`：将压缩字节序列解压回文本。

所有具体算法（RLE / LZ77 / 未来算法）都实现该接口，使上层代码无需关心内部细节。


### 3.2 RLE 算法实现

文件：`src/rle_compressor.{h,cpp}`。

- **编码思想**：统计连续相同字符的“游程”，用 (次数, 字符) 对来表示。
- **压缩格式**：
  - 每个段使用固定 2 字节：`[count:1 byte][char:1 byte]`。
  - `count` 范围为 1..255，超出则拆成多个段。
- **解压逻辑**：
  - 输入长度必须为偶数，否则抛异常；
  - 每两个字节读取一次 `(count, char)`，将 `char` 追加 `count` 次到输出字符串。

RLE 实现简单、速度快，但在一般自然语言文本上的压缩率通常一般，主要作为“基线算法”和“高重复场景”示例。


### 3.3 简化 LZ77 算法实现

文件：`src/lz77_compressor.{h,cpp}`。

- **窗口与匹配参数**：
  - `kWindowSize = 1024` 字节：回溯窗口大小。
  - `kMaxMatchLength = 32` 字节：一次匹配的最大长度。
- **Token 设计**（保持简洁、便于实现）：
  - **字面量 token**：
    - 第 1 字节：`0x80`（最高位为 1，表示 literal）。
    - 第 2 字节：实际字符字节。
  - **匹配 token**：
    - 第 1 字节：`length`，范围 [3, `kMaxMatchLength`]，最高位为 0（区别于字面量）。
    - 第 2 字节：`offset_high`（高 8 位）。
    - 第 3 字节：`offset_low`（低 8 位）。
- **压缩流程**：
  1. 对输入当前位置 `pos`，在回溯窗口内搜索最长匹配串；
  2. 若找到长度 ≥ 3 的匹配，则输出“匹配 token”，移动 `pos += length`；
  3. 否则输出“字面量 token”，仅消耗当前 1 字符。
- **解压流程**：
  - 顺序读取 token：
    - 若第 1 字节最高位为 1 → 读下一个字节作为字面量字符；
    - 否则 → 这是匹配 token，根据 `(length, offset)` 从已生成的输出中复制对应子串。
  - 对非法 `length`、`offset` 做边界检查并抛异常。

该简化 LZ77 实现保持结构清晰，易于理解和调试，同时能在许多文本场景取得比 RLE 更好的压缩率。


## 4. 容器格式设计

文件：`src/container.{h,cpp}`。

为了在压缩文件中存放“算法元信息 + 原始长度”，设计了一个简单的容器格式，便于解压阶段自动选择算法与校验数据。

### 4.1 头部格式

压缩文件的整体结构为：

- **1 字节**：魔数 `magic`，固定为 `0xC3`（"CompressUp" 简写）。
- **1 字节**：算法 ID（`AlgorithmId` 枚举，当前支持 Rle=1, Lz77=2）。
- **8 字节**：原始数据长度 `original_size`，`std::uint64_t`，小端序存储。
- **剩余字节**：具体算法生成的压缩字节流。

### 4.2 打包与解包

- `pack_container(AlgorithmId algorithm, std::uint64_t original_size, const std::vector<Byte>& compressed)`：
  - 构造容器格式：魔数 + 算法 ID + 原始长度 + `compressed` 数据。
- `unpack_container(const std::vector<Byte>& data)`：
  - 校验魔数、解析算法 ID 与原始长度；
  - 将剩余字节作为 `payload` 返回。

通过容器头中的算法 ID，解压时可以自动选用正确的算法，无需 CLI 再额外指定。


## 5. 文件 IO、API 与命令行工具

### 5.1 文件 IO

文件：`src/file_io.{h,cpp}`，提供简单封装：

- `read_text_file(path)`：以二进制方式读入整个文件到 `std::string`。
- `read_binary_file(path)`：读入整个文件到 `std::vector<Byte>`。
- `write_text_file(path, content)`：写出文本内容（UTF-8 视为原始字节）。
- `write_binary_file(path, data)`：写出压缩后字节流/容器数据。

### 5.2 算法工厂与注册

文件：`src/registry.{h,cpp}`。

- 枚举 `AlgorithmId`：
  - `Rle = 1`
  - `Lz77 = 2`
- 工厂函数：
  - `create_compressor(const std::string& name)`：按算法名实例化压缩器。
  - `create_compressor(AlgorithmId id)`：按枚举 ID 实例化压缩器。
  - `algorithm_id_from_name(name)` / `algorithm_name_from_id(id)`：名字与 ID 互转。
  - `available_algorithms()`：返回当前所有支持算法名列表（如 `{"rle", "lz77"}`）。

新增算法时，只需：

1. 在 `AlgorithmId` 枚举中添加新项；
2. 新增对应 `ICompressor` 实现类；
3. 在 `registry.cpp` 中增加创建逻辑和名称映射；
4. 测试与 benchmark 会自动纳入该算法。

### 5.3 高层 API

文件：`src/api.{h,cpp}`，封装“文件级”压缩/解压流程。

- `compress_file(input_path, output_path, algorithm_name)`：
  1. 使用 `read_text_file` 读入文本；
  2. 根据 `algorithm_name` 创建压缩器；
  3. 调用 `compress` 得到压缩字节流；
  4. 使用 `pack_container` 打包容器；
  5. 通过 `write_binary_file` 写出到目标文件。

- `decompress_file(input_path, output_path)`：
  1. 使用 `read_binary_file` 读入整个压缩文件；
  2. 调用 `unpack_container` 解析出算法 ID、原始长度与压缩数据；
  3. 使用算法 ID 创建压缩器；
  4. 调用 `decompress` 得到原始文本；
  5. 校验解压后长度与原始长度一致；
  6. 通过 `write_text_file` 写出到目标路径。

### 5.4 命令行工具

入口：`src/main.cpp`，生成可执行程序 `compressup_cli`。

支持的命令：

- **压缩**：
  - `compressup_cli compress --algo <name> <input> <output>`
  - 示例：
    - `compressup_cli compress --algo rle  input.txt  out_rle.cu`
    - `compressup_cli compress --algo lz77 input.txt  out_lz77.cu`
- **解压**（算法自动识别）：
  - `compressup_cli decompress <input> <output>`
  - 示例：
    - `compressup_cli decompress out_rle.cu  recovered_rle.txt`
- **列出支持算法**：
  - `compressup_cli list-algorithms`

所有错误（如文件读写失败、算法名错误、容器格式错误等）会以 `std::exception` 抛出并在 `main` 中统一捕获，打印错误信息并返回非 0 退出码。


## 6. 测试设计

文件：`tests/test_main.cpp`。

测试目标：

- **正确性**：确保对各种输入，`decompress(compress(x)) == x` 对所有算法成立。
- **场景覆盖**：
  - 空字符串：`""`
  - 极短字符串：`"a"`、`"abc"`
  - 高重复模式：`"aaaaaa"`、大量 `"x"` 组成的字符串
  - 交替模式：`"abababababab"`
  - 英文句子：`"The quick brown fox jumps over the lazy dog"`
  - 混合字母数字：`"0123456789abcdefghijklmnopqrstuvwxyz"`

测试逻辑：

- 调用 `available_algorithms()` 获取算法列表；
- 对每个算法、每个用例：
  1. 使用工厂创建压缩器；
  2. 执行 `compress` 得到压缩字节流；
  3. 执行 `decompress` 得到输出字符串；
  4. 比较输出与输入是否一致，不一致则打印详细信息并计数失败用例。

最终输出：

- 若所有用例通过：打印 `All tests passed` 并返回 0。
- 否则打印失败数量并返回非 0。


## 7. 基准测试与压缩率对比

文件：`bench/bench_main.cpp`，生成可执行程序 `compressup_bench`。

### 7.1 度量指标

对于每个 **算法 × 输入文件**，统计：

- `original_size`：原始字节数。
- `compressed_size`：压缩后字节数。
- `ratio`：压缩率，`compressed_size / original_size`（越小越好）。
- `compress_ms`：多次压缩的平均耗时（毫秒）。
- `decompress_ms`：多次解压的平均耗时（毫秒）。
- `compress_mb_s`：压缩吞吐量（MB/s）。
- `decompress_mb_s`：解压吞吐量（MB/s）。

时间测量使用 `std::chrono::steady_clock`，重复运行若干次（默认 5 次），取平均值。

### 7.2 使用方式与输出格式

在构建目录（例如 `build/`）中运行：

```bash
./compressup_bench file1.txt file2.txt ...
```

程序会：

1. 通过 `available_algorithms()` 获取所有算法名；
2. 对每个输入文件、每个算法运行压缩/解压 benchmark；
3. 以表格形式打印结果，方便直接复制到报告中。

输出示例（表格）：

```text
Algo      File                Orig(KB)   Comp(KB)     Ratio    Cmp(ms)    Dec(ms)   CmpMB/s   DecMB/s
rle       data1.txt            1000.00    700.00     0.700      5.23      3.01     190.00    330.00
lz77      data1.txt            1000.00    400.00     0.400      8.10      4.50     123.00    222.00
```

你可以：

- 直接将该表格复制到 markdown 报告；
- 或重定向输出到文件：`./compressup_bench data1.txt > bench_result.txt`，再做后处理或转成 CSV。


## 8. 新算法扩展方式

为了兼容后续更多压缩算法（自实现或外部库封装），扩展步骤设计为：

1. 在 `src/` 中新增一个类实现 `ICompressor` 接口（例如 `huffman_compressor.{h,cpp}` 或 `zstd_compressor.{h,cpp}`）。
2. 在 `AlgorithmId` 枚举中添加新值（例如 `Huffman = 3` 或 `Zstd = 4`）。
3. 在 `registry.cpp` 中：
   - 在 `create_compressor(name)` 中增加名称分支；
   - 在 `create_compressor(AlgorithmId)` 中增加枚举分支；
   - 在 `algorithm_id_from_name` / `algorithm_name_from_id` 中增加映射；
   - 在 `available_algorithms()` 中追加新算法名字符串。
4. 重新编译后：
   - CLI 会自动支持新的 `--algo <name>`；
   - 测试程序会自动对新算法做往返正确性验证；
   - benchmark 会自动纳入新算法的性能与压缩率结果。

通过这种“集中注册 + 统一接口”的方式，保证多算法兼容和后续扩展都保持 KISS 原则。


## 9. 构建与使用示例

### 9.1 构建

在项目根目录下：

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

生成的主要可执行文件：

- `compressup_cli`：命令行压缩工具。
- `compressup_tests`：正确性测试。
- `compressup_bench`：性能与压缩率 benchmark。

### 9.2 运行测试

```bash
cd build
./compressup_tests
# 或使用 ctest 统一运行
ctest
```

### 9.3 命令行示例

- 压缩文本：

  ```bash
  ./compressup_cli compress --algo rle  input.txt  out_rle.cu
  ./compressup_cli compress --algo lz77 input.txt  out_lz77.cu
  ```

- 解压：

  ```bash
  ./compressup_cli decompress out_rle.cu  recovered_rle.txt
  ./compressup_cli decompress out_lz77.cu recovered_lz77.txt
  ```

- 查看支持算法：

  ```bash
  ./compressup_cli list-algorithms
  ```

### 9.4 Benchmark 示例

```bash
cd build
./compressup_bench data1.txt data2.txt > bench_result.txt
```

将输出表格直接贴入 Markdown 报告，即可方便地展示不同压缩算法在不同数据集上的压缩率与性能表现。
