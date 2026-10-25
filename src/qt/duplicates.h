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

#include <QString>
#include <QStringList>
#include <QVector>

#include "font_db.h"

// Finds fonts the library holds more than once, and names that lead to more
// than one different font.
namespace duplicates {

enum class Kind {
  // One name, several fonts that draw differently. Which one a subtitle
  // gets depends on matching order.
  kConflict,
  // Like a conflict, but the fonts are releases of the same font.
  kVersions,
  // The same glyphs under other names or other metadata.
  kRepackaged,
  // Byte-for-byte the same font in several files.
  kIdentical
};

struct Group {
  Kind kind = Kind::kIdentical;
  QString title;
  // Names the fonts share, for conflicts and versions.
  QStringList shared_names;
  // Indexes into FontDatabase::Fonts().
  QVector<int> fonts;
  // Index into `fonts` of the one worth keeping, or -1 without advice.
  int keep = -1;
  // Indexes into `fonts` that can go without losing anything.
  QVector<int> removable;
};

struct Report {
  QVector<Group> groups;
  // Fonts read before fingerprints existed; an update reads them again.
  int num_unchecked = 0;
};

Report Find(const QVector<FontDatabase::Font>& fonts,
            const QStringList& system_dirs);

// Moves the files into `target_dir`, renaming a file whose name is taken
// there. Returns the files that could not be moved; they stay where they are.
QStringList MoveFiles(const QStringList& sources, const QString& target_dir);

// True when the path lies in one of the folders.
bool IsInDirs(const QString& path, const QStringList& dirs);

}  // namespace duplicates
