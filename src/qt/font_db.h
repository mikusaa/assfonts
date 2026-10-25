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

#pragma once

#include <QDateTime>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// Read-only view of the fonts.json database written by the library, used for
// statistics, searching and suggesting similar fonts.
class FontDatabase {
 public:
  struct Font {
    QStringList families;
    QStringList fullnames;
    QStringList psnames;
    int weight = 400;
    int slant = 0;
    QString path;
    int index = 0;
    // Empty for fonts read by a version without duplicate checking.
    QString glyph_hash;
    QString face_hash;
    QString version;
    double revision = 0;
    QString vendor;
    int num_glyphs = 0;
  };

  struct DirStats {
    int num_files = 0;
    int num_new = 0;
    int num_removed = 0;
  };

  void Load(const QString& db_dir);

  bool IsEmpty() const { return fonts_.isEmpty(); }
  int FileCount() const { return paths_.size(); }
  int FamilyCount() const { return num_families_; }
  QDateTime LastUpdated() const { return last_updated_; }
  const QVector<Font>& Fonts() const { return fonts_; }

  QVector<Font> Search(const QString& text, int limit) const;

  // Family names that look like the given one, best match first.
  QStringList Similar(const QString& fontname, int limit) const;

  // Compares a directory on disk with what the database holds for it.
  // Safe to call from another thread.
  DirStats ScanDir(const QString& dir) const;

  static QStringList SystemFontDirs();

  static QString DbFile(const QString& db_dir);

 private:
  QVector<Font> fonts_;
  QSet<QString> paths_;
  int num_families_ = 0;
  QDateTime last_updated_;

  static QString PathKey(const QString& path);
};
