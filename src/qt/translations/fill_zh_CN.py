#!/usr/bin/env python3
"""Fills assfonts-gui_zh_CN.ts with the Simplified Chinese strings below.

Usage (from the repository root, with Qt's linguist tools on PATH):
    lupdate src/qt -ts src/qt/translations/assfonts-gui_zh_CN.ts
    python3 src/qt/translations/fill_zh_CN.py
    lrelease src/qt/translations/assfonts-gui_zh_CN.ts \
        -qm src/qt/resources/i18n/assfonts-gui_zh_CN.qm

The .qm file is committed, so building the app does not need Qt's linguist
tools.
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ZH_CN = {
    # Common
    ", ": "、",
    "OK": "好",
    "Bold": "粗体",
    "Light": "细体",
    "Regular": "常规",
    " · Italic": " · 斜体",
    "just now": "刚刚",
    "%1 min ago": "%1 分钟前",
    "%1 h ago": "%1 小时前",
    "Never": "从未",
    "font": "字体",
    # Main window
    "Subtitle font embedding": "字幕字体子集化与内嵌",
    "Process subtitles": "处理字幕",
    "Font library": "字体库",
    "Settings": "设置",
    "Check for updates": "检查更新",
    "Processing %n subtitle(s)": "处理 %n 个字幕",
    "Rebuilding font library": "完全重建字体库",
    "Updating font library": "更新字体库",
    "Still working": "正在处理",
    "Please wait until the current task finishes.": "请等当前任务完成后再操作。",
    # Worker
    "Updating fonts database…": "正在更新字体库…",
    "Checking fonts…": "正在检查字体…",
    "Processing subtitles…": "正在处理字幕…",
    # Process page
    "Drop subtitles or whole folders here. The fonts they use are subset and "
    "embedded automatically.":
        "拖入字幕文件或整个文件夹，自动找出所需字体、子集化后嵌入字幕。",
    "Clear list": "清空列表",
    "Subtitle files": "字幕文件",
    "Add files": "添加文件",
    "Add folder": "添加文件夹",
    "File": "文件名",
    "Folder": "所在文件夹",
    "Fonts": "字体数",
    "Status": "状态",
    "Choose subtitle files": "选择字幕文件",
    "ASS subtitles (*.ass *.ssa)": "ASS 字幕 (*.ass *.ssa)",
    "Choose a folder with subtitles": "选择字幕所在文件夹",
    "Missing fonts": "缺失字体",
    "Copy all names": "复制全部名称",
    "Put the fonts into any font folder of the library, then update the "
    "library and process the affected files again.":
        "把字体放进字体库的任一目录，然后更新字体库并重新处理这些文件。",
    "Options": "处理选项",
    "Output location": "输出位置",
    "Next to subtitle": "与字幕相同",
    "Choose folder": "指定文件夹",
    "Output folder": "输出文件夹",
    "Browse…": "浏览…",
    "Each result is written to the folder of its subtitle.":
        "结果输出到各字幕所在的文件夹。",
    "Processing": "处理方式",
    "Subset and embed": "子集化并嵌入",
    "Subset only": "仅子集化",
    "Embed only": "仅嵌入",
    "Subset only writes the font files; embed only puts whole fonts into the "
    "subtitle.":
        "仅子集化：只输出子集字体文件；仅嵌入：把完整字体嵌入字幕。",
    "HDR brightness": "HDR 亮度调整",
    "Off": "关闭",
    "Low · 100 nit": "低 · 100 nit",
    "High · 203 nit": "高 · 203 nit",
    "Lowers subtitle brightness for HDR video.": "为 HDR 视频降低字幕亮度。",
    "Rename subset fonts": "子集字体重命名",
    "Gives every subtitle its own set of fonts so players never mix them up.":
        "保证每个字幕对应一套独立的子集字体，避免播放器串字。",
    "Combine shared fonts (experimental)": "合并同名子集字体（实验性）",
    "Subtitles using the same font share one subset.":
        "多个字幕用到同一字体时，合成一份子集。",
    "Choose output folder": "选择输出文件夹",
    "Update library before processing": "处理前自动更新字体库",
    "Manage library": "管理字体库",
    "Open output folder": "打开输出文件夹",
    "Check fonts only": "只检查字体",
    "Start": "开始处理",
    "Stop": "停止",
    "Stopping…": "正在停止…",
    "Font library · %1 folders · %2 fonts": "字体库 · %1 个目录 · %2 个字体",
    "No library yet. Add your font folders in Font library.":
        "还没有字体库，请先在「字体库」里添加字体目录。",
    "Updated %1": "%1更新",
    "Not checked": "未检查",
    "Checking…": "检查中…",
    "All fonts found": "字体齐全",
    "%n font(s) missing": "缺 %n 个字体",
    "Cannot read": "无法读取",
    "Processing…": "处理中…",
    "Done": "完成",
    "%n font(s) missing, not written": "缺 %n 个字体，未输出",
    "Failed": "失败",
    "Skipped": "已跳过",
    "Remove": "移除",
    "Remove %1": "移除 %1",
    "Drop .ass / .ssa files or whole folders here\n"
    "A whole season can be processed at once":
        "把 .ass / .ssa 文件或整个文件夹拖到这里\n一次可以处理一整季",
    "Drop more files or folders here": "继续拖入文件或文件夹",
    "Used in %1 · %n character(s)": "用于 %1 · 共 %n 个字",
    "Similar fonts in your library: %1": "字体库里有相近的字体：%1",
    "Copy name": "复制名称",
    "Search the web": "在网上搜索",
    "Find in library": "在字体库里查找",
    "Missing characters": "缺少字符",
    "These fonts were found but lack some characters. Players may show boxes "
    "for them; processing still works.":
        "这些字体找到了，但缺少部分字符，播放时可能显示为方框，不影响输出。",
    "… %1 more": "… 另有 %1 个",
    "Update library and retry %n file(s)": "更新字体库并重试 %n 个文件",
    "Add subtitles to get started.": "添加字幕后即可开始。",
    "Processing %1 of %2…": "正在处理第 %1 / %2 个…",
    "Finished: %1 succeeded · %2 missing fonts · %3 failed":
        "处理完成：成功 %1 · 缺字体 %2 · 失败 %3",
    "%n file(s)": "%n 个文件",
    "%n file(s) are missing fonts; add them first or process the rest":
        "%n 个文件缺字体，可先补齐再处理",
    "Choose an output folder first.": "请先选择输出文件夹。",
    "%1, %2 and %n more": "%1、%2 等 %n 个",
    # Library page
    "Add your font folders once. Updates only read new or changed font files, "
    "so they take seconds.":
        "目录只需添加一次。之后更新只读取新增或改动过的字体文件，几秒就好。",
    "Rebuild from scratch": "完全重建",
    "Forget everything and read all font files again":
        "清空字体库，重新读取所有字体文件",
    "Update library": "更新字体库",
    "Font folders": "字体目录",
    "Font files": "字体文件",
    "Font names": "字体名称",
    "Last updated": "上次更新",
    "System fonts": "系统字体",
    "Always included": "始终包含",
    "Drop font folders here to add them": "把字体文件夹拖到这里即可添加",
    "Update before processing subtitles": "处理字幕前自动更新",
    "Look for changes on startup": "启动时检查目录变化",
    "Choose a font folder": "选择字体文件夹",
    "Find a font": "查找字体",
    "Font name, PostScript name or file name": "按字体名、PostScript 名或文件名搜索",
    "Font name": "字体名称",
    "Weight / style": "字重 / 样式",
    "PostScript name": "PostScript 名",
    "The library is saved in %1": "字体库数据保存在 %1",
    "Counting font files…": "正在统计字体文件…",
    "Scanning…": "扫描中…",
    "Remove from library": "从字体库移除",
    "Remove %1 from library": "从字体库移除 %1",
    "The folder can't be found": "找不到这个文件夹",
    "Not found": "找不到",
    "%n font file(s), including subfolders": "%n 个字体文件 · 含子文件夹",
    "%n new file(s)": "发现 %n 个新文件",
    "%n file(s) removed": "%n 个文件已删除",
    "Up to date": "已是最新",
    "No font folders yet. Add the folders where you keep fonts for "
    "subtitles.":
        "还没有字体目录。添加你存放字幕字体的文件夹吧。",
    "First %1 matches": "仅显示前 %1 个",
    "%n match(es)": "找到 %n 个",
    # Settings page
    "Changes are saved automatically.": "所有设置自动保存。",
    "Follow system": "跟随系统",
    "Language": "界面语言",
    "Appearance": "外观",
    "Dark": "深色",
    "General": "通用",
    "The window reloads in the new language.": "切换后窗口会以新语言重新载入。",
    "Process several files at once": "多线程处理",
    "Faster, but uses much more memory.": "同时处理多个字幕，速度更快，但内存占用会明显升高。",
    "Check fonts when files are added": "添加字幕后自动预检",
    "Shows missing fonts right away instead of after processing.":
        "拖入文件后立刻检查缺不缺字体，不必等到处理完。",
    "Open the output folder when done": "完成后打开输出文件夹",
    "Only when every file succeeded.": "全部成功时才打开。",
    "Log": "日志",
    "Clear the log before each run": "每次运行前自动清空",
    "Show the log when there are warnings or errors": "有警告或错误时自动展开日志",
    "Otherwise the log stays folded away.": "平时保持收起，界面更清爽。",
    "Show in Explorer": "在资源管理器中显示",
    "Show in Finder": "在访达中显示",
    "Open folder": "打开文件夹",
    "Change…": "更改…",
    "Library data location": "字体库数据位置",
    "Reset…": "恢复默认…",
    "Data": "数据",
    "Reset settings": "恢复默认设置",
    "Your font library and its folders are kept.": "不会删除字体库和已添加的目录。",
    "Version %1.%2.%3": "版本 %1.%2.%3",
    "Choose where to keep the font library": "选择字体库数据的保存位置",
    "Reset all settings to their defaults? Your font library and its folders "
    "are kept.":
        "确定把所有设置恢复为默认吗？字体库和已添加的目录会保留。",
    # Log panel
    "All": "全部",
    "Warnings": "警告",
    "Errors": "错误",
    "Filter by level": "按级别筛选",
    "Clear before each run": "每次运行前自动清空",
    "Copy log": "复制日志",
    "Save log to file": "导出日志",
    "Clear log": "清空日志",
    "Collapse": "收起",
    "Expand": "展开",
    "Save log": "导出日志",
    "Log files (*.log *.txt)": "日志文件 (*.log *.txt)",
    "INFO": "信息",
    "WARN": "警告",
    "ERROR": "错误",
    "All %1": "全部 %1",
    "Warnings %1": "警告 %1",
    "Errors %1": "错误 %1",
    "%1 warnings · %2 errors": "警告 %1 · 错误 %2",
    # Library log messages
    "Found %1 font files in \"%2\", reading them":
        "在「%2」中找到 %1 个字体文件，正在读取",
    "Loaded fonts database \"%1\"": "已载入字体库「%1」",
    "Fonts database \"%1\" doesn't exist yet": "字体库「%1」还不存在",
    "Cannot read fonts database \"%1\"": "无法读取字体库「%1」",
    "Fonts database saved to \"%1\"": "字体库已保存到「%1」",
    "\"%1\" already exists and will be overwritten": "「%1」已存在，将被覆盖",
    "No font found, nothing to save": "没有找到字体，未保存",
    "Reading \"%1\"": "读取「%1」",
    "Detected file encoding: %1": "检测到文件编码：%1",
    "Cannot convert the file to UTF-8": "无法把文件转换为 UTF-8",
    "Cannot parse \"%1\": format error": "无法解析「%1」：格式错误",
    "Cannot parse \"%1\": wrong number of fields": "无法解析「%1」：字段数量不对",
    "Cannot parse \"%1\": no [Events] section": "无法解析「%1」：缺少 [Events] 段",
    "Cannot parse \"%1\": no [V4+ Styles] section":
        "无法解析「%1」：缺少 [V4+ Styles] 段",
    "Style \"%1\" is not defined (line %2)": "样式「%1」未定义（第 %2 行）",
    "Missing font \"%1\" (%2)": "找不到字体「%1」（%2）",
    "Found font \"%1\" (%2) → %3": "找到字体「%1」（%2）→ %3",
    "\"%1\" (%2) has no glyph for: %3": "「%1」（%2）缺少这些字符：%3",
    "Some fonts are missing, see the warnings above": "有字体缺失，详见上方警告",
    "Created folder for subset fonts \"%1\"": "已创建子集字体文件夹「%1」",
    "Cannot create folder for subset fonts: %1": "无法创建子集字体文件夹：%1",
    "Failed to subset \"%1\"": "子集化失败：「%1」",
    "Wrote subtitle with embedded fonts \"%1\"": "已输出内嵌字体的字幕「%1」",
    "Wrote subtitle with renamed fonts \"%1\"": "已输出字体重命名的字幕「%1」",
    "Cannot write file %1": "无法写入文件 %1",
    "HDR brightness adjustment failed: %1": "HDR 亮度调整失败：%1",
    "Wrote HDR-adjusted subtitle \"%1\"": "已输出 HDR 亮度调整后的字幕「%1」",
    "Cannot create \"%1\"": "无法创建「%1」",
    "Cannot open \"%1\"": "无法打开「%1」",
    "Cannot read the font name of \"%1\"": "无法读取「%1」的字体名",
    "Cannot access \"%1\"": "无法访问「%1」",
    "\"%1\" is not a TrueType font": "「%1」不是 TrueType 字体",
    "Failed to search for font files: %1": "搜索字体文件失败：%1",
    "No fonts database folder set": "未设置字体库位置",
    "No font folder added": "还没有添加字体目录",
    "No subtitle file added": "还没有添加字幕文件",
    "No output folder set": "未设置输出文件夹",
    # Update window
    "Check update": "检查更新",
    "Current version: ": "当前版本：",
    "Latest version: ": "最新版本：",
    "Fetching ...": "获取中…",
}


# Same English text, different meaning: keyed by (source, disambiguation).
ZH_CN_CONTEXT = {
    ("Light", "appearance"): "浅色",
    ("Dark", "appearance"): "深色",
    ("Processing", "settings section"): "处理",
}


def main() -> int:
    ts_path = Path(__file__).with_name("assfonts-gui_zh_CN.ts")
    tree = ET.parse(ts_path)
    root = tree.getroot()
    root.set("language", "zh_CN")

    missing = []
    for message in root.iter("message"):
        source = message.findtext("source")
        translation = message.find("translation")
        if translation is None:
            translation = ET.SubElement(message, "translation")

        comment = message.findtext("comment")
        text = ZH_CN_CONTEXT.get((source, comment)) if comment else None
        if text is None:
            text = ZH_CN.get(source)
        for child in list(translation):
            translation.remove(child)
        translation.text = None
        translation.attrib.pop("type", None)

        if text is None:
            missing.append(source)
            translation.set("type", "unfinished")
            continue

        if message.get("numerus") == "yes":
            form = ET.SubElement(translation, "numerusform")
            form.text = text
        else:
            translation.text = text

    tree.write(ts_path, encoding="utf-8", xml_declaration=True)

    for source in missing:
        print(f"untranslated: {source!r}", file=sys.stderr)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
