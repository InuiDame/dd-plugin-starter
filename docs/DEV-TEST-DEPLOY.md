# 开发、测试与部署

> 配套 [ARCHITECTURE.md](ARCHITECTURE.md) 的操作手册。所有命令都在本仓库根目录执行。

## 1. 环境

```bash
source D:/workplace/DD.ModLoader/tools/env.sh   # 补 xmake PATH、DKUtilPath、DDPluginAuthor
dd_config                                       # xmake f -p windows -a x64 -m releasedbg -y
```

`env.sh` 的关键行为：**只有 `DD_DEPLOY=1` 时才导出 `DDPath`**，否则 `unset DDPath`。所以默认构建不会部署。

Git Bash 里 xmake 默认会选 mingw 并报 `cannot get program for cxx`，必须显式 `-p windows -a x64`（`dd_config` 已经做了）。

父目录里另有 xmake 工程时，`.xmake` 缓存可能把 projectdir 解析到父目录，表现为 `xxx is not a valid target name for this project`。此时把 action 放前面并固定项目目录：

```bash
xmake f -P . -p windows -a x64 -m releasedbg -y
xmake build -P . Demo
```

## 2. 开发循环

```mermaid
flowchart LR
    A["1 定特征码"] --> B["2 验证唯一命中"]
    B --> C["3 写校验项"]
    C --> D["4 全有或全无"]
    D --> E["5 失败降级"]
    E --> F["6 计数器 + DIAG"]
    F --> G["7 cfg 开关"]
    G -.->|"实机异常"| E
```

| 步 | 做法 | 对应代码 |
| --- | --- | --- |
| 1 定特征码 | 从反汇编器里拷 20+ 字节，通配掉会变的立即数/位移 | `Demo/src/Config.cpp` 的 cfg 模板 |
| 2 验证唯一命中 | 离线跑一次，多命中就加长特征码 | `Scan::FindAllInModule` |
| 3 写校验项 | 每个会变的字节都进校验；不要"看起来一样就不校验" | `Patch::Verify` + `Pattern::Concrete` |
| 4 全有或全无 | 任一失败就不写 | `Patch::Verify` 返回 false → `Apply` 直接拒绝 |
| 5 失败降级 | 单功能失败只跳过，不 `return` 整个 `Install` | `Hooks::Install` 的分支 |
| 6 计数器 + DIAG | 无条件计数 + 每秒一条 | `g_scans / g_hits / g_applied`、`DiagLoop` |
| 7 cfg 开关 | 默认关、三处写值、一键回退 | `Config` 的 `enabled` |

## 3. 三层测试

| 层 | 做什么 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| 1 离线单测 | `demo_test.exe`，纯内存 fixture | 解析、唯一命中、校验、写入、cfg 容错都对 | 运行时是否真的被调用 |
| 2 加载自检 | 由 DDModLoader 拉起，看日志 | 插件被加载、日志能写、cfg 能生成、补丁能落 | 游戏表现是否正确 |
| 3 实机场景 | 进存档/地牢/界面实操 | 功能真的可用、无闪退、不与他人冲突 | —— |

### 3.1 离线单测

```bash
env -u DDPath -u DD_DEPLOY xmake build demo_test
./build/windows/x64/releasedbg/demo_test.exe
# == ALL PASS ==  ，失败时退出码 1
```

改完 `Scan/Patch/Config` 先跑这个。它不会加载 DLL，也不需要游戏。

### 3.2 加载自检

只能通过 DDModLoader 加载。最省事的路径是用测试目录，见 §4。启动后只看到主菜单就够，重点看日志：

```
[INFO] ==== Demo v1.0.0 log opened ====
[INFO] Demo v1.0.0 loading
[INFO] no pattern configured (safe no-op). ...
```

### 3.3 实机场景

每改一个功能，固定复现一遍：进存档 → 触发目标界面/行为 → 退出 → `save-log` 留证。日志和截图一起留，否则过两天没人能判断"当时到底什么样"。

## 4. 部署

### 4.1 只构建不部署（默认）

```bash
env -u DDPath -u DD_DEPLOY xmake build Demo
```

产物：`build\windows\x64\releasedbg\Demo.dll` + `.pdb`。

### 4.2 部署到测试目录（推荐）

```bash
# 第一次：建测试目录（复制 exe、联接数据目录、记录真实安装基线）
python -B D:/workplace/DD.ModLoader/tools/rig.py create

# 每次部署：把 DDPath 指向测试目录，再带 DD_DEPLOY=1 构建
source D:/workplace/DD.ModLoader/tools/env.sh
export DDPath="D:/Program Files (x86)/Steam/steamapps/common/DarkestDungeon_Dev"
export DD_DEPLOY=1
xmake build Demo
# → 复制到 <测试目录>\mods\DemoMod\Demo.dll

# 确认部署目标是测试目录、真实安装没被动
python -B D:/workplace/DD.ModLoader/tools/rig.py status
python -B D:/workplace/DD.ModLoader/tools/rig.py verify-real

# 跑一次自检（45 秒后强杀，只看启动到主菜单）
python -B D:/workplace/DD.ModLoader/tools/rig.py run --seconds 45
python -B D:/workplace/DD.ModLoader/tools/rig.py save-log demo-first-run
```

注意 `rig.py deploy <方案>` 只认它自己 `PROFILES` 里的方案，那些是 DD.ModLoader 自带测试插件的；本模板用上面的"DDPath 指向测试目录 + 构建部署"路径。

需要进存档的长时间操作时，用测试目录里的 `start.bat` 正常启动、正常退出，别用 `run`（强杀可能撞上写存档）。

### 4.3 部署到真实安装（最后一步）

确认实机验收通过、且准备好回退方案后：

```bash
source D:/workplace/DD.ModLoader/tools/env.sh
export DD_DEPLOY=1
xmake build Demo        # env.sh 已把 DDPath 设成真实安装
```

**部署前必须关闭游戏**：DLL 被占用时复制会失败。改 cfg 也要重启游戏才生效。

## 5. 回退

| 层级 | 操作 | 效果 |
| --- | --- | --- |
| 功能级 | cfg 里对应键改 `0` | 只关该功能，其余不变 |
| 插件级 | cfg `enabled = 0` | 连特征码都不扫，完全不碰内存 |
| DLL 级 | `DDModLoader.cfg` 写 `0\|<绝对路径>`，或删掉 dll | 整个 DLL 不加载 |
| 构建级 | 不设 `DD_DEPLOY` 或清掉 `DDPath` | 构建对游戏目录零副作用 |

`mods\DD Mod Lib\` 里的 DLL 是**强制加载**的，GUI 关不掉，只能用 `DDModLoader.cfg` 的 `0|` 或移走文件。

## 6. 排错

| 现象 | 先看哪里 | 常见原因 |
| --- | --- | --- |
| 完全没有日志文件 | `DDModLoader.log` | DLL 没被加载（路径/`0\|` 禁用/依赖缺失）；或日志目录不可写，落到了 `%LOCALAPPDATA%` |
| 日志只有 `log opened` 没有后续 | — | `enabled = 0` 或 `pattern` 为空（这是设计上的安全空转） |
| `pattern not found` | 你的特征码 | exe 更新了；特征码跨了指令边界；通配位置写多了 |
| `pattern is ambiguous` | 日志里列的候选 RVA | 特征码太短；换成 20+ 字节并带上不变量 |
| `byte N differs` | 日志里的第 N 个字节 | 版本不符；或你抄错了字节；或别的插件先改了这一处 |
| `length mismatch` | cfg | pattern 与 replacement 字节数不同 |
| 补丁成功但功能没效果 | 计数器 `[DIAG]` | `applied=1` 但无效果 → 改的位置不对，不是没装上 |
| 游戏启动卡住 | — | debug 构建在等调试器（`#ifndef NDEBUG` 那段），换成 releasedbg |
| 部署失败 | 构建输出 | 游戏还开着，DLL 被占用 |
| 真实安装被动过 | `rig.py verify-real` | 立刻停，`DDPath` 指错了 |

## 7. 游戏更新后

顺序固定，不要跳步：

1. 记录新 exe 的 SHA1 / 大小，和上次比对；
2. 逐条重校特征码，要求**唯一命中**；
3. 重校校验项（字节、常量、偏移）；
4. 重跑离线单测，补上新字节的断言；
5. 测试目录走一遍三层验收；
6. 实机场景逐项留证；
7. 校准完成之前，任何"新版可用"的说法都不成立。

校验会主动拒绝不匹配的版本，这是设计行为，不是 bug。
