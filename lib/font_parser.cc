/*  This file is part of assfonts.
 *
 *  assfonts is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation,
 *  either version 3 of the License,
 *  or (at your option) any later version.
 *
 *  assfonts is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with assfonts. If not, see <https://www.gnu.org/licenses/>.
 *  
 *  written by wyzdwdz (https://github.com/wyzdwdz)
 */

#include "font_parser.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <future>
#include <regex>
#include <sstream>
#include <thread>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

#include FT_MODULE_H
#include FT_TYPE1_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H
#ifdef __cplusplus
}
#endif

#include "ThreadPool.h"
#include <ghc/filesystem.hpp>
#include <nlohmann/json.hpp>

#ifdef __APPLE__
#include "get_home_dir_macos.h"
#elif _WIN32
#include <Shlobj.h>
#else
#endif

#include "ass_freetype.h"

#ifdef _WIN32
constexpr int MAX_TCHAR = 128;
#endif

namespace fs = ghc::filesystem;

static std::vector<AString> DEFAULT_FONT_PATHS = []() {
  std::vector<AString> paths;

#ifdef __APPLE__
  auto path_1 = fs::path("/Library/Fonts");
  if (fs::is_directory(path_1)) {
    paths.emplace_back(path_1.native());
  }

  auto path_2 = fs::path("/Network/Library/Fonts");
  if (fs::is_directory(path_2)) {
    paths.emplace_back(path_2.native());
  }

  auto path_3 = fs::path("/System/Library/Fonts");
  if (fs::is_directory(path_3)) {
    paths.emplace_back(path_3.native());
  }

  auto path_4 = fs::path(GetHomeDir()) / "Library" / "Fonts";
  if (fs::is_directory(path_4)) {
    paths.emplace_back(path_4.native());
  }

#elif __linux__
  auto path_1 = fs::path("/usr/share/fonts");
  if (fs::is_directory(path_1)) {
    paths.emplace_back(path_1.native());
  }

  if (getenv("HOME")) {
    auto path_2 = fs::path(getenv("HOME")) / ".local" / "share" / "fonts";
    if (fs::is_directory(path_2)) {
      paths.emplace_back(path_2.native());
    }
  }

#elif _WIN32
  TCHAR sz_path[MAX_PATH];
  SHGetFolderPath(NULL, CSIDL_FONTS, NULL, 0, sz_path);

  auto path = fs::path(sz_path);
  if (fs::is_directory(path)) {
    paths.emplace_back(path.native());
  }

  // Fonts installed "for the current user only" live here since Windows 10.
  TCHAR sz_local_path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0,
                                sz_local_path))) {
    auto user_path =
        fs::path(sz_local_path) / "Microsoft" / "Windows" / "Fonts";
    if (fs::is_directory(user_path)) {
      paths.emplace_back(user_path.native());
    }
  }

#endif

  return paths;
}();

namespace ass {

void FontParser::LoadFonts(std::vector<AString> fonts_dirs, bool with_default) {
  if (with_default) {
    fonts_dirs.insert(fonts_dirs.end(), DEFAULT_FONT_PATHS.begin(),
                      DEFAULT_FONT_PATHS.end());
  }

  for (const auto& dir : fonts_dirs) {
    auto fonts_path = FindFileInDir(dir, _ST(".+\\.(ttf|otf|ttc|otc)$"));
    fonts_path_.insert(fonts_path_.end(), fonts_path.begin(), fonts_path.end());
    logger_->Info(_ST("Found {} font files in \"{}\". Parsing font files."),
                  fonts_path.size(), dir);
  }

  // Overlapping folders (e.g. a parent and its subfolder) must not add the
  // same font file twice.
  std::sort(fonts_path_.begin(), fonts_path_.end());
  fonts_path_.erase(std::unique(fonts_path_.begin(), fonts_path_.end()),
                    fonts_path_.end());

  font_list_.reserve(fonts_path_.size());

  ThreadPool pool(std::thread::hardware_concurrency() + 1);
  std::vector<std::future<std::unordered_multimap<AString, FontInfo>>> results;

  for (const AString& font_path : fonts_path_) {
    results.emplace_back(
        pool.enqueue([this, font_path]() { return GetFontInfo(font_path); }));
  }

  for (auto&& result : results) {
    auto font_list = result.get();
    font_list_.insert(font_list.begin(), font_list.end());
  }
}

void FontParser::AddFontFiles(const std::vector<AString>& font_files) {
  font_list_ = font_list_in_db_;

  for (const AString& font_path : font_files) {
    font_list_.erase(font_path);
  }

  ThreadPool pool(std::thread::hardware_concurrency() + 1);
  std::vector<std::future<std::unordered_multimap<AString, FontInfo>>> results;

  for (const AString& font_path : font_files) {
    results.emplace_back(
        pool.enqueue([this, font_path]() { return GetFontInfo(font_path); }));
  }

  for (auto&& result : results) {
    auto font_list = result.get();
    font_list_.insert(font_list.begin(), font_list.end());
  }

  logger_->Info(_ST("Added {} font files to the fonts database."),
                font_files.size());
}

void FontParser::SaveDB(const AString& db_path) {
  if (font_list_.size() == 0) {
    logger_->Warn(_ST("No font is found. Nothing to save."));
    return;
  }

  fs::path file_path(db_path);
  if (fs::is_regular_file(file_path)) {
    logger_->Info(_ST("\"{}\" already exists. It will be overwritten."),
                  file_path.native());
  }

  std::ofstream db_file(file_path.native());
  if (!db_file.is_open()) {
    logger_->Warn(_ST("\"{}\" is inaccessible."), file_path.native());
    return;
  }

  nlohmann::ordered_json json;

  for (const auto& font : font_list_) {
    nlohmann::ordered_json js_font;
    js_font["families"] = font.second.families;
    js_font["fullnames"] = font.second.fullnames;
    js_font["psnames"] = font.second.psnames;
    js_font["weight"] = font.second.weight;
    js_font["slant"] = font.second.slant;
#ifdef _WIN32
    js_font["path"] = WideToU8(font.first);
#else
    js_font["path"] = font.first;
#endif
    js_font["index"] = font.second.index;
    js_font["last_write_time"] = font.second.last_write_time;
    js_font["glyph_hash"] = font.second.glyph_hash;
    js_font["face_hash"] = font.second.face_hash;
    js_font["version"] = font.second.version;
    js_font["revision"] = font.second.revision;
    js_font["vendor"] = font.second.vendor;
    js_font["num_glyphs"] = font.second.num_glyphs;

    json.emplace_back(js_font);
  }

  db_file << json.dump(4);

  logger_->Info(_ST("Fonts database has been saved in \"{}\""),
                file_path.native());
}

void FontParser::LoadDB(const AString& db_path) {
  fs::path file_path(db_path);

  std::ifstream db_file(file_path.native());
  if (!db_file.is_open()) {
    logger_->Warn(_ST("Fonts database \"{}\" doesn't exists."),
                  file_path.native());
    return;
  }

  // Older databases may hold the same name more than once in a list.
  auto unique_names = [](const nlohmann::json& js_names) {
    std::vector<std::string> names;
    for (const auto& js_name : js_names) {
      const std::string name = js_name;
      if (std::find(names.begin(), names.end(), name) == names.end()) {
        names.emplace_back(name);
      }
    }
    return names;
  };

  try {
    nlohmann::json json;
    db_file >> json;

    for (const nlohmann::json& js_font : json) {
      std::pair<AString, FontInfo> font;
      font.second.families = unique_names(js_font["families"]);
      font.second.fullnames = unique_names(js_font["fullnames"]);
      font.second.psnames = unique_names(js_font["psnames"]);
      font.second.weight = js_font["weight"];
      font.second.slant = js_font["slant"];
#ifdef _WIN32
      font.first = U8ToWide(js_font["path"]);
#else
      font.first = js_font["path"];
#endif
      font.second.index = js_font["index"];
      font.second.last_write_time = js_font["last_write_time"];
      font.second.is_complete = js_font.contains("glyph_hash");
      font.second.glyph_hash = js_font.value("glyph_hash", "");
      font.second.face_hash = js_font.value("face_hash", "");
      font.second.version = js_font.value("version", "");
      font.second.revision = js_font.value("revision", 0.0);
      font.second.vendor = js_font.value("vendor", "");
      font.second.num_glyphs = js_font.value("num_glyphs", 0L);
      font_list_in_db_.emplace(font);
    }
  } catch (const nlohmann::json::exception&) {
    logger_->Warn(_ST("Cannot load fonts database: \"{}\""),
                  file_path.native());
    font_list_in_db_.clear();
    return;
  }

  logger_->Info(_ST("Load fonts database \"{}\""), file_path.native());
}

void FontParser::clean_font_list() {
  font_list_.clear();
}

std::vector<AString> FontParser::FindFileInDir(const AString& dir,
                                               const AString& pattern) {
  std::vector<AString> res;
  const std::basic_regex<AChar> r(pattern, std::regex::icase);
  const fs::path dir_path(dir);

  try {
    const fs::recursive_directory_iterator iter(dir_path);

    for (const auto& dir_entry : iter) {
      if (std::regex_match(dir_entry.path().native(), r)) {
        res.emplace_back(dir_entry.path().native());
      }
    }

  } catch (const fs::filesystem_error& e) {
    logger_->Warn("Failed in searching font files. Error code: {}", e.what());
    res.clear();
    return res;
  }

  return res;
}

std::unordered_multimap<AString, FontParser::FontInfo> FontParser::GetFontInfo(
    const AString& font_path) {
  std::string last_write_time;
  std::vector<std::unordered_multimap<AString, FontInfo>::iterator> iters_found;

  if (ExistInDB(font_path, last_write_time, iters_found)) {
    return GetFontInfoFromDB(font_path, last_write_time, iters_found);
  }

  std::unordered_multimap<AString, FontInfo> font_list;

  FTLibrary ft_library;
  FT_Init_FreeType(&ft_library.get());

  FT_StreamRec ft_stream = {};
  FT_Open_Args open_args = {};
  NewOpenArgs(font_path, ft_stream, open_args);

  FTFace ft_face;
  if (!OpenFontFace(ft_library.get(), open_args, ft_face.get(), font_path)) {
    return font_list;
  }

  const long n_face = ft_face.get()->num_faces;
  for (long face_idx = 0; face_idx < n_face; ++face_idx) {
    GetFontInfoFromFace(ft_library.get(), ft_face.get(), open_args, face_idx,
                        font_list, font_path, last_write_time);
  }

  if (font_list.empty()) {
    logger_->Warn(_ST("\"{}\" has no parsable name."), font_path);
  }

  return font_list;
}

std::unordered_multimap<AString, FontParser::FontInfo>
FontParser::GetFontInfoFromDB(
    const AString& font_path, const std::string& last_write_time,
    const std::vector<std::unordered_multimap<AString, FontInfo>::iterator>&
        iters_found) {
  std::unordered_multimap<AString, FontInfo> font_list;

  for (const auto& iter : iters_found) {
    font_list.emplace(*iter);
  }

  return font_list;
}

bool FontParser::OpenFontFace(FT_Library& ft_library,
                              const FT_Open_Args& open_args, FT_Face& ft_face,
                              const AString& font_path) {
  if (FT_Open_Face(ft_library, &open_args, -1, &ft_face)) {
    logger_->Warn(_ST("\"{}\" cannot be opened."), font_path);
    return false;
  }
  return true;
}

void FontParser::GetFontInfoFromFace(
    FT_Library& ft_library, FT_Face& ft_face, const FT_Open_Args& open_args,
    const long face_idx, std::unordered_multimap<AString, FontInfo>& font_list,
    const AString& font_path, const std::string& last_write_time) {
  std::pair<AString, FontInfo> font_info;
  std::vector<std::string> families;
  std::vector<std::string> fullnames;
  std::vector<std::string> psnames;
  std::string version;

  if (FT_Open_Face(ft_library, &open_args, face_idx, &ft_face)) {
    return;
  }

  if (FT_Has_PS_Glyph_Names(ft_face)) {
    PS_FontInfo afont_info = nullptr;
    if (!FT_Get_PS_Font_Info(ft_face, afont_info)) {
      families.emplace_back(std::string(afont_info->family_name));
      fullnames.emplace_back(std::string(afont_info->full_name));
    }
  }

  const unsigned int num_names = FT_Get_Sfnt_Name_Count(ft_face);
  for (unsigned int name_idx = 0; name_idx < num_names; ++name_idx) {
    ParseFontName(ft_face, name_idx, families, fullnames, psnames, version);
  }

  if (families.empty() && fullnames.empty() && psnames.empty()) {
    return;
  }

  font_info.second.slant =
      110 * !!(ft_face->style_flags & FT_STYLE_FLAG_ITALIC);

  font_info.second.weight = AssFaceGetWeight(ft_face);

  if (font_info.second.slant < 0 || font_info.second.slant > 110) {
    font_info.second.slant = 0;
  }

  if (font_info.second.weight < 100 || font_info.second.weight > 900) {
    font_info.second.weight = 400;
  }

  font_info.second.families = families;
  font_info.second.fullnames = fullnames;
  font_info.second.psnames = psnames;

  font_info.first = font_path;

  font_info.second.index = face_idx;

  font_info.second.last_write_time = last_write_time;

  font_info.second.version = version;

  const auto* head =
      static_cast<TT_Header*>(FT_Get_Sfnt_Table(ft_face, FT_SFNT_HEAD));
  if (head != nullptr) {
    font_info.second.revision = head->Font_Revision / 65536.0;
  }

  const auto* os2 =
      static_cast<TT_OS2*>(FT_Get_Sfnt_Table(ft_face, FT_SFNT_OS2));
  if (os2 != nullptr) {
    std::string vendor(reinterpret_cast<const char*>(os2->achVendID), 4);
    vendor.erase(vendor.find_last_not_of(std::string(" \0", 2)) + 1);
    font_info.second.vendor = vendor;
  }

  font_info.second.num_glyphs = ft_face->num_glyphs;

  HashTableDirectory(font_path, face_idx, font_info.second.glyph_hash,
                     font_info.second.face_hash);

  font_list.emplace(font_info);
}

void FontParser::ParseFontName(const FT_Face& ft_face,
                               const unsigned int name_idx,
                               std::vector<std::string>& families,
                               std::vector<std::string>& fullnames,
                               std::vector<std::string>& psnames,
                               std::string& version) {
  FT_SfntName name;
  std::string wbuf;
  std::string buf;

  if (FT_Get_Sfnt_Name(ft_face, name_idx, &name)) {
    return;
  }

  if (name.name_id != TT_NAME_ID_FULL_NAME &&
      name.name_id != TT_NAME_ID_FONT_FAMILY &&
      name.name_id != TT_NAME_ID_PS_NAME &&
      name.name_id != TT_NAME_ID_VERSION_STRING) {
    return;
  }

  if (name.platform_id != TT_PLATFORM_MICROSOFT) {
    return;
  }

  wbuf = std::string(reinterpret_cast<char*>(name.string), name.string_len);

  switch (name.encoding_id) {
    case TT_MS_ID_PRC: {
      std::string wbufn = wbuf;
      for (auto it = wbufn.begin(); it != wbufn.end(); ++it) {
        if (*it == '\0') {
          it = wbufn.erase(it);
        }
      }
      if (!IconvConvert(wbufn, buf, "GB2312", "UTF-8")) {
        if (!IconvConvert(wbuf, buf, "UTF-16BE", "UTF-8")) {
          return;
        }
      }
      break;
    }

    case TT_MS_ID_BIG_5: {
      std::string wbufn = wbuf;
      for (auto it = wbufn.begin(); it != wbufn.end(); ++it) {
        if (*it == '\0') {
          it = wbufn.erase(it);
        }
      }
      if (!IconvConvert(wbufn, buf, "BIG-5", "UTF-8")) {
        if (!IconvConvert(wbuf, buf, "UTF-16BE", "UTF-8")) {
          return;
        }
      }
      break;
    }

    default:
      if (!IconvConvert(wbuf, buf, "UTF-16BE", "UTF-8")) {
        return;
      }
      break;
  }

  size_t last_none_zero = buf.find_last_not_of('\0');
  if (last_none_zero != std::string::npos) {
    buf.erase(last_none_zero + 1);
  }

  if (name.name_id == TT_NAME_ID_VERSION_STRING) {
    if (version.empty()) {
      version = buf;
    }
    return;
  }

  // Names are stored lowercased, so they must be compared lowercased too, or
  // every name record with capitals is added once more.
  const std::string lower = ToLower(buf);
  if (lower.empty() || lower == "undefined") {
    return;
  }

  auto add_name = [&lower](std::vector<std::string>& names) {
    if (std::find(names.begin(), names.end(), lower) == names.end()) {
      names.emplace_back(lower);
    }
  };

  switch (name.name_id) {
    case TT_NAME_ID_FONT_FAMILY:
      add_name(families);
      break;

    case TT_NAME_ID_FULL_NAME:
      add_name(fullnames);
      break;

    case TT_NAME_ID_PS_NAME:
      add_name(psnames);
      break;

    default:
      break;
  }
}

bool FontParser::HashTableDirectory(const AString& font_path,
                                    const long face_idx,
                                    std::string& glyph_hash,
                                    std::string& face_hash) {
  // Only the table directory is read: each record carries the table's
  // checksum and length, which tell tables apart without reading them.
  std::ifstream file(font_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  auto read_u32 = [&file](uint32_t& value) {
    unsigned char bytes[4];
    if (!file.read(reinterpret_cast<char*>(bytes), 4)) {
      return false;
    }
    value = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
            (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
    return true;
  };

  uint32_t tag = 0;
  uint32_t offset = 0;
  if (!read_u32(tag)) {
    return false;
  }
  if (tag == 0x74746366) {  // 'ttcf'
    uint32_t num_fonts = 0;
    file.seekg(8);
    if (!read_u32(num_fonts) || face_idx >= static_cast<long>(num_fonts)) {
      return false;
    }
    file.seekg(12 + 4 * face_idx);
    if (!read_u32(offset)) {
      return false;
    }
  } else if (face_idx != 0) {
    return false;
  }

  uint32_t num_tables = 0;
  file.seekg(offset + 4);
  if (!read_u32(num_tables)) {
    return false;
  }
  num_tables >>= 16;

  struct Record {
    uint32_t tag;
    uint32_t checksum;
    uint32_t length;
  };
  std::vector<Record> records;
  for (uint32_t idx = 0; idx < num_tables; ++idx) {
    Record record = {};
    uint32_t table_offset = 0;
    file.seekg(offset + 12 + 16 * idx);
    if (!read_u32(record.tag) || !read_u32(record.checksum) ||
        !read_u32(table_offset) || !read_u32(record.length)) {
      return false;
    }
    records.emplace_back(record);
  }
  std::sort(records.begin(), records.end(),
            [](const Record& a, const Record& b) { return a.tag < b.tag; });

  // Outlines, bitmaps, character map and advance widths. Renaming a font
  // leaves these alone.
  static const std::vector<std::string> kGlyphTables = {
      "glyf", "CFF ", "CFF2", "EBDT", "CBDT", "sbix", "SVG ", "cmap", "hmtx"};

  auto fnv = [](uint64_t hash, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
      hash ^= (value >> shift) & 0xff;
      hash *= 0x100000001b3ULL;
    }
    return hash;
  };

  uint64_t glyph = 0xcbf29ce484222325ULL;
  uint64_t face = 0xcbf29ce484222325ULL;
  for (const auto& record : records) {
    const std::string name = {char(record.tag >> 24), char(record.tag >> 16),
                              char(record.tag >> 8), char(record.tag)};
    // A signature changes with any edit, so it says nothing about content.
    if (name != "DSIG") {
      face = fnv(fnv(fnv(face, record.tag), record.checksum), record.length);
    }
    if (std::find(kGlyphTables.begin(), kGlyphTables.end(), name) !=
        kGlyphTables.end()) {
      glyph = fnv(fnv(fnv(glyph, record.tag), record.checksum), record.length);
    }
  }

  auto to_hex = [](uint64_t value) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx",
             static_cast<unsigned long long>(value));
    return std::string(buf);
  };

  glyph_hash = to_hex(glyph);
  face_hash = to_hex(face);
  return true;
}

int FontParser::AssFaceGetWeight(const FT_Face& face) {
  /*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

  TT_OS2* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
  FT_UShort os2Weight = os2 ? os2->usWeightClass : 0;
  switch (os2Weight) {
    case 0:
      return 300 * !!(face->style_flags & FT_STYLE_FLAG_BOLD) + 400;
    case 1:
      return 100;
    case 2:
      return 200;
    case 3:
      return 300;
    case 4:
      return 350;
    case 5:
      return 400;
    case 6:
      return 600;
    case 7:
      return 700;
    case 8:
      return 800;
    case 9:
      return 900;
    default:
      return os2Weight;
  }
}

bool FontParser::ExistInDB(
    const AString& font_path, std::string& last_write_time,
    std::vector<std::unordered_multimap<AString, FontInfo>::iterator>&
        iters_found) {
  last_write_time = GetLastWriteTime(font_path);

  if (last_write_time.empty()) {
    return false;
  }

  auto iter_pair = font_list_in_db_.equal_range(font_path);
  for (auto iter = iter_pair.first; iter != iter_pair.second; ++iter) {
    iters_found.emplace_back(iter);
  }

  if (iters_found.empty()) {
    return false;
  }

  if (iters_found[0]->second.last_write_time == last_write_time &&
      iters_found[0]->second.is_complete) {
    return true;
  } else {
    return false;
  }
}

std::string FontParser::GetLastWriteTime(const AString& font_path) {
#ifdef _WIN32
  FILETIME file_time;
  SYSTEMTIME system_time;
  HANDLE file_handle =
      CreateFileW(font_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (file_handle == INVALID_HANDLE_VALUE) {
    return std::string();
  }

  TCHAR date[MAX_TCHAR], time[MAX_TCHAR];

  if (!GetFileTime(file_handle, NULL, NULL, &file_time)) {
    CloseHandle(file_handle);
    return std::string();
  }

  if (!FileTimeToSystemTime(&file_time, &system_time)) {
    CloseHandle(file_handle);
    return std::string();
  }

  if (!GetDateFormat(LOCALE_SYSTEM_DEFAULT, NULL, &system_time,
                     "yyyy'-'MM'-'dd", date, MAX_TCHAR)) {
    CloseHandle(file_handle);
    return std::string();
  }

  if (!GetTimeFormat(LOCALE_SYSTEM_DEFAULT, NULL, &system_time, "HH':'mm':'ss",
                     time, MAX_TCHAR)) {
    CloseHandle(file_handle);
    return std::string();
  }

  CloseHandle(file_handle);
  return "UTC " + std::string(date) + " " + std::string(time);
#else
  auto buffer = std::unique_ptr<struct stat>(new struct stat);

  if (stat(font_path.c_str(), buffer.get())) {
    return std::string();
  }

  std::stringstream ss;

  auto gmt_time = std::gmtime(&buffer->st_mtime);
  if (gmt_time == nullptr) {
    return std::string();
  }

  try {
    ss << std::put_time(gmt_time, "UTC %Y-%m-%d %H:%M:%S");
  } catch (const std::exception&) {
    return std::string();
  }

  return ss.str();
#endif
}

}  // namespace ass