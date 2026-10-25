![Supported Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue.svg)
[![License](https://img.shields.io/github/license/mikusaa/assfonts)](https://github.com/mikusaa/assfonts/blob/main/LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/mikusaa/assfonts)](https://github.com/mikusaa/assfonts/releases/latest)

# assfonts

[中文](README.md) | English

Subset fonts and embed them into an ASS subtitle.

assfonts finds the fonts an ASS subtitle needs, subsets them and embeds them UUEncoded into the subtitle, as the ASS format allows. One subtitle file then carries everything needed to render it, much like graphical subtitles such as PGS.

> This is a fork of [wyzdwdz/assfonts](https://github.com/wyzdwdz/assfonts). Upstream's last release was v0.7.3 in October 2024; new features and releases happen here. See [Changes from upstream](#changes-from-upstream).

## Download

Get the package for your platform from [Releases](https://github.com/mikusaa/assfonts/releases/latest):

| Platform | File | Architectures |
| --- | --- | --- |
| Windows | `assfonts-<version>-<arch>-Windows.zip` | x86_64, aarch64, i686 |
| macOS | `assfonts-<version>-<arch>-macOS.dmg` | x86_64, aarch64 (Apple Silicon) |
| Linux | `assfonts-<version>-<arch>-Linux.tar.gz` (with AppImage) | x86_64, aarch64, armhf, i686 |

The graphical app is `assfonts`; the command line tool is `assfonts-cli`. Every file comes with a `.sha256sum`.

## Changes from upstream

### New GUI

- Sidebar with Process subtitles / Font library / Settings pages; the sidebar collapses to an icon strip and the log panel folds away.
- Light and dark themes, following the system by default and switching live.
- Simplified Chinese translation, following the system language or chosen in Settings; library log messages are translated too.
- Drop files or whole folders onto the subtitle list; fonts are checked right away with a result per file. The run buttons stay at the top of the page.

### Missing fonts

- The missing-fonts panel lists what all files lack in one place and suggests similar fonts from the library.
- Update the library and retry only the affected files.
- **Import from font pack**: pick a folder, see which missing fonts it provides (and which characters are still missing), and copy the picked files into the library. Identical files are reused, a different file with the same name is kept under a new name, and the database is updated without a full rescan.

### Font library

- Remembers font folders, updates incrementally and can update before processing; shows new and removed files per folder and lets you search the database.
- Records font fingerprints and versions. The "Duplicates and name conflicts" report separates name conflicts (different fonts with one name at the same weight and slant), versions of one font, renamed copies and identical copies, and moves the extra files out of the library.
- Reads per-user fonts on Windows (`%LOCALAPPDATA%\Microsoft\Windows\Fonts`) and skips duplicate files when font folders overlap.
- Fix: font names with capitals were written to the database again on every update.

### CLI and library

- New `--quiet-non-ttf` to silence the warnings for embedded non-TTF fonts (Settings › Processing › "Warn when embedding OTF fonts" in the GUI). The warnings now explain the actual risk: libass-based players such as mpv load them fine, some other players may not.
- New `AssfontsCheck`, `AssfontsCheckFonts` and `AssfontsAddFonts` APIs, which the GUI uses to find missing fonts and update the database incrementally.
- Renamed binaries: the GUI is `assfonts` and the CLI `assfonts-cli`, so both fit in one bin folder. Settings and the fonts database stay in the same data folder.

### Build and release

- Upstream vcpkg with Qt 6.11; builds with CMake 4.
- Fixed Linux builds on every architecture; Apple Silicon macOS builds natively on an M-series runner.
- Pull requests to `main` run the full multi-platform build.
- "Check for updates" looks at this repository.

## Usage

### GUI

1. On the Font library page, add your font folders and update the library.
2. On the Process subtitles page, drop in subtitle files or folders and look at each file's font check; fill in missing fonts from the panel.
3. Pick the output location and options, then start.

Settings and the fonts database live in the data folder; set `ASSFONTS_DATA_DIR` to keep them elsewhere (e.g. for a portable setup):

| Platform | Data folder |
| --- | --- |
| Windows | `%LOCALAPPDATA%\assfonts` |
| macOS | `~/Library/Application Support/assfonts` |
| Linux | `~/.local/share/assfonts` |

### CLI

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

Before first use, build the fonts database:

```
assfonts-cli -f <your_fonts_dir> -b
```

Without `-d`, the database `fonts.json` is kept in the data folder above, shared with the GUI.

### Non-TTF fonts

Per the [ASS specs](http://moodub.free.fr/video/ass-specs.doc), only TrueType fonts can be embedded in an ASS script. assfonts embeds .otf and other fonts anyway: libass-based players such as mpv load them fine, but some players may not. Pass `--quiet-non-ttf` to skip the warnings.

The safe route is to subset only (`-s`), convert the fonts in `<output_dir>/<subtitle_name>_subsetted/` to .ttf with a tool such as [otf2ttf](https://github.com/shimarulin/otf2ttf), then embed only (`-e`):

```
assfonts-cli -s -i <ass_file>
otf2ttf <ass_file>_subsetted/*.otf
assfonts-cli -e -f <ass_file>_subsetted/ -i <ass_file>
```

## Building

Dependencies come from vcpkg (a git submodule); you need CMake ≥ 3.28 and Ninja. The presets in `.github/<windows|macos|linux>/CMakeUserPresets.json` are the ones CI uses:

```
git clone --recursive https://github.com/mikusaa/assfonts.git
cd assfonts
cp .github/macos/CMakeUserPresets.json .
cmake --preset macos-arm64
cmake --build --preset macos-arm64-release --target install
```

The output lands in `out/install/<preset>/`. On Windows, run this from an MSVC developer prompt for the target architecture; Linux release builds run in the `wyzdwdz/assfonts-amd64` container, see [build_release.yml](.github/workflows/build_release.yml).

## License

[GPL-3.0-or-later](LICENSE). Originally written by [wyzdwdz](https://github.com/wyzdwdz); third-party components are listed in [NOTICE](NOTICE).
