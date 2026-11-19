# DD.PluginStarter

《暗黑地牢》DLL 插件的**最小可跑**工程模板，从 `D:\workplace\DD.Plugins` 抽取骨架与纪律，砍掉全部业务逻辑。

目标只有一个：让你在 5 分钟内得到「构建通过 → 加载成功 → 日志有输出 → 补丁只在唯一命中时才写入」的完整回路，然后往上填自己的功能。

## 与 DD.ModLoader 的关系（先读这段）

```
Darkest.exe ──静态导入──> dbghelp.dll（代理，来自 DD.ModLoader）
                              │
                              ├─ ModLoaderThread：Sleep(1500) 后扫描 mods\
                              └─ LoadLibraryW(<game>\mods\<dd.mod>\*.dll) ──> 本仓库的插件
```

- **插件只能由 DD.ModLoader 加载**。本仓库不提供、也不支持任何"绕过加载器直接注入"的方式（不写独立宿主、不写 injector、不写调试器直接 LoadLibrary 的脚本）。没有加载器就没有插件运行环境。
- 部署终点是 `<游戏目录>\mods\<dd.mod>\<插件>.dll`，加载器负责把它叫起来。
- 所以"验证插件"只有两条合法路径：**离线单测**（纯逻辑，不加载 DLL）和**测试目录实机**（由 `DD.ModLoader/tools/rig.py` 管理的 `DarkestDungeon_Dev`）。

## 环境准备

| 项 | 要求 |
| --- | --- |
| 编译器 | Visual Studio 2022 / 2026，「使用 C++ 的桌面开发」工作负载 |
| 构建 | xmake ≥ 2.8.5 |
| 加载器 | `D:\workplace\DD.ModLoader`，且游戏目录里已装好代理 `dbghelp.dll` |
| 环境变量 | `DDPluginAuthor`（作者名，写进 DLL 版本信息）、`DDPath`（游戏目录）、`DKUtilPath`（可选） |

环境变量由 `DD.ModLoader/tools/env.sh` 统一注入（它也补上 Git Bash 里缺失的 xmake 路径）：

```bash
source D:/workplace/DD.ModLoader/tools/env.sh
dd_config                 # = xmake f -p windows -a x64 -m releasedbg -y
```

`env.sh` 只在 `DD_DEPLOY=1` 时才导出 `DDPath`，所以**默认构建绝不碰真实游戏目录**。

## 5 分钟上手

```bash
# 1) 构建插件（不部署）
cd dd-plugin-starter
env -u DDPath -u DD_DEPLOY xmake build Demo

# 2) 跑离线单测（不启动游戏、不加载 DLL）
env -u DDPath -u DD_DEPLOY xmake build demo_test
./build/windows/x64/releasedbg/demo_test.exe        # 期望 == ALL PASS ==

# 3) 部署到测试目录（不是真实安装）
export DD_DEPLOY=1
env DD_DEPLOY=1 xmake build Demo
python -B D:/workplace/DD.ModLoader/tools/rig.py deploy <方案名>
python -B D:/workplace/DD.ModLoader/tools/rig.py run --seconds 45
python -B D:/workplace/DD.ModLoader/tools/rig.py save-log first-run

# 4) 看日志
#    <测试目录>\DDModLoader\log\Demo.log
#    <测试目录>\DDModLoader\cfg\Demo.cfg   ← 首次运行自动生成
```

首次运行、且 `pattern` 留空时，日志里应该只有这几行（**这已经是"插件工作正常"的完整证据**）：

```
[..:..:..] [INFO] ==== Demo v1.0.0 log opened ====
[..:..:..] [INFO] Demo v1.0.0 loading
[..:..:..] [INFO] config template created: ...\DDModLoader\cfg\Demo.cfg
[..:..:..] [INFO] no pattern configured (safe no-op). fill pattern/replacement in ...
```

填了特征码之后，多出来的是：

```
[..:..:..] [INFO] module base=0x7FF7xxxxxxxx size=0x1280000
[..:..:..] [INFO] unique hit at RVA=0x1EADE0, verifying before write
[..:..:..] [INFO] patched 4 bytes at RVA=0x1EADE0
[..:..:..] [INFO] [DIAG] scans=1 hits=1 applied=1
```

## 目录结构

```
dd-plugin-starter/
├─ xmake.lua               工程入口：自动 includes 每个插件目录
├─ xmake/plugin.lua        dd.plugin 规则：C++23 /MT、版本资源、Plugin.h 生成、部署门控
├─ xmake/Plugin.h.in       构建期生成 Plugin.h（NAME/AUTHOR/VERSION）
├─ xmake/version.rc        DLL 版本信息
├─ common/Log.h|.cpp       仓库级日志（零依赖，写 <game>\DDModLoader\log\）
├─ PluginTemplate/         新插件模板（scripts/new-plugin.* 复制它）
├─ Demo/                   示例插件：读 cfg → 特征码定位 → 校验 → 字节补丁
│  ├─ src/PCH.h            预编译头
│  ├─ src/main.cpp         唯一入口 DllMain
│  ├─ src/Hooks.cpp        安装流程编排（失败降级都在这里）
│  ├─ src/Scan.h|.cpp      特征码解析 / 唯一命中扫描（含跨页 carry）
│  ├─ src/Patch.h|.cpp     补丁前校验 + 写入（VirtualProtect + FlushInstructionCache）
│  ├─ src/Config.h|.cpp    cfg 解析：缺键补默认、非法值告警、首次生成不覆盖
│  └─ tools/test_demo.cpp  离线单测（43 项断言，无需游戏）
├─ scripts/new-plugin.ps1  新建插件（PowerShell）
├─ scripts/new-plugin.sh   新建插件（Git Bash）
├─ docs/ARCHITECTURE.md    分层、加载时序、Demo 数据流
├─ docs/DEV-TEST-DEPLOY.md 开发循环、三层测试、部署、回滚、排错
├─ CONVENTIONS.md          八条铁律 + 事实分级写法
└─ .github/workflows/      CI：构建并上传 dll/pdb（不部署、不跑测试）
```

## 三种最常见的改动

**1. 换成你自己的特征码**

改 `Demo/src/Config.cpp` 里的 cfg 模板注释即可，或直接编辑运行后生成的 `DDModLoader\cfg\Demo.cfg`：

```ini
pattern     = 48 8B ?? ?? E8        # ?? 是通配
replacement = 90 90 90 90 90
```

长度必须相等，否则日志报 `length mismatch` 且**一个字节都不写**。

**2. 新建一个插件**

```powershell
.\scripts\new-plugin.ps1 -Name MyPlugin -Mod MyMod
xmake build MyPlugin
```

脚本只复制 `PluginTemplate/src` 并改写 `target(...)` / `set_version(...)`；`Hooks/Config/Scan/Patch` 这些要自己按 Demo 的结构补。

**3. 加一个 cfg 开关**

在 `Config` 结构体加字段 → `LoadConfig` 里加一个 `else if (key == "...")` → 在 `kTemplate` 里写上默认值和注释。三处都要写，不要只在代码里给默认值。

## 设计取舍（与 DD.Plugins 的差异）

| 项 | DD.Plugins | 本模板 | 原因 |
| --- | --- | --- | --- |
| 外部依赖 | xmake 包（nlohmann_json/spdlog/xbyak/simpleini）+ DKUtil 必需 | 零依赖，DKUtil 可选 | 保证 clone 下来就能编译，不被网络和 extern 卡住 |
| 日志 | DKUtil Logger | `common/Log.cpp` | 同上；接口保持 `LOG_INFO/WARN/ERROR` 可平替 |
| 部署门控 | `DDPath` + `dd.mod` | 再加 `DD_DEPLOY=1` | `DDPath` 常是用户级环境变量，全量构建会静默写进真实安装 |
| cfg 模板 | `mod/<插件>.cfg` 随构建复制 | 运行时首次生成（内嵌在 Config.cpp） | 单 DLL 插件不必额外资源文件，且杜绝"模板与代码两份不一致" |
| 单测 | 映射真实 exe 的 PE 映像 | 纯内存 fixture | 不绑定具体 exe 版本，starter 里没有可映射的目标 |

保留不动的：**全有或全无校验**、**失败只降级不 return**、**无条件计数器**、**日志节流**、**cfg 三处显式写值**、**ASLR 用 base+RVA**。这些见 `CONVENTIONS.md`。

## 验证状态（不要把它当"已验证可用"）

编写本模板时在本机实际做过：

- `xmake f -p windows -a x64 -m releasedbg -y` + `xmake build Demo` → 生成 `build\windows\x64\releasedbg\Demo.dll`；
- `xmake build demo_test` + 运行 → **43 项断言全 PASS**；
- 部署门控反证：`DDPath` 存在但未设 `DD_DEPLOY=1` 时，构建后游戏目录无任何变化。

**没有做**：DLL 实际加载、游戏内实机验收。这两步只能在装有 DD.ModLoader 的测试目录里完成，步骤见 `docs/DEV-TEST-DEPLOY.md` §4。
