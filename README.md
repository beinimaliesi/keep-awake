# keep-awake (C++)

防休眠工具，兼顾 CLI 与系统托盘。使用 C++17 开发。

保持屏幕唤醒——从系统托盘或终端运行。默认以托盘方式运行，可无限、定时或按每日时间窗口保持唤醒。

## 功能

- 默认从系统托盘运行
- 无限 / 定时 / 每日窗口三种激活模式（支持跨午夜窗口）
- 无托盘（headless）与静默（daemon）运行
- 平台原生防休眠机制

| 平台    | 防休眠机制                      | 说明                        |
| ------- | ------------------------------- | --------------------------- |
| Windows | `SetThreadExecutionState`       | 请求系统与显示器持续可用    |
| macOS   | `IOPMAssertionCreateWithName`   | 未验证，交由 CI 编译校验    |
| Linux   | `xdg-screensaver reset`（周期） | 托盘暂不支持，headless 可用 |

## 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- Windows 产物：`build/Release/keep-awake.exe`
- macOS / Linux 产物：`build/keep-awake`

要求 CMake 3.15+ 与 C++17 编译器（Windows 用 MSVC，macOS 用 clang，Linux 用 gcc/clang）。

## 用法

```sh
# 无限保持唤醒并显示托盘
keep-awake

# 45 分钟后退出
keep-awake --duration 45

# 每日 9:00-17:00 保持唤醒（跨午夜窗口如 21:00-06:00 亦可）
keep-awake --active-window "9am-5pm"

# 无托盘、静默运行
keep-awake --no-tray --daemon
```

## CLI 参数

| 选项                          | 说明                                                    |
| ----------------------------- | ------------------------------------------------------- |
| `--duration <minutes>`        | 给定分钟后退出                                          |
| `--active-window <start-end>` | 在本地时间窗口内保持唤醒，如 `9am-5pm` 或 `21:00-06:00` |
| `--interval <seconds>`        | 刷新间隔（默认 30）                                     |
| `--no-tray`                   | 无系统托盘运行                                          |
| `--tray`                      | 显式启用托盘（默认）                                    |
| `--daemon`                    | 静默运行                                                |
| `--debug`                     | 打印状态变化与刷新活动                                  |
| `-h`, `--help`                | 帮助                                                    |
| `-V`, `--version`             | 版本                                                    |

## 托盘菜单

- **Until stopped / 15 / 30 / 60 / 120 minutes** — 选择激活时长
- **Pause / Resume** — 临时释放或恢复唤醒状态
- **Follow daily window** — 回到配置的每日窗口
- **Quit** — 释放唤醒状态并退出

## 图标

- `assets/keep-awake-icon-design.svg` — 图标母版（可编辑）
- `assets/render_icon.mjs` — 用 `@resvg/resvg-js` 将母版渲染为多尺寸 PNG，并打包成 `assets/tray.ico`（16/24/32/48/64/256）与预览 `assets/tray_preview.png`
- 重新生成：

```sh
npm install   # 首次，安装渲染依赖
npm run icon  # 渲染并生成 tray.ico
```

Windows 构建时通过 `resources/app.rc.in` 将 `.ico` 作为 Win32 资源嵌入，托盘使用 `LoadIconW` 加载。
