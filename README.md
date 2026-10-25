![Supported Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)
[![License](https://img.shields.io/github/license/mikusaa/assfonts)](https://github.com/mikusaa/assfonts/blob/main/LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/mikusaa/assfonts)](https://github.com/mikusaa/assfonts/releases/latest)

# assfonts

中文 | [English](README.en.md)

为 ASS 字幕子集化字体，并把字体内嵌进字幕。

assfonts 会找出 ASS 字幕用到的字体，把它们子集化，再按 ASS 格式的 UUEncode 方式直接嵌入字幕文件。这样一个字幕文件就带齐了渲染所需的全部信息，效果接近 PGS 等图形字幕。

> 本仓库是 [wyzdwdz/assfonts](https://github.com/wyzdwdz/assfonts) 的分支。上游最后一个版本是 2024 年 10 月的 v0.7.3，之后的新功能和发布都在这里进行，详见下文的[与上游的区别](#与上游的区别)。

## 下载

在 [Releases](https://github.com/mikusaa/assfonts/releases/latest) 下载对应平台的包：

| 平台 | 文件 | 架构 |
| --- | --- | --- |
| Windows | `assfonts-<版本>-<架构>-Windows.zip` | x86_64、aarch64、i686 |
| macOS | `assfonts-<版本>-<架构>-macOS.dmg` | x86_64、aarch64（Apple Silicon） |
| Linux | `assfonts-<版本>-<架构>-Linux.tar.gz`（内含 AppImage） | x86_64、aarch64、armhf、i686 |

图形界面程序是 `assfonts`，命令行工具是 `assfonts-cli`。每个文件都附有 `.sha256sum` 校验值。

## 与上游的区别

### 全新的图形界面

- 侧边栏分为「处理字幕」「字体库」「设置」三页；侧边栏可以收起成图标栏，日志面板也可以折叠。
- 浅色和深色主题，默认跟随系统，系统切换外观时实时生效。
- 简体中文界面，默认跟随系统语言，也可以在设置里切换；库输出的日志同样会翻译。
- 字幕列表可以拖入文件或整个文件夹，添加后立即检查字体，每个文件单独显示结果；开始处理的按钮固定在页面顶部。

### 缺字体处理

- 「缺失字体」面板把所有文件缺的字体合并列出，并从字体库里推荐名字相近的字体。
- 可以更新字体库后只重试受影响的文件。
- **从字体包导入**：选一个字体包文件夹，列出它能补上的缺失字体（以及仍缺的字符），把选中的文件复制进字体库。相同内容的文件会直接复用，同名不同内容的会改名保留；导入后增量写入数据库，不必全量重扫。

### 字体库

- 记住字体文件夹，增量更新，可以设置在处理前自动更新；每个文件夹显示新增和移除的文件，支持搜索数据库。
- 数据库记录字体指纹和版本。「重复与名称冲突」报告分出四类：名称冲突（同名同字重的不同字体）、同一字体的不同版本、改名副本、完全相同的副本；可以把多余的文件移出字体库。
- Windows 下读取用户安装的字体（`%LOCALAPPDATA%\Microsoft\Windows\Fonts`），字体文件夹互相重叠时跳过重复文件。
- 修复：带大写字母的字体名每次都会被重复写入数据库。

### 命令行与库

- 新增 `--quiet-non-ttf`，关闭内嵌非 TTF 字体时的警告（图形界面里对应「设置 › 处理方式 › 嵌入 OTF 字体时提示」）。警告文字也改为说明实际风险：基于 libass 的播放器（如 mpv）能正常加载，部分其他播放器可能不行。
- 新增 `AssfontsCheck`、`AssfontsCheckFonts`、`AssfontsAddFonts` 等 API，供图形界面检查缺字体和增量更新数据库。
- 程序改名：图形界面为 `assfonts`，命令行为 `assfonts-cli`，两者可以放在同一个 bin 目录。设置和字体数据库仍在原来的数据目录。

### 构建与发布

- 改用上游 vcpkg 和 Qt 6.11，支持 CMake 4。
- 修复 Linux 各架构的构建；macOS Apple Silicon 改为在 M 系列 runner 上原生构建。
- 向 `main` 提交的 Pull Request 会跑完整的多平台构建。
- 「检查更新」改为查询本仓库。

## 使用

### 图形界面

1. 在「字体库」页添加你的字体文件夹并更新字体库。
2. 在「处理字幕」页拖入字幕文件或文件夹，查看每个文件的字体检查结果；缺字体时可以在面板里补齐。
3. 选择输出位置和选项，开始处理。

设置和字体数据库保存在数据目录中，可以用环境变量 `ASSFONTS_DATA_DIR` 改到别处（例如便携使用）：

| 平台 | 数据目录 |
| --- | --- |
| Windows | `%LOCALAPPDATA%\assfonts` |
| macOS | `~/Library/Application Support/assfonts` |
| Linux | `~/.local/share/assfonts` |

### 命令行

```
Usage:     assfonts-cli [options...] [<files>]
Examples:  assfonts-cli <files>                  Embed subset fonts into ASS script
           assfonts-cli -i <files>               Same as above
           assfonts-cli -o <dir> -s -i <files>   Only subset fonts but not embed
           assfonts-cli -f <dirs> -e -i <files>  Only embed fonts without subset
           assfonts-cli -f <dirs> -b             Build or update fonts database only
           assfonts-cli -l <num> -i <files>      Recolorize the subtitle for HDR contents
Options:
  -i, --input         <files>   Input .ass files
  -o, --output        <dir>     Output directory  (Default: same directory as input)
  -f, --fontpath      <dirs>    Set fonts directories
  -b, --build                   Build or update fonts database  (Require: --fontpath)
  -d, --dbpath        <dir>     Set fonts database path  (Default: current path)
  -s, --subset-only   <bool>    Subset fonts but not embed them into subtitle  (Default: False)
  -e, --embed-only    <bool>    Embed fonts into subtitle but not subset them (Default: False)
  -r, --rename        <bool>    Rename subsetted fonts (Default: False)
  -l, --luminance     <num>     Set subtitle brightness for HDR contents  (Default: 203)
  -m, --multi-thread  <num>     Enable multi thread mode, <num> is the number of threads for processing
                                (Default: <cpu_count> + 1)
  -c, --font-combined <bool>    !!Experimental!! When there are multiple input files, combine the
                                (subsetted) fonts with the same fontname together (Default: False)
      --quiet-non-ttf <bool>    Do not warn about embedding non-TTF (e.g. .otf) fonts (Default: False)
  -v, --verbose       <num>     Set logging level (0 to 3), 0 is off  (Default: 3)
  -h, --help                    Get help info
```

第一次使用前，建议先建立字体数据库：

```
assfonts-cli -f <你的字体文件夹> -b
```

不指定 `-d` 时，数据库 `fonts.json` 保存在上表的数据目录中（与图形界面共用）。

### 关于非 TTF 字体

按照 [ASS 规范](http://moodub.free.fr/video/ass-specs.doc)，只有 TrueType 字体可以嵌入 ASS 字幕。assfonts 会无视这条规定，强制嵌入 .otf 等字体：基于 libass 的播放器（如 mpv）能正常加载，但部分播放器可能加载失败。不需要警告时加上 `--quiet-non-ttf`。

最稳妥的做法是先只子集化（`-s`），把 `<输出目录>/<字幕文件名>_subsetted/` 里的字体用 [otf2ttf](https://github.com/shimarulin/otf2ttf) 等工具转成 .ttf，再只内嵌（`-e`）：

```
assfonts-cli -s -i <ass_file>
otf2ttf <ass_file>_subsetted/*.otf
assfonts-cli -e -f <ass_file>_subsetted/ -i <ass_file>
```

## 构建

依赖通过 vcpkg（git 子模块）安装，需要 CMake ≥ 3.28 和 Ninja。各平台的预设在 `.github/<windows|macos|linux>/CMakeUserPresets.json`，与 CI 使用的完全一致：

```
git clone --recursive https://github.com/mikusaa/assfonts.git
cd assfonts
cp .github/macos/CMakeUserPresets.json .
cmake --preset macos-arm64
cmake --build --preset macos-arm64-release --target install
```

产物位于 `out/install/<预设名>/`。Windows 需在对应架构的 MSVC 开发者命令行中执行；Linux 的发布构建在 `wyzdwdz/assfonts-amd64` 容器里完成，具体步骤见 [build_release.yml](.github/workflows/build_release.yml)。

## 许可

[GPL-3.0-or-later](LICENSE)。原作者 [wyzdwdz](https://github.com/wyzdwdz)，第三方组件见 [NOTICE](NOTICE)。
