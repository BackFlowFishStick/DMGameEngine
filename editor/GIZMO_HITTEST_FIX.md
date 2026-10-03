# DMGameEditor - Gizmo 命中检测失效修复记录

> 日期：2026-08-06
> 模块：editor / EditorLayer viewport gizmo
> 状态：已修复并验证构建通过
> 目的：记录"能点选但 gizmo 无法拖拽"的根因与修复，让 ImGuizmo 跨窗口命中机制可追溯

---

## 1. 症状

选中物体后切到 Translate / Rotate / Scale gizmo 模式：

- ✅ viewport 中点击可选中物体（picking 正常）
- ❌ 鼠标放在 gizmo 上，`ImGuizmo::IsOver()` 恒为 `false`
- ❌ gizmo 无法拖拽，`ImGuizmo::IsUsing()` 恒为 `false`

即：坐标是对齐的（picking 能命中），但 ImGuizmo 完全"看不到"鼠标。

---

## 2. 根因

### 2.1 直接原因：gizmo 画到了没有所属窗口的 draw list

`EditorLayer::DrawViewport()` 中原写法：

```cpp
ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
```

`ImGui::GetForegroundDrawList()` 返回 viewport 级别的覆盖层 draw list（`imgui.h:1074`），
它**不归属任何 `Begin()` 创建的命名窗口**，其 `_OwnerName`（`imgui.h:3481`）不指向
一个普通 ImGui 窗口。

而 ImGuizmo 的命中检测恰好依赖 draw list 的所属窗口。

### 2.2 ImGuizmo 命中检测链条（源码证据）

ImGuizmo vendored 版本 `editor/dependencies/ImGuizmo/src/ImGuizmo.cpp`：

**(a) 每次 `Manipulate` 开头，`ComputeContext()` 设定 `mbMouseOver`：**

```cpp
// ImGuizmo.cpp:1190
gContext.mbMouseOver = IsHoveringWindow();
```

**(b) `IsHoveringWindow()` 用 `mDrawList->_OwnerName` 反查所属窗口：**

```cpp
// ImGuizmo.cpp:1015-1034
static bool IsHoveringWindow() {
    ImGuiWindow* window = ImGui::FindWindowByName(gContext.mDrawList->_OwnerName);
    if (g.HoveredWindow == window)   return true;   // ① 鼠标 hover 所属窗口
    if (gContext.mAlternativeWindow && g.HoveredWindow == gContext.mAlternativeWindow)
        return true;                                  // ② 备选窗口
    if (g.HoveredWindow != NULL) return false;       // ③ ← 命中：有别的窗口被 hover
    ...
}
```

用前景层 draw list 时：`FindWindowByName(...)` 找不到对应窗口 -> `window = NULL`；
而鼠标悬停在 "Viewport" 面板 -> `g.HoveredWindow` 非 NULL -> 走到 ③ `return false`。

-> `mbMouseOver = false`

**(c) 命中检测入口全部被 `mbMouseOver` 短路：**

```cpp
// ImGuizmo.cpp:2266（TRANSLATE）/ 2455（SCALE）/ 2579（ROTATE）
if(!Intersects(op, TRANSLATE) || gContext.mbUsing || !gContext.mbMouseOver)
    return MT_NONE;        // ← mbMouseOver=false，直接返回 NONE
```

**(d) 于是 `mbOverGizmoHotspot` 永远不被置位：**

```cpp
// ImGuizmo.cpp:2415
type = gContext.mbOverGizmoHotspot ? MT_NONE : GetMoveType(op, &gizmoHitProportion);
gContext.mbOverGizmoHotspot |= type != MT_NONE;   // type 恒 NONE -> 永远 false
```

**(e) 最终 `IsOver()` / 拖拽全部失效：**

```cpp
// ImGuizmo.cpp:1113
bool IsOver() { return gContext.mbOverGizmoHotspotLastFrame || IsUsingAny(); }  // 两者皆 false
```

`Manipulate` 内部检测不到 hover / using，不进入拖拽分支 -> `IsUsing()` 恒 false。

### 2.3 为什么点选能工作、gizmo 不行

两者走完全不同的鼠标路径：

| 功能 | 鼠标来源 | 是否依赖 `mbMouseOver` | 结果 |
|---|---|---|---|
| 点选 picking | 编辑器自写：`ImGui::GetMousePos()` + `GetItemRectMin/Max` + 射线-AABB | ❌ 不经过 ImGuizmo | ✅ 正常 |
| gizmo 命中 | ImGuizmo 内部 `GetMoveType` 等 | ✅ 被 `mbMouseOver` 短路 | ❌ 失效 |

坐标本身对齐（picking 能命中证明），但 ImGuizmo 的窗口归属检测被前景层 draw list 切断。

---

## 3. 原作者的误区

原注释：

> Draw gizmo to the foreground draw list so it renders above the viewport image
> (BeginFrame's "gizmo" window sits below Viewport).

`ImGuizmo::BeginFrame()`（`ImGuizmo.cpp:1066` 附近）内部创建了一个全屏 NoInputs 的
`"gizmo"` 窗口。若用默认 draw list（`GetWindowDrawList`，此时当前窗口是 "gizmo"），
gizmo 画在该窗口 draw list，先于 "Viewport" 窗口提交，被 `ImGui::Image` 遮挡。

作者为让 gizmo "画在 image 之上"切到前景层——但这切断了窗口归属，牺牲了命中检测。

**遮盖问题有更简单的解法**：只要 `Manipulate` 在 `ImGui::Image` 之后调用，
同一窗口 draw list 内后提交的命令就在上层，gizmo 自然盖住 image，无需切前景层。

---

## 4. 修复

`editor/src/EditorLayer.cpp` `DrawViewport()` gizmo 段：

```diff
- // Draw gizmo to the foreground draw list so it renders above the
- // viewport image (BeginFrame's "gizmo" window sits below Viewport).
- ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
+ // Draw into the Viewport window's own draw list. ImGuizmo's
+ // IsHoveringWindow() resolves the owning window from
+ // gContext.mDrawList->_OwnerName to set mbMouseOver (hit-testing);
+ // ForegroundDrawList has no owning window, so mbMouseOver was always
+ // false and IsOver()/IsUsing() silently failed. Drawing here (after
+ // ImGui::Image) also keeps the gizmo above the viewport image.
+ ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
```

该调用位于 `ImGui::Begin("Viewport")` 之后，`GetWindowDrawList()` 返回 "Viewport" 窗口
的 draw list，其 `_OwnerName == "Viewport"`：

- `IsHoveringWindow()` 找到 "Viewport" 窗口，鼠标悬停其上时 `g.HoveredWindow == window`
  -> ① 命中 `return true` -> `mbMouseOver = true` -> 命中检测恢复
- `Manipulate` 在 `ImGui::Image` 之后提交，同一 draw list 后提交在上层 -> gizmo 仍画在 image 之上

两个问题（显示 + 命中）同时解决。

---

## 5. 验证

- 增量构建（Ninja）：`[2/2] Linking CXX executable editor\DMGameEditor.exe;
  Copying DMGameEngine.dll to editor runtime`，`EXIT=0`
- `DMGameEditor.exe` 时间戳更新至 `2026/8/6 14:54`

修复后选中物体切到 Translate/Rotate/Scale，鼠标移到 gizmo 轴上应能高亮并拖拽。

---

## 6. 技术备忘（务必保留）

### 6.1 ImGuizmo 命中检测必须能解析 draw list 的所属窗口

`SetDrawlist(dl)` 设置 `gContext.mDrawList`，`IsHoveringWindow()` 用
`dl->_OwnerName` 反查窗口判定 `mbMouseOver`：

- **可用**：`ImGui::GetWindowDrawList()`（当前 `Begin(...)` 窗口的 draw list，有 owner）
- **不可用**：`ImGui::GetForegroundDrawList()`（viewport 覆盖层，无命名 owner）

若必须用非窗口 draw list，可调 `ImGuizmo::SetAlternativeWindow(ImGuiWindow*)`
（`ImGuizmo.h:234`）显式指定判定窗口，作为 `IsHoveringWindow()` ② 分支的命中源。

### 6.2 gizmo 显示在 image 之上靠"提交顺序"，不靠"前景层"

同一 ImGui 窗口的 draw list 内，`AddXXX` 按调用顺序追加，后调用的在上层。
`Manipulate` 在 `ImGui::Image` 之后调用即可保证 gizmo 盖住 image，无需前景层。

### 6.3 相关状态字段（ImGuizmo context）

| 字段 | 含义 | 失效时的表现 |
|---|---|---|
| `mbMouseOver` | 鼠标是否在 gizmo 关联窗口上 | 命中检测全短路 |
| `mbOverGizmoHotspot` | 本帧是否 hover 到某 gizmo 热点 | `IsOver()` 返回上一帧值 |
| `mbOverGizmoHotspotLastFrame` | 上一帧快照，供 `IsOver()` 跨帧查询 | — |
| `mAlternativeWindow` | `SetAlternativeWindow` 设的备选判定窗口 | 默认 nullptr |

### 6.4 调试技巧

排查 ImGuizmo 命中问题时，优先确认 `gContext.mbMouseOver` 是否为 `true`（可临时在
`ComputeContext` 后打印）。若为 `false`，问题必在 `IsHoveringWindow()` /
draw list owner，而非 gizmo 几何或矩阵。

### 6.5 构建命令备忘

同步 `cmd /c "vcvars && cmake"` 在 PowerShell 下会卡住等待，疑似 shell 同步机制问题。
改用后台 `Start-Process` 启动 .bat（vcvars + cmake --build，重定向日志）再轮询日志，
构建本身很快（增量仅 `[2/2]` 编译 + 链接）。