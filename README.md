<h1 align="center">ScreenOff Lock</h1>

<p align="center"><b>简体中文</b> | <a href="README.en.md">English</a></p>

<p align="center">轻量 Windows 程序，<b>屏幕被关闭、或者合上盖子时自动锁屏</b>。</p>

---
## 快速开始

前往 [Release 页面](https://github.com/AomiRaku/Screen-Off-Lock/releases) 下载  `ScreenOffLock.exe` 。

双击 `ScreenOffLock.exe` 运行即可，无需管理员权限。

想让它开机自启，直接右键托盘图标勾选「开机启动」即可（写的是注册表 Run 键）。

完全卸载：取消勾选「开机启动」→ 退出程序 → 删除 exe →
删掉注册表项 `HKCU\Software\ScreenOffLock`。

## 功能

- 运行后只在系统托盘显示一个图标，没有窗口
- **只有右键**才有反应，左键点击不做任何事
- 右键菜单：

  | 菜单项 | 说明 |
  | --- | --- |
  | 关闭屏幕时锁屏 | 可勾选 / 取消勾选，状态立即生效并自动保存 |
  | 合盖锁屏 | 同上；台式机没有盖子设备时自动置灰 |
  | 延迟锁定 | 子菜单，单选：立即 / 3 秒 / 5 秒 / 10 秒 / 30 秒。息屏或合盖后等这么多秒再锁定，中途重新亮屏或开盖就取消这次锁定 |
  | —— 分割线 —— | |
  | 开机启动 | 勾选后随系统登录自动启动；取消时会把注册表里那一项彻底删掉 |
  | —— 分割线 —— | |
  | 检查设置 | 检查电源计划里的电源按钮 / 合盖动作是否符合推荐值，并支持一键修正 |
  | 关于 | 显示作者、项目地址（可点击）、版本与注册表位置；另有一个语言按钮 |
  | 退出 | 移除托盘图标并结束进程 |

- **多语言**：界面支持 English / 简体中文。第一次运行时会先弹出语言选择窗口，
  选完记进注册表，之后不再询问；想换语言可以在「关于」里点
  「Language / 语言」重新选（换语言后已打开的对话框会关掉，重新打开即是新语言）。
- 设置保存在注册表 `HKCU\Software\ScreenOffLock`，全是 DWORD：
  - `LockWhenScreenOff` —— 关闭屏幕时锁屏，`1` 开 / `0` 关（默认开启）
  - `LockWhenLidClosed` —— 合盖时锁屏，`1` 开 / `0` 关（默认开启）
  - `LockDelay` —— 延迟锁定的秒数，`0` = 立即（默认不写，即视作 `0`）；只认
    `0` / `3` / `5` / `10` / `30`，其它值一律当 `0` 处理
  - `Language` —— 界面语言，`0` = English、`1` = 简体中文（**值不存在**即视为第一次运行）
  - `SkipSettingsWarning` —— 不再显示首次启动的设置提醒（默认不写，即每次都提醒）
- 开机启动写在 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 的
  `ScreenOffLock` 值里，内容是带引号的 exe 完整路径。**取消勾选时这个值会被
  直接删除**，不会留下 `0` 之类的残骸；Run 键本身是系统原有的，不会被创建或删除，
  同键下的其他启动项也不受影响。
- 重复运行 exe 时会弹窗提示「已在运行，请勿重复运行。」然后自动退出

## 从源码构建

```powershell
pwsh -File build.ps1
```

产物在 `dist\ScreenOffLock.exe`。

本仓库不带工具链。构建时如果找不到 `g++`，脚本会自动调用
`scripts/fetch-toolchain.py` 把便携版 **w64devkit**（自包含的 MinGW-w64，约 64 MB）
下载并解压到仓库内的 `.toolchain\`。全程不写系统目录、不需要管理员权限，
删掉 `.toolchain\` 就等于彻底卸载工具链。

需要本机有 Python 3（仅用于下载，编译不依赖它）。如果你已经装了 MinGW-w64，
把 `g++.exe` 放进 PATH 也可以，脚本会优先使用 `.toolchain\` 里的那一份。

```powershell
pwsh -File build.ps1 -Clean       # 重新编译
pwsh -File build.ps1 -Reinstall   # 连工具链一起重新下载
```

## 检查设置

本程序要生效有个前提：**屏幕得真的被关掉、盖子状态得被系统上报**。所以托盘
菜单里的「检查设置」会读取当前电源计划里这两项的取值：

| 项 | 电源设置 GUID | 推荐值（插电 / 电池） |
| --- | --- | --- |
| 电源按钮操作 | `{7648EFA3-DD9C-4E3E-B566-50F929386280}` | 关闭屏幕 \| 关闭屏幕 |
| 合盖操作 | `{5CA83367-6E45-459F-A27B-476B1D01C936}` | 不采取任何操作 \| 睡眠 |

两者都在子组 `{4F971E89-EEBD-4455-A8DE-9E59040E7347}`（电源按钮和盖子）下。
注意它们在控制面板里看得见，却属于**隐藏**的电源设置——`powercfg /query`
不会列出它们，只能按 GUID 通过 API 读写。取值就是电源计划里的索引：
`0` 不采取任何操作、`1` 睡眠、`2` 休眠、`3` 关机、`4` 关闭屏幕。

判断规则：

- 电池时合盖如果不是「睡眠」，但只要是「不采取任何操作」也算合规——在本程序
  的语义下两者效果一致，软件自己会把屏幕锁上。
- 其余各项严格比对推荐值。
- 两个供电模式分别给出结论：都合规是「设置正常。」，都不合规是「合盖和按下
  电源按钮时不能锁屏，只有屏幕自动关闭时才能锁屏。」，只错一项则单独指出。

**读取不需要管理员权限**（`PowerReadACValueIndex` / `PowerReadDCValueIndex`
普通用户即可调用，注意 `SchemeGuid` 必须传活动方案 GUID，传 `NULL` 会返回
`ERROR_INVALID_PARAMETER`）。但窗口里那两个「设置为推荐值」按钮要**写**电源
计划，那就必须提权，所以它们会通过 `ShellExecuteEx` + `runas` 启动一个提权
子进程（命令行参数 `--apply-ac` / `--apply-dc`）去写，写完后刷新界面。你在
UAC 上点「否」就什么都不会发生。

命令行带 `--apply-ac` 或 `--apply-dc` 时程序只写电源计划就退出，不建窗口、
不进消息循环、也不参与单实例检查。

首次启动时如果检测到设置不全在推荐值，会弹一次提示。勾选「不再显示」并确定后
写入 `SkipSettingsWarning`，之后不再打扰。

## 系统要求

| 系统 | 能否运行 | 「关于」图标 | 窗口圆角 |
| --- | --- | --- | --- |
| Windows 11 | 可以 | Segoe Fluent Icons 字形 | 有 |
| Windows 10 | 可以 | Segoe MDL2 Assets 字形（外观几乎一致） | 无，直角 |
| Windows 8.1 / 8 | 可以 | 系统位图图标 | 无，直角（系统本身也是直角风格） |
| Windows 7 | 可以 | 系统位图图标 | Aero 圆角 |
| Windows Vista | 理论可行，未验证 | 系统位图图标 | — |
| Windows XP | 不行 | — | — |

静态导入的 API 门槛是 **Vista**：`RegisterPowerSettingNotification` 由 Vista 引入，
XP 上会直接因缺少入口点而无法加载。PE 子系统版本是 5.2，导入表里只有
`ADVAPI32 / GDI32 / KERNEL32 / msvcrt / SHELL32 / USER32` 六个最基础的系统 DLL；
`dwmapi`、`shcore` 之类一律是运行时动态加载并带降级路径，不构成硬依赖。

字体和图标都是运行时探测、逐级回退的，不会出现某台机器因为缺字体而画出方框：
界面字体用 Segoe UI Variable Text → Segoe UI，图标字体用
Segoe Fluent Icons → Segoe MDL2 Assets → 系统位图图标。

需要说明的是，上表**只有 Windows 11 是实机验证过的**，其余各行是依据 API 与
字体的可用版本推断的。

## 实现说明

程序只做几件事：

1. 创建一个**不可见的顶层窗口**，用它接收系统消息；
2. 用 `RegisterPowerSettingNotification` 订阅两个电源设置，都经
   `WM_POWERBROADCAST` / `PBT_POWERSETTINGCHANGE` 送达：

   | 电源设置 GUID | 含义 | 触发锁屏的边沿 |
   | --- | --- | --- |
   | `GUID_CONSOLE_DISPLAY_STATE`<br>`{6FE69556-704A-47A0-8F24-C28D936FDA47}` | 显示器电源状态：`0` 关 / `1` 开 / `2` 变暗 | `1 → 0` |
   | `GUID_LIDSWITCH_STATE_CHANGE`<br>`{BA3E0F4D-B817-4094-A2D1-D56379E6A0F3}` | 盖子开关状态：`0` 已合上 / `1` 已打开 | `1 → 0` |

3. 命中断言后调用一次 `LockWorkStation()`；
4. 启动时声明 **Per-Monitor V2 级别的 DPI 感知**
   （`SetProcessDpiAwarenessContext`，取不到则退回 `SetProcessDPIAware`）。
   不声明的话，系统会把整个进程按 DPI 不感知处理，再把窗口、右键菜单和对话框
   整体做位图拉伸，于是在高 DPI 屏幕上文字和界面都会发虚。

两个状态机都只在“由开转关”的那一次边沿动作，重复通知不会重复锁屏；程序启动时
如果屏幕本来就是关的、或者盖子本来就是合上的，也不会立刻锁屏。
`LockWorkStation()` 是幂等的，两条路径同时命中也不会出问题。

设了延迟锁定时，命中边沿只是记下一个「待定」标记再起一个 `SetTimer` 计时器，
到点前会重新核对一次开关和状态才锁定。屏幕重新亮起（含转为变暗）取消「关屏」
那条待定，盖子重新打开取消「合盖」那条——两条各自独立：笔记本合盖时这两条通知
通常都会来，只开盖而屏幕没亮的话，「关屏」那条仍会按时锁上。

判断机器有没有盖子用的是 `GetPwrCapabilities` 的 `LidPresent`，**不是**
`RegisterPowerSettingNotification` 的返回值——实测后者并不校验 GUID 是否真实
存在，随手编一个 GUID 它照样返回成功句柄，拿它的返回值当依据会出错。

## 界面

可视部分只有托盘菜单和「关于」窗口，都按 Windows 11 的视觉规范做：

- **托盘右键菜单**靠 `src\app.manifest` 里声明的 Common Controls 6.0 依赖
  启用现代视觉样式。缺了这段声明，Windows 会退化到 Windows 95 时代的经典
  三维控件外观——立体边框、直角、老式配色。
- **「关于」窗口**没有用 `MessageBox`：它的图标只能是 `IDI_*` 那套老式位图，
  窗口外观也停留在经典控件样式。这里是自绘窗口——DWM 提供圆角，字体用
  Segoe UI Variable Text（老系统退回 Segoe UI），图标取自 Segoe Fluent Icons
  字形（`U+E946` ⓘ），按钮仍用系统标准按钮以保留原生手感。

命令行带 `--about` 可以直接打开「关于」窗口，方便单独查看。

## 已知限制

- **无法区分“按下电源按钮关屏”和“屏幕超时自动关闭”**。系统对这两种情况发出
  的是完全相同的通知，不告诉应用关屏的原因。所以勾选之后，任何原因导致的屏幕
  关闭都会触发锁屏。这一点是 Windows 的限制，不是实现取舍。
- **不覆盖台式机“直接关掉显示器电源”的场景**。物理断电会让显示器从系统拓扑里
  消失，走的是另一条检测路径；而且很多显示器待机时仍保持 HPD 高电平，系统根本
  察觉不到。这部分留待后续版本。
- 托盘图标由 `scripts\make-icon.ps1` 生成：用 Windows 11 自带的 Segoe Fluent Icons
  字形（显示器 + 锁）拼成一个圆角方块图标，`src\app.ico` 已入库。它仍然是占位性质，
  随时可以换掉。
- “关于”里的作者与项目地址还是占位文本，等待填写。

## 目录结构

```Tree
ScreenOffLock\
├─ src\main.cpp                全部源码（单文件）
├─ src\app.rc                  图标、版本信息与清单资源
├─ src\app.manifest            应用程序清单（Common Controls 6.0 / DPI 感知）
├─ src\app.ico                 程序图标（由脚本生成，已入库）
├─ scripts\make-icon.ps1       生成 src\app.ico
├─ scripts\fetch-toolchain.py  下载便携工具链
├─ build.ps1                   构建脚本
├─ build\                      中间产物（资源 .o）
├─ dist\                       编译产物
└─ .toolchain\                 自动下载的工具链（可删）
```

## 许可证

本项目以 [MIT 许可证](LICENSE) 发布。
