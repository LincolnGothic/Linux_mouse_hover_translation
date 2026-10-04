# Hover Translate：离线英语 ↔ 简体中文翻译

[English](README.md) · [简体中文](README.zh-CN.md)

[下载发布包及对应源码](https://github.com/LincolnGothic/Linux_mouse_hover_translation/releases)

0.2.0 默认使用本地 Argos 模型进行翻译，可选 CC-CEDICT 词典释义。
首次下载运行环境及模型后，翻译不需要联网或 API 密钥。
项目采用 **GPL-3.0-or-later**，保留 Crow Translate 4.1.0 的版权及修改声明。

## Ubuntu 26.04 安装

使用本次构建的 Ubuntu 26.04 amd64 安装包和对应源码：

```bash
sudo apt install ./hover-translate_0.2.0_amd64.deb
hover-translate
```

在设置中保留 **Translation → Offline**，点击 **Install offline models**。
首次联网安装会下载私有 Python 环境、CPU PyTorch、Argos Translate 1.11.0
和两个方向的 1.9 翻译模型，可选下载 CC-CEDICT。
文件存放在 `~/.local/share/hover-translate`，不修改系统 Python。
完成后点击 **Test translation**，再使用 **Translate text** 或
**Translate screen region**。首个句子翻译需要加载模型，速度及准确度取决于
电脑配置、模型和 OCR 质量。

若安装失败，在终端运行 `hover-translate-offline-setup` 查看完整原因。
旧版 v0.1.0 发布包不包含这些功能。完整说明见
[Ubuntu 26.04 使用指南](docs/UBUNTU-26.04.md)。

## Wayland 与 X11

- **Wayland**：桌面截图权限流程和框选区域 OCR 翻译；输入文字翻译。
- **X11/Xorg**：上述功能，以及单屏、100% 缩放条件下的自动鼠标悬停翻译。
- 原生 Wayland 全局自动悬停仍未实现，设置中的悬停开关会禁用。

点击 **Translate screen region**，先在系统对话框中确认截图，再在本地
预览中拖动框选文字。按 Enter 使用整个预览，按 Esc 取消。
程序不读取全局鼠标坐标，不修改其他应用的文字选择或剪贴板。
可在桌面键盘设置中添加快捷键，命令为 `hover-translate --capture`。

截图需要 `xdg-desktop-portal` 和桌面后端。标准 Ubuntu GNOME 使用
`xdg-desktop-portal-gnome`。实际 GNOME 权限流程仍需在用户桌面验证，
不能把测试用截图服务等同于真实桌面截图。

## 词典与隐私

CC-CEDICT 主要用于中文 → 英文释义，也支持精确匹配英文释义后反查中文，
但不是完整的英汉词典。没有匹配词条时使用 Argos 句子翻译。
可取消 **Show dictionary definitions**，让所有文字都使用模型。
设置支持已有 CC-CEDICT `.u8` 文件；下载文件保留完整版权及许可头。

离线工作进程阻止网络连接，包括隐式下载；只有单独安装程序下载资源。
OCR 与翻译在本机运行，不保存文字历史。系统截图门户管理临时截图。
词典、模型和运行时不包含在本项目安装包中，各自保留自己的许可。

**Online → Mozhi** 是可选在线模式，会将识别文字发送给所选服务器，
可能再转发到 Google。离线失败时不会自动改用在线服务。

## 编译、测试与命令行

Ubuntu 26.04 / Debian 13，Qt 6.8+；编译依赖见 [English README](README.md)。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON
cmake --build build --parallel 3
dbus-run-session -- xvfb-run -a -s '-screen 0 1200x900x24' \
  ctest --test-dir build --output-on-failure
bash tools/test-wayland.sh build
python3 offline/setup_offline.py
./build/hover-translate --translate 'Hello world' --target zh-CN --no-dictionary
./build/hover-translate --translate '你好世界' --target en --no-dictionary
```

测试依赖包括 `libxcb-xtest0-dev xvfb xauth x11-utils dbus-x11 weston`。
OCR、翻译、帮助及版本命令支持无图形界面。测试覆盖真实 OCR、X11、词典
查找和截图门户协议，不等同于句子翻译质量或所有桌面验证。
详见 [测试说明](docs/TESTING.md)。

## 开源与分发

许可证见 [LICENSE](LICENSE)，依赖及版权见 [NOTICE.md](NOTICE.md)，
上游版本与修改见 [docs/UPSTREAM.md](docs/UPSTREAM.md)。
分发二进制时同时提供完整对应源码，包含修改和构建、安装脚本。
源码包为 `hover-translate-0.2.0-Source.tar.gz`。另行分发词典和模型时须
遵守各自许可。发布步骤见 [docs/RELEASE.md](docs/RELEASE.md)。
