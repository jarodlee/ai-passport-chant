<p align="right"><strong>简体中文</strong> · <a href="chant-bridge-source.md">English</a></p>

# 语音桥接源码

配套源码位于 [`integrations/ai-passport-bridge`](../../integrations/ai-passport-bridge/)，
包含 Cindy 插件清单、沙箱控制器、原生 Node 工作进程、面板、图标与协议测试。
1.5 增加幂等上传、定长 JSON 和结束任务恢复；1.6 源码要求知识问答先查询公开
网络再回答，纯问候无需检索。查询失败必须如实说明，不能编造搜索结果。
1.6 已打包并通过主机测试，尚未安装或完成真机验收。已安装的桥接仍为 1.5；
固件连接复用已通过实际 HTTPS 设备对话测试。

## 准备私有安装

这里是源码，不是可以直接安装的完整包。配对凭据、插件包、本机日志、原生
可执行文件与识别模型均不入库。请提供可信的 macOS ARM64 whisper.cpp CLI
到 `native/whisper-cli`，并把兼容的 tiny 多语言模型放入 `native/ggml-tiny.bin`，
保留对应许可证。打包前设置可执行权限。桥接还需要 macOS Tingting 与安装
Pillow 的 Python 3，用于播报和文字渲染。

`pairing.example.json` 只有非秘密占位值。为私有 `pairing.json` 新生成随机
16 字节、编码为 32 位十六进制的密钥，并把相同密钥放入固件被忽略的
`main/chant_bridge_local.h`。实际部署不能使用占位值，不复用其它设备的密钥，
不提交私有配置，也不分发含配对配置的固件或插件。私有固件头还可以配置 HTTPS
入口，详见[跨网络语音连接](chant-player.zh_CN.md#跨网络语音连接)。

准备完整的私有源码后，使用 Cindy Forge 校验、打包；安装仍须单独明确授权。
更新 Git 源码不会自动安装插件或给设备刷机。

插件复用自己创建的可见 Cindy 任务，保留已配置模型。已测试的任务权限是默认
`ask`。把任务改成完全访问或未获插件批准的更高权限，可能出现
`Task permission exceeds plugin dispatch policy`；应在 Cindy 任务界面恢复
默认权限。公开网络查询在默认权限下已实测可用；宿主仍要求授权时须遵循其流程，
先检索的提示词不能绕过审批或授予本地写权限。

## 执行主机检查

在桥接源码目录运行：

```sh
node --check main.js
node tests/test_voice_prompt.cjs
node tests/test_voice_jobs.cjs
node tests/test_voice_http.cjs
python3 tests/test_voice_codec.py
```

独立编解码对照需要 Python 3.12 或更低版本（使用 `audioop`）；Node 检查需要
Node 18 或更高版本。测试使用合成音频和模拟的 Cindy／识别／播报接口，不要求
私有配对配置、CLI／模型或正在运行的 Cindy，也不证明真实搜索质量、音质和公网
稳定性。固件完整门禁见[构建与测试](engineering/build-and-test.zh_CN.md)。
