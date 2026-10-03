# DMGameEngine CI 说明

> 本文件说明 CI 布局与本地等价命令。构建事实的权威来源是 `kb/KB-01-构建系统与预编译头.md`。

## CI 布局（.github/workflows/ci.yml）

| Job | Runner | 做什么 |
|---|---|---|
| `build-test` | `windows-latest` | configure → build → ctest（MSVC / VS2022 生成器 / x64 / Debug / `DMGE_BUILD_TESTS=ON`） |

- **触发**：push 到 `main` / `develop`、所有 pull_request、手动 `workflow_dispatch`。
- **并发取消**：同分支新提交自动取消旧运行（`concurrency.cancel-in-progress`）。
- **fail-fast**：矩阵内任一配置失败立即取消其余 job（当前仅 Debug 单配置，为后续扩展预留）。
- **生成器选择**：用 `"Visual Studio 17 2022"` 而非 Ninja + vcvars —— runner 镜像自带完整 VS2022，
  VS 生成器无需环境引导步骤；若镜像升级导致生成器名不匹配，改 `-G` 参数即可（ci.yml 注释里有
  Ninja + `ilammy/msvc-dev-cmd` 替代方案）。
- **Vulkan 后端 job**：`DMGE_VULKAN_BACKEND=ON` 需 runner 装 Vulkan SDK（VMA + shaderc）。
  SDK 安装方案（社区 action 或 LunarG 安装器）可靠性未验证，暂以注释骨架形式保留在 ci.yml 末尾，
  固定 SDK 版本后再启用。
- **clang-tidy**：仓库根 `.clang-tidy` 为保守检查集（bugprone 精选 + modernize-use-nullptr +
  performance 精选 + readability-identifier-naming 按 KB-06）。**暂不设 CI 强制 job**，
  避免存量告警卡死首轮 CI；噪声清理完后再升级为门禁。

## 本地等价命令

与 AGENTS.md / KB-01 一致 —— 首选 CLion 内置 Ninja 工具链（裸终端缺 VS 环境会报
LNK1104，那是环境问题不是代码问题，见 kb/KB-07 K-004）：

```bash
# 配置（Debug + 测试；等价于 CI 的 configure 步骤）
cmake -S . -B cmake-build-debug -G Ninja -DDMGE_BUILD_TESTS=ON

# 构建（等价于 CI 的 build 步骤）
cmake --build cmake-build-debug

# 跑测试（等价于 CI 的 test 步骤；⚠️ 必须指向 build 目录下的 engine/ 子目录，原因见下）
ctest --test-dir cmake-build-debug/engine --output-on-failure

# 可选：Vulkan 后端（需本机 Vulkan SDK）
# cmake -S . -B cmake-build-debug -G Ninja -DDMGE_BUILD_TESTS=ON -DDMGE_VULKAN_BACKEND=ON
```

## 注意事项

- **ctest 目录陷阱**：`enable_testing()` 目前写在 `engine/CMakeLists.txt`（子目录）里而非根
  CMakeLists，所以根 build 目录不生成 `CTestTestfile.cmake`，对根目录跑 `ctest` 会报
  "No tests were found!!!"。必须 `ctest --test-dir <build>/engine`。正确修法是把
  `enable_testing()` 挪进根 CMakeLists.txt（热点文件，待修 CMake 时处理，见 kb/KB-07 K-009）。
- GoogleTest / entt / nlohmann_json 走 CMake FetchContent，configure 需要网络。
- 测试 target `dmge_tests` 的 POST_BUILD 会自动拷引擎 DLL，直接 `ctest` 即可，别手动搬 DLL。
- 新增源文件必须手动注册进对应 CMakeLists 的 `set(DMGE_SOURCES ...)`（禁 GLOB，R2）。
