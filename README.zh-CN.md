# Hover Translate

[English](README.md) · [简体中文](README.zh-CN.md)

开源的英语 ↔ 简体中文鼠标悬停翻译工具。**0.4.2** 将设置分为三个页签，修复小窗口中的横向溢出与按钮被滚动隐藏的问题。通过随附的
**GNOME 50 扩展**，支持 Ubuntu 26.04 默认 Wayland 桌面的自动悬停翻译。
本地 Tesseract OCR、Argos 句子模型和可选 CC-CEDICT 词典不需要 API 密钥。
采用 GPL-3.0-or-later，保留 Crow Translate 版权及修改声明。

本版仍为预览版。已在真实的无显示器 GNOME 50 合成器中验证鼠标输入、
截图、OCR、双向离线模型翻译及弹窗。用户实际桌面、分数缩放和其他 GNOME
版本仍需验证。模型不包含在安装包内，需要首次下载。

## Ubuntu 26.04 安装与使用

从 [v0.4.2 发布页](https://github.com/LincolnGothic/Linux_mouse_hover_translation/releases/tag/v0.4.2)
下载 amd64 安装包；对应完整源码和校验文件在同一页面。
先从托盘菜单退出旧进程，再安装：

```bash
sudo apt install ./hover-translate_0.4.2_amd64.deb
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

## 设置窗口

- **Hover**：悬停开关、目标语言、单词/行/句子、延迟、高亮、临时快捷键与 GNOME 设置。
- **Translation**：离线/在线引擎、服务器、词典开关、模型安装与翻译测试。
- **Advanced**：可选 OCR、Python、翻译模型与词典路径。

状态、**Screen region**、**Translate text**、**About**、**Apply**、**Close**
固定在页签外；小窗口仅需纵向滚动，鼠标滚轮和滚动条均可使用。改动后点击 **Apply**。

## 悬停文字范围

在 **Hover text** 中选择后点击 **Apply**：

- **Word**：默认只翻译鼠标下的单词。已有设置的模式保留。
  中文使用字符位置和 CC-CEDICT 匹配包含鼠标所指字符的最长已知词；
  没有匹配时仍使用 OCR 的词元，不保证所有中文语境都能正确分词。
- **Line**：只翻译当前行内相邻的文字；较大的水平空隙分开处理，
  如表格的 “Mode” 与 “What it translates” 不会合并。
- **Sentence**：同一段落、同一列的折行最多拼接 3 行、300 字符，
  遇到大空隙或不确定的边界退回当前文字段，不扩展为整段。

可勾选原文高亮；按住 **Shift** 临时查单词，**Ctrl+Shift** 临时翻译句子，
松开后恢复保存的模式。这些临时快捷方式可以在设置里关闭。
移入弹窗后可点击 **Copy translation**、**Pin/Unpin** 或 **Close**。
固定弹窗会暂停后续悬停查询，关闭、Esc、暂停或退出后恢复/停止。
只有点击复制才会写入剪贴板；不保存永久翻译历史。

从 0.4.1 升级：退出旧进程、安装新包并重新打开。扩展仍是 version 4，
已启用时不需要重装扩展或注销。0.4.0 及更早版本需在 **Hover** 页
点击 **Set up GNOME hover**，注销并重新登录。模型和设置保留。
旧扩展会被暂停并提示升级。英文词典查询新增常见词形还原，如 running → run，
但 CC-CEDICT 的英文反查覆盖范围仍有限，不能称为完整英汉词典。

## 截图、词典与隐私

设置窗口的 **Screen region**（托盘菜单 **Translate screen region**）可手动截图。批准桌面截图后，在预览中拖动
框选完整文字；Enter 使用整个图像，Esc 取消。可修改识别文字后按 Ctrl+Enter。
小字号和暗色背景的 OCR 已改进，低置信度文字保留供修改。
**Translate text** 可输入文字，不依赖 OCR。

GNOME 自动悬停只在启用时读取全局鼠标位置，截取鼠标所在窗口及屏幕内的局部
区域，图像只在内存和本机会话总线上传递，不写截图文件或剪贴板。
锁屏、概览、移动鼠标、暂停、退出软件和停用扩展均取消任务。
软件不调用 Shell Eval，不启用 GNOME unsafe mode。
手动截图门户权限流程与自动悬停不同，仍需在用户实际桌面验证。

CC-CEDICT 主要提供中译英词义，以及有限的英文释义精确反查；句子使用 Argos。
词典没有有效期，不需要定期更新才能使用。可按需要每 1–3 个月运行
`hover-translate-offline-setup --dictionary-only` 下载当前版本，随后重启软件
清除旧词义缓存。此操作不会更新 Argos 句子模型。
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
本版对应 `hover-translate-0.4.2-Source.tar.gz`。
版权和许可见 [NOTICE.md](NOTICE.md)，上游来源见
[docs/UPSTREAM.md](docs/UPSTREAM.md)。
