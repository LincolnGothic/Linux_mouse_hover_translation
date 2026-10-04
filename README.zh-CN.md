# Hover Translate：Linux 鼠标悬停翻译

[English](README.md) · [简体中文](README.zh-CN.md)

一个开源 Linux 鼠标悬停翻译工具，支持 **英语 ↔ 简体中文**。
基于 Crow Translate 4.1.0 的 OCR 和翻译组件，使用本地 Tesseract
识别鼠标下方的文字，再通过用户选择的 Mozhi 服务器获取译文。
本项目采用 **GPL-3.0-or-later**，保留上游版权及许可证声明。

![鼠标悬停翻译弹窗](docs/images/hover-demo.png)

图片来自真实 X11/OCR 集成测试；其中的译文由本地 Mozhi 协议测试服务器提供。

## 主要功能

- 鼠标停留默认 600 毫秒后，识别指针所在的文字行并显示翻译。
- 可选择并保存目标语言：英语或简体中文。
- 可设置悬停延时（100–3000 毫秒）、Mozhi 服务器和 OCR 模型目录。
- 移动鼠标或按 Escape 关闭弹窗；支持暂停和系统托盘控制。
- 弹窗不抢夺焦点，不修改文本选择或剪贴板。
- 取消过期任务，设置 10 秒请求超时，最多缓存 128 条译文。
- 提供 GUI、本地 OCR/在线翻译命令行诊断和自动化测试。

## 当前支持范围

- **X11/Xorg、单显示器、100% 缩放**。
- 英语和简体中文；目标语言在设置中固定选择。
- Qt 6.8 或更新版本、Tesseract 5，以及 `eng` 和 `chi_sim` 模型。
- 已验证的构建平台：Debian 13 amd64。
- 在线翻译需要可访问且启用 Google 引擎的 Mozhi 服务器。

首版暂不支持原生 Wayland、多显示器、非整数缩放、无障碍 API 取词或离线翻译。
OCR 对过小、低对比度或艺术字体可能识别不准确；捕获区域最多为 700 × 160
像素，并限制在指针所在窗口内，过长的文字行可能被截断。
语言判定针对英语和中文，不能代替通用语言识别。

## 安装与运行

若 [Releases](https://github.com/LincolnGothic/Linux_mouse_hover_translation/releases)
中已有发布包，下载 Debian 13 amd64 安装包及对应源码，然后运行：

```bash
sudo apt-get install ./hover-translate_0.1.0_amd64.deb
hover-translate
```

从源码编译：

```bash
git clone https://github.com/LincolnGothic/Linux_mouse_hover_translation.git
cd Linux_mouse_hover_translation
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config \
  qt6-base-dev qt6-base-dev-tools qt6-scxml-dev qt6-svg-plugins \
  libtesseract-dev libleptonica-dev libxcb1-dev \
  tesseract-ocr-eng tesseract-ocr-chi-sim fonts-noto-cjk
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF
cmake --build build --parallel 4
./build/hover-translate
```

![语言、服务器及隐私设置](docs/images/settings.png)

第一次运行默认暂停。选择 **Translate into**，使用 **Test server**
测试服务器，勾选 **Enable hover translation**，再点击 **Apply**。
设置默认保存到 `~/.config/LincolnGothic/HoverTranslate/settings.ini`。

有系统托盘时，关闭设置窗口后可继续使用托盘控制；没有系统托盘时，
应保持设置窗口打开，关闭窗口将退出程序。

默认服务器为 `https://mozhi.aryak.me`。公共服务器可能限流、故障或被网络阻止，
可以改用其他支持 Google 引擎的 Mozhi HTTPS 实例。本项目不需要 API 密钥。
仅本机回环地址允许使用 HTTP；服务器地址不能包含凭据、查询参数或片段。

## 隐私

截图及 OCR 处理在本机完成。启用悬停后，指针所在行的识别文字会发送到配置的
Mozhi 服务器，该服务器可能再转发给 Google。服务器的日志、服务条款及可用性
由运营者决定。不要在不信任这些服务时对敏感内容启用自动悬停翻译。
应用不保存文字历史，译文缓存只存在于内存中，修改设置时会清空。

## 测试与命令行

```bash
sudo apt-get install libxcb-xtest0-dev xvfb xauth x11-utils
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build --parallel 4
xvfb-run -a -s '-screen 0 1200x900x24' ctest --test-dir build --output-on-failure

./build/hover-translate --ocr image.png
./build/hover-translate --translate 'Hello world' --target zh-CN
./build/hover-translate --translate '你好世界' --target en
```

测试使用真实 Tesseract 模型、X11 窗口和 Crow HTTP 翻译组件；网络响应由本地测试
服务器提供。测试通过不代表公共服务器始终可用。详细验证范围见
[测试说明](docs/TESTING.md)，云环境开发步骤见 [English README](README.md)。

## 许可证、修改与分发

完整许可证见 [LICENSE](LICENSE)，版权及依赖说明见 [NOTICE.md](NOTICE.md)，
上游版本、文件来源及修改记录见 [docs/UPSTREAM.md](docs/UPSTREAM.md)。
这是独立维护的派生项目，并非 Crow Translate 官方发布版本。

向他人分发修改后的二进制时，应依据 GPL-3.0-or-later 向接收者提供完整对应源码，
包含实际修改以及构建、安装脚本，并保留许可证与版权声明。仅自己私下修改和使用、
不分发程序，并不因此必须公开修改。发布步骤见 [docs/RELEASE.md](docs/RELEASE.md)。

欢迎通过 [GitHub Issues](https://github.com/LincolnGothic/Linux_mouse_hover_translation/issues)
提交问题，或参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 提交修改。
