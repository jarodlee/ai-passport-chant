[English](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/docs/development/release/chant-release.md)

# v1.0.0-chant — AI Passport 佛音播放器

首个公开固件版本，适用于 FoloToy AI Passport（ESP32-C3、8 MB Flash）。

## 功能

- 五首离线佛音；开机默认播放《心经长版》节选，音乐音量 80%。
- 主菜单扫描 Wi-Fi 第一、二维码第二、Cindy 对话最后；修复 Wi-Fi 图标和中文标题缺字。
- 常用网络保存、免密码临时手机配网、安庆天气、主页网络名称和日期时间。
- Cindy 按住说话、打断重问、语音缓冲、独立回复音量和远程上传失败恢复。
- 包含用户授权公开的离线二维码。NFC 显示手机靠近位置提示；固件不改写被动标签。

## 构建与配置

启用 ESP-IDF 5.5.3，运行 `./tools/validate.sh` 完成全部检查；仅构建并验证合并镜像
可运行 `./tools/validate.sh --firmware`。下载固件来自干净的 GitHub 源码构建，
不含用户 Wi-Fi 密码、私有配对密钥及远程桥接地址。刷入后需自行配网。
Cindy 对话需另行准备桥接，并构建使用匹配私有密钥的设备固件；公开占位密钥不构成可用配对。
详见[桥接配置说明](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/docs/development/chant-bridge-source.zh_CN.md)。
仓库包含桥接 1.6 的先检索后回答源码，但该版本尚未完成真机验收。

## 刷写与使用

下载下方的 `FoloToy-AI-Passport-full.bin` 和 `SHA256SUMS`。
Linux 运行 `sha256sum -c SHA256SUMS`，macOS 运行 `shasum -a 256 -c SHA256SUMS` 验证下载。
用数据线连接设备，在兼容的桌面浏览器打开[在线刷机工具](https://ai-passport.folotoy.cn/tools/web-flasher/)，
将此**完整合并镜像写入 `0x0`**。完整镜像可能重置已存设置；已配对设备若要保留配置，
不要直接用此公开完整镜像覆盖更新。不需要整片擦除。

上下键选择，确定键打开。长按确定或双击上键返回主页（Cindy 页长按确定为录音）。
“连接 Wi-Fi → 手机配网”打开临时 `Passport-XXXX` 热点，免密码连接后访问
`http://192.168.4.1`，配置 2.4 GHz 网络。扫描 Wi-Fi 页面按确定重新扫描。

## 验证与限制

发布流水线执行完整的主机测试、静态检查及固件构建，校验合并镜像后才发布。
此公开配置尚未刷机实测；相关本机私有配置已通过启动及语音检查。
LCD 显示二维码后的手机扫码、长期弱网运行仍待验证。录音来源和权利说明见
[素材文档](https://github.com/jarodlee/ai-passport-chant/blob/v1.0.0-chant/assets/README.zh_CN.md)；
代码许可证不授予第三方录音的版权许可。
