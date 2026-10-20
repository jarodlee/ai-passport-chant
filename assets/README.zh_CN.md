<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

唱诵播放器使用 [Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk)，
许可为 SIL Open Font License，本地许可文件为 `fonts/OFL.txt`。
`fonts/NotoSansCJKsc-Regular.otf` 为源字体；`fonts/chant_font_18.c` 和
`fonts/chant_font_28.c` 为未压缩的 4bpp LVGL 子集，包含 ASCII 和
`fonts/chant-glyphs.txt` 中的正文字符（18 px），标题字体（28 px）仅包含
`fonts/chant-title-glyphs.txt` 中的标题字符。执行 `python3 tools/generate_chant_fonts.py`
生成（需 Pillow/FreeType）；生成器逐像素回读检查，静态门禁检查应用实际文字覆盖。
大号字体标题和主页菜单名称由应用源码提取，不再依赖单独手工维护的标题清单。
扫描页的 Wi-Fi 图标通过独立控件绘制弧线和圆点，不需要图片或额外图标字体。

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

用户于 2026-10-20 授权公开本次二维码。原图、验证预览及 I1 头文件分别为
`images/chant-qr.jpg`、`images/chant-qr-preview.png`、`images/chant-qr.h`。
运行 `python3 tools/prepare_chant_qr.py --public assets/images/chant-qr.jpg`
可重新生成。OpenCV 解码后去掉装饰图标，重新编码相同目标，保留四模块白边、
采用整数倍像素缩放；196 像素方图共 4,908 字节，含八字节黑白调色板。
不带 `--public` 时仍生成被忽略的 `main/chant_qr_local.h` 和
`images/chant-qr-local*`，可覆盖公开默认图片。本次授权不包括配对密钥、
Wi-Fi 密码及含私有配置的本机固件。

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

离线唱诵播放器使用[这段 YouTube 录音](https://www.youtube.com/watch?v=HQR_jVqWE_Y)
的前 8:30。`music/dabei.mp3` 为保留的 64 kbps 单声道截取，首尾各加了短淡入淡出。
未找到明确的再分发许可，素材仅用于用户要求的本机开发和设备播放，
不提交、不上传、不发布。
`main/CMakeLists.txt` 将 `music/dabei.adpcm` 嵌入应用镜像。
音频为 11.025 kHz 单声道 IMA ADPCM，高半字节优先，预测器和索引初始值为零；
按每次 512 个采样解码成 16 位 PCM，不在内存中加载完整录音。
来源、时长和哈希见 `music/dabei-metadata.json`。
执行 `python3 tools/encode_chant_audio.py` 可重新生成（需 FFmpeg 和 Python 3.9 audioop）；
静态门禁将实际解码器与独立参考结果比较。

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 11.025 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

第二版另外加入[万佛城官方音频目录](https://www.cttbusa.org/sitemap.asp.html)中的
[观音菩萨圣号](https://www.cttbusa.org/recitation/gwanyinchant.mp3)（37.329 秒，
`music/guanyin.mp3`）和[消灾吉祥神咒](https://www.cttbusa.org/recitation/auspicious_mantra.mp3)
（76.643 秒，`music/auspicious.mp3`）。第四首为[《心经》](https://www.youtube.com/watch?v=j6YKyMu2Kfc)
（`music/xinjing.mp3`，305.850 秒）。第五首为另一段 YouTube《心经》长版
节选（`music/xinjinglong.mp3`，290.000 秒）。各曲均保留源文件与各自的 ADPCM、
`*-metadata.json`，同样只用于本地，不提交或发布。编码脚本重新生成五首
11.025 kHz 资源和 `main/chant_playlist.c`；两首 CTTB 录音、两首《心经》保持
完整，大悲咒只取能放下五首的 8:30 片段。字体生成器收集所有
`main/chant_*.c` 的固定文字并生成运行时字表。
