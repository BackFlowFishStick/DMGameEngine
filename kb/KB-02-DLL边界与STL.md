# KB-02 DLL 边界与 STL / ImGui 跨 DLL

> 深度阅读：`documents/DLL_STL_BOUNDARY.md`（完整事故案例）。本条是速查与规则。

## 基本事实

- 引擎编为 **DLL**（`DMGE_BUILD_SHARED`），编辑器/游戏 exe 通过 `DMGE_API`（dllexport/dllexport-import 切换宏）消费引擎。
- 两侧必须同 CRT（`/MDd` Debug 一致）才能跨边界安全传 STL——项目已统一，**不要单独改任何 target 的 RuntimeLibrary**。

## 规则 R1：导出类的成员不暴露 STL 容器

- `class DMGE_API Foo { std::vector<int> m_Vec; };` 这类代码在 MSVC 下触发 C4251，且消费端模板实例化不一致时会真实崩溃（实例化渲染事故：跨 DLL 传含 vector 成员对象 → 运行时崩溃）。
- **替代方案**（按优先级）：
  1. 容器移进 .cpp（文件静态 / 匿名命名空间 / 函数内 static），头文件只暴露标量/句柄。
  2. Pimpl：`struct Foo::Impl;` 持容器，.cpp 内实现。
  3. 确实要跨边界传集合：走显式 API（`GetCount()/GetAt(i)` 或 span 视图），不传容器对象本身。
- 模板函数/类跨 DLL 使用需**显式实例化并导出**（`extern template` + `DMGE_API template class ...`），否则消费端 LNK2019。先例：`Mesh` 已导出，但 `AssetLoader<Material>`/`AssetLoader<VertexArray>` 特化尚未导出——消费者直接 `Load<Material>` 会链接失败，需走 DLL 内门面或补导出。

## 规则 R7：ImGui 单 context

- 引擎侧把 5 个 imgui 源编进引擎 DLL，加 `IMGUI_API=__declspec(dllexport)` 定义；编辑器侧 dllimport 同一份。
- **后果**：编辑器与引擎（ProfilerLayer/Console 等）共享同一 GImGui context、同一 atlas。
- **禁止**：编辑器再链一份独立 ImGui（双 context = 输入错乱/字体白屏类事故）。
- `WINDOWS_EXPORT_ALL_SYMBOLS` 已被禁用（与 shaderc PCH 冲突），不要重新打开。

## 速查

| 症状 | 根因方向 |
|---|---|
| C4251 警告 | 导出类 STL 成员，按上面替代方案处理 |
| 跨 DLL 对象传递偶发崩溃 | STL 成员布局不一致 / CRT 不一致 |
| 消费端 LNK2019（模板/特化） | 缺显式实例化导出 |
| ImGui 输入/渲染异常 | 疑似双 context，查链接了几个 imgui |
