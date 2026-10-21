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

#include "font_db.h"

#include <algorithm>
#include <cstdlib>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

QStringList ToStringList(const QJsonValue& value) {
  QStringList list;
  for (const auto& item : value.toArray()) {
    list.append(item.toString());
  }
  return list;
}

// Lower case without spaces, dashes and underscores, so "FZLanTingYuan-DB"
// and "fzlantingyuan db" compare equal.
QString Normalize(const QString& name) {
  QString res = name.toLower();
  static const QRegularExpression kSeparators("[\\s_\\-]+");
  res.remove(kSeparators);
  return res;
}

int CommonPrefix(const QString& a, const QString& b) {
  const int len = std::min(a.size(), b.size());
  int idx = 0;
  while (idx < len && a.at(idx) == b.at(idx)) {
    ++idx;
  }
  return idx;
}

}  // namespace

QString FontDatabase::DbFile(const QString& db_dir) {
  return QDir(db_dir).filePath("fonts.json");
}

QString FontDatabase::PathKey(const QString& path) {
  QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef _WIN32
  key = key.toLower();
#endif
  return key;
}

void FontDatabase::Load(const QString& db_dir) {
  fonts_.clear();
  paths_.clear();
  num_families_ = 0;
  last_updated_ = QDateTime();

  QFile file(DbFile(db_dir));
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  last_updated_ = QFileInfo(file).lastModified();

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  QSet<QString> families;

  for (const auto& value : doc.array()) {
    const QJsonObject obj = value.toObject();

    Font font;
    font.families = ToStringList(obj.value("families"));
    font.fullnames = ToStringList(obj.value("fullnames"));
    font.psnames = ToStringList(obj.value("psnames"));
    font.weight = obj.value("weight").toInt(400);
    font.slant = obj.value("slant").toInt(0);
    font.path = QDir::toNativeSeparators(obj.value("path").toString());

    for (const auto& family : font.families) {
      families.insert(family.toLower());
    }
    paths_.insert(PathKey(font.path));
    fonts_.append(font);
  }

  num_families_ = families.size();
}

QVector<FontDatabase::Font> FontDatabase::Search(const QString& text,
                                                 int limit) const {
  QVector<Font> res;
  const QString needle = Normalize(text);
  if (needle.isEmpty()) {
    return res;
  }

  auto matches = [&](const QStringList& names) {
    return std::any_of(names.begin(), names.end(), [&](const QString& name) {
      return Normalize(name).contains(needle);
    });
  };

  for (const auto& font : fonts_) {
    if (matches(font.families) || matches(font.fullnames) ||
        matches(font.psnames) ||
        Normalize(QFileInfo(font.path).fileName()).contains(needle)) {
      res.append(font);
      if (res.size() >= limit) {
        break;
      }
    }
  }

  return res;
}

QStringList FontDatabase::Similar(const QString& fontname, int limit) const {
  const QString needle = Normalize(fontname);
  if (needle.size() < 2) {
    return {};
  }

  // Score each known name by how much of the wanted name it shares from the
  // start; "方正兰亭圆_GBK" finds "方正兰亭圆_GBK_中".
  QHash<QString, int> scores;
  QHash<QString, QString> display;

  for (const auto& font : fonts_) {
    QStringList names = font.families + font.fullnames;
    for (const auto& name : names) {
      const QString norm = Normalize(name);
      if (norm.isEmpty() || norm == needle) {
        continue;
      }

      int score = CommonPrefix(norm, needle);
      if (norm.contains(needle) || needle.contains(norm)) {
        score = std::max(score, static_cast<int>(std::min(norm.size(),
                                                          needle.size())));
      }

      const int threshold = std::max(2, static_cast<int>(needle.size() * 0.6));
      if (score < threshold) {
        continue;
      }

      // Prefer names close in length to the wanted one.
      score = score * 100 - std::abs(norm.size() - needle.size());

      const QString key = font.families.isEmpty() ? norm : Normalize(font.families.first());
      if (score > scores.value(key, -1)) {
        scores[key] = score;
        display[key] = name;
      }
    }
  }

  QStringList keys = scores.keys();
  std::sort(keys.begin(), keys.end(), [&](const QString& a, const QString& b) {
    return scores.value(a) > scores.value(b);
  });

  QStringList res;
  for (const auto& key : keys) {
    res.append(display.value(key));
    if (res.size() >= limit) {
      break;
    }
  }
  return res;
}

FontDatabase::DirStats FontDatabase::ScanDir(const QString& dir) const {
  DirStats stats;
  static const QStringList kFilters = {"*.ttf", "*.otf", "*.ttc", "*.otc",
                                       "*.TTF", "*.OTF", "*.TTC", "*.OTC"};

  QSet<QString> on_disk;
  QDirIterator iter(dir, kFilters, QDir::Files, QDirIterator::Subdirectories);
  while (iter.hasNext()) {
    const QString key = PathKey(iter.next());
    on_disk.insert(key);
    ++stats.num_files;
    // Files the library can't read never enter the database, so only files
    // that appeared since the last update count as new. Creation time catches
    // copied fonts, which keep their old modification time on Windows.
    if (!paths_.contains(key)) {
      const QFileInfo info = iter.fileInfo();
      QDateTime changed = info.lastModified();
      if (info.birthTime().isValid() && info.birthTime() > changed) {
        changed = info.birthTime();
      }
      if (!last_updated_.isValid() || changed > last_updated_) {
        ++stats.num_new;
      }
    }
  }

  QString prefix = PathKey(dir);
  if (!prefix.endsWith('/')) {
    prefix.append('/');
  }
  for (const auto& path : paths_) {
    if (path.startsWith(prefix) && !on_disk.contains(path)) {
      ++stats.num_removed;
    }
  }

  return stats;
}

QStringList FontDatabase::SystemFontDirs() {
  QStringList dirs;
#ifdef _WIN32
  const QString windir = qEnvironmentVariable("WINDIR", "C:\\Windows");
  dirs << QDir::toNativeSeparators(QDir(windir).filePath("Fonts"));
  const QString local = qEnvironmentVariable("LOCALAPPDATA");
  if (!local.isEmpty()) {
    dirs << QDir::toNativeSeparators(
        QDir(local).filePath("Microsoft/Windows/Fonts"));
  }
#elif __APPLE__
  dirs << "/System/Library/Fonts"
       << "/Library/Fonts"
       << QDir(QDir::homePath()).filePath("Library/Fonts");
#else
  dirs << "/usr/share/fonts"
       << QDir(QDir::homePath()).filePath(".local/share/fonts");
#endif
  return dirs;
}
