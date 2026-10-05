# Hover Translate

[English](README.md) · [简体中文](README.zh-CN.md)

开源的英语 ↔ 简体中文鼠标悬停翻译工具。**0.3.0** 通过随附的
**GNOME 50 扩展**，支持 Ubuntu 26.04 默认 Wayland 桌面的自动悬停翻译。
本地 Tesseract OCR、Argos 句子模型和可选 CC-CEDICT 词典不需要 API 密钥。
采用 GPL-3.0-or-later，保留 Crow Translate 版权及修改声明。

本版仍为预览版。已在真实的无显示器 GNOME 50 合成器中验证鼠标输入、
截图、OCR、双向离线模型翻译及弹窗。用户实际桌面、分数缩放和其他 GNOME
版本仍需验证。模型不包含在安装包内，需要首次下载。

## Ubuntu 26.04 安装与使用

从 [v0.3.0 发布页](https://github.com/LincolnGothic/Linux_mouse_hover_translation/releases/tag/v0.3.0)
下载 amd64 安装包；对应完整源码和校验文件在同一页面。
先从托盘菜单退出旧进程，再安装：

```bash
sudo apt install ./hover-translate_0.3.0_amd64.deb
/usr/bin/hover-translate
```

1. 点击 **Set up GNOME hover**，为当前账户安装并启用随附扩展。
   安装程序备份旧扩展，保留其他扩展设置。**注销并重新登录**，再打开软件。
2. 保留 **Translation → Offline**，点击 **Install offline models**。
   保持软件打开，等待私有 CPU Python 环境、两个翻译模型和可选词典安装完毕。
3. 英文译中文选择 **Simplified Chinese**，中文译英文选择 **English**。
   点击 **Test translation** 验证。
4. 勾选 **Enable hover translation**，点击 **Apply**。
   鼠标在其他应用的一行文字上停留，即在附近显示翻译；移动鼠标或按 Esc 关闭。

扩展只支持 **GNOME 50**，不要关闭 GNOME 的扩展版本检查。
如果 GNOME 全局禁用了用户扩展，请在 Extensions 中打开总开关。
没有托盘时请保留软件窗口；关闭最后一个窗口会退出软件并停止自动翻译。
KDE 和其他 Wayland 桌面需要单独的集成，本扩展不支持。

0.3.0 修复了官方模型服务器拒绝 Python 默认下载标识而返回 HTTP 403 的问题，
下载器使用真实的软件名称。旧模型的 Stanza 元数据不兼容问题也已修复，
使用本地分句，不会隐式下载额外模型。安装失败时运行
`hover-translate-offline-setup` 查看完整原因。
数据默认存放在 `~/.local/share/hover-translate`，不修改系统 Python。
旧版 0.2.0 不包含这些修复或 Wayland 自动悬停支持。

## 截图、词典与隐私

**Translate screen region** 仍可手动截图。批准桌面截图后，在预览中拖动
框选完整文字；Enter 使用整个图像，Esc 取消。可修改识别文字后按 Ctrl+Enter。
小字号和暗色背景的 OCR 已改进，低置信度文字保留供修改。
**Translate text** 可输入文字，不依赖 OCR。

GNOME 自动悬停只在启用时读取全局鼠标位置，截取鼠标所在窗口及屏幕内的局部
区域，图像只在内存和本机会话总线上传递，不写截图文件或剪贴板。
锁屏、概览、移动鼠标、暂停、退出软件和停用扩展均取消任务。
软件不调用 Shell Eval，不启用 GNOME unsafe mode。
手动截图门户权限流程与自动悬停不同，仍需在用户实际桌面验证。

CC-CEDICT 主要提供中译英词义，以及有限的英文释义精确反查；句子使用 Argos。
取消 **Show dictionary definitions** 可强制用模型。可选路径留空使用默认值。
离线工作进程阻止网络连接；只有显式安装程序联网下载。
**Online → Mozhi** 会发送文字到所选服务器，不会在离线失败时自动启用。
模型和词典保留原始许可声明，不包含在本项目分发包内。

## 编译、验证与分发

依赖、开发和命令行步骤见 [English README](README.md)。
完整 Ubuntu/Docker 构建和真实模型验证：

```bash
bash tools/build-ubuntu26.04.sh --model-check
```

常规测试涵盖 OCR、X11、GNOME 桥接、截图门户、下载及取消逻辑；
额外的 `tools/test-gnome.sh` 使用隔离的真实 GNOME 合成器。
详细证据及限制见 [docs/TESTING.md](docs/TESTING.md)。

分发二进制时同时提供完整对应源码，包括扩展及构建、安装脚本。
本版对应 `hover-translate-0.3.0-Source.tar.gz`。
版权和许可见 [NOTICE.md](NOTICE.md)，上游来源见
[docs/UPSTREAM.md](docs/UPSTREAM.md)。
