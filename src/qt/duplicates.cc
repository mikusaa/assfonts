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

#include "duplicates.h"

#include <algorithm>
#include <numeric>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

namespace duplicates {

namespace {

QString PathKey(const QString& path) {
  QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef _WIN32
  key = key.toLower();
#endif
  return key;
}

QString Title(const FontDatabase::Font& font) {
  if (!font.fullnames.isEmpty()) {
    return font.fullnames.first();
  }
  if (!font.families.isEmpty()) {
    return font.families.first();
  }
  return QFileInfo(font.path).fileName();
}

// Index of the copy worth keeping: not in a system folder, and without the
// "_0" Windows adds when the same font is installed twice.
int BestCopy(const QVector<FontDatabase::Font>& fonts,
             const QVector<int>& members, const QStringList& system_dirs) {
  static const QRegularExpression kInstallSuffix("_\\d+$");
  int best = -1;
  auto rank = [&](int idx) {
    const QFileInfo info(fonts.at(members.at(idx)).path);
    return std::make_tuple(kInstallSuffix.match(info.completeBaseName())
                               .hasMatch(),
                           info.fileName().size(), info.filePath());
  };
  for (int idx = 0; idx < members.size(); ++idx) {
    if (IsInDirs(fonts.at(members.at(idx)).path, system_dirs)) {
      continue;
    }
    if (best < 0 || rank(idx) < rank(best)) {
      best = idx;
    }
  }
  return best;
}

class UnionFind {
 public:
  int Find(int idx) {
    while (parent_.value(idx, idx) != idx) {
      idx = parent_.value(idx);
    }
    return idx;
  }
  void Join(int a, int b) { parent_[Find(a)] = Find(b); }

 private:
  QHash<int, int> parent_;
};

}  // namespace

bool IsInDirs(const QString& path, const QStringList& dirs) {
  const QString key = PathKey(path);
  return std::any_of(dirs.begin(), dirs.end(), [&](const QString& dir) {
    QString prefix = PathKey(dir);
    if (!prefix.endsWith('/')) {
      prefix.append('/');
    }
    return key.startsWith(prefix);
  });
}

QStringList MoveFiles(const QStringList& sources, const QString& target_dir) {
  const QDir dir(target_dir);
  QStringList failed;
  for (const auto& source : sources) {
    const QFileInfo info(source);
    QString target = dir.filePath(info.fileName());
    for (int num = 2; QFileInfo::exists(target); ++num) {
      target = dir.filePath(QString("%1 (%2).%3")
                                .arg(info.completeBaseName())
                                .arg(num)
                                .arg(info.suffix()));
    }
    // Rename fails across drives; copy and remove then, and never leave
    // the file in both places.
    bool is_moved = QFile::rename(source, target);
    if (!is_moved && QFile::copy(source, target)) {
      is_moved = QFile::remove(source);
      if (!is_moved) {
        QFile::remove(target);
      }
    }
    if (!is_moved) {
      failed.append(source);
    }
  }
  return failed;
}

Report Find(const QVector<FontDatabase::Font>& fonts,
            const QStringList& system_dirs) {
  Report report;

  // Faces by the glyphs they draw.
  QMap<QString, QVector<int>> by_glyphs;
  for (int idx = 0; idx < fonts.size(); ++idx) {
    if (fonts.at(idx).glyph_hash.isEmpty()) {
      ++report.num_unchecked;
    } else {
      by_glyphs[fonts.at(idx).glyph_hash].append(idx);
    }
  }

  auto sort_by_path = [&](QVector<int>& members) {
    std::sort(members.begin(), members.end(), [&](int a, int b) {
      return fonts.at(a).path < fonts.at(b).path;
    });
  };

  auto num_files = [&](const QVector<int>& members) {
    QSet<QString> paths;
    for (int idx : members) {
      paths.insert(PathKey(fonts.at(idx).path));
    }
    return paths.size();
  };

  // Copies: the same glyphs in more than one file. Identical faces get a
  // group of their own, since all but one of them can go.
  for (auto it = by_glyphs.begin(); it != by_glyphs.end(); ++it) {
    QVector<int> members = it.value();
    if (num_files(members) < 2) {
      continue;
    }
    sort_by_path(members);

    QMap<QString, QVector<int>> by_face;
    for (int idx : members) {
      by_face[fonts.at(idx).face_hash].append(idx);
    }

    for (const auto& copies : by_face) {
      if (num_files(copies) < 2) {
        continue;
      }
      Group group;
      group.kind = Kind::kIdentical;
      group.fonts = copies;
      group.title = Title(fonts.at(copies.first()));
      group.keep = BestCopy(fonts, copies, system_dirs);
      for (int idx = 0; idx < copies.size(); ++idx) {
        if (group.keep >= 0 && idx != group.keep &&
            !IsInDirs(fonts.at(copies.at(idx)).path, system_dirs)) {
          group.removable.append(idx);
        }
      }
      report.groups.append(group);
    }

    if (by_face.size() > 1) {
      Group group;
      group.kind = Kind::kRepackaged;
      group.fonts = members;
      group.title = Title(fonts.at(members.first()));
      report.groups.append(group);
    }
  }

  // Names: which glyph sets answer to the same name at the same weight and
  // slant. That is what a subtitle asks for.
  const QStringList glyph_keys = by_glyphs.keys();
  QHash<QString, int> glyph_ids;
  for (int idx = 0; idx < glyph_keys.size(); ++idx) {
    glyph_ids.insert(glyph_keys.at(idx), idx);
  }

  QHash<QString, QSet<int>> by_name;
  QHash<QString, QString> name_text;
  for (int idx = 0; idx < fonts.size(); ++idx) {
    const auto& font = fonts.at(idx);
    if (font.glyph_hash.isEmpty()) {
      continue;
    }
    QSet<QString> names;
    for (const auto& name : font.families + font.fullnames + font.psnames) {
      names.insert(name.toLower());
    }
    for (const auto& name : names) {
      const QString key =
          QString("%1|%2|%3").arg(name).arg(font.weight).arg(font.slant);
      by_name[key].insert(glyph_ids.value(font.glyph_hash));
      name_text.insert(key, name);
    }
  }

  UnionFind sets;
  QHash<int, QStringList> shared_names;
  for (auto it = by_name.begin(); it != by_name.end(); ++it) {
    if (it.value().size() < 2) {
      continue;
    }
    const int first = *it.value().begin();
    for (int id : it.value()) {
      sets.Join(id, first);
    }
    shared_names[first].append(name_text.value(it.key()));
  }

  QHash<int, QStringList> component_names;
  for (auto it = shared_names.begin(); it != shared_names.end(); ++it) {
    component_names[sets.Find(it.key())].append(it.value());
  }
  QHash<int, QSet<int>> component_glyphs;
  for (auto it = by_name.begin(); it != by_name.end(); ++it) {
    if (it.value().size() < 2) {
      continue;
    }
    for (int id : it.value()) {
      component_glyphs[sets.Find(id)].insert(id);
    }
  }

  QCollator collator;
  collator.setNumericMode(true);

  for (auto it = component_glyphs.begin(); it != component_glyphs.end();
       ++it) {
    QVector<int> members;
    for (int id : it.value()) {
      members.append(by_glyphs.value(glyph_keys.at(id)));
    }
    // Faces of one collection sharing a name is how that file was made;
    // nothing in the library can be changed about it.
    if (num_files(members) < 2) {
      continue;
    }
    sort_by_path(members);

    QStringList names = component_names.value(it.key());
    names.removeDuplicates();
    std::sort(names.begin(), names.end());

    Group group;
    group.kind = Kind::kConflict;
    group.fonts = members;
    group.shared_names = names;
    group.title = names.isEmpty() ? Title(fonts.at(members.first()))
                                  : names.first();

    // Releases of one font keep their PostScript name and vendor and
    // differ in version.
    QSet<QString> common_psnames;
    QSet<QString> vendors;
    QSet<QString> versions;
    for (int pos = 0; pos < members.size(); ++pos) {
      const auto& font = fonts.at(members.at(pos));
      const QStringList psnames = font.psnames;
      const QSet<QString> psname_set(psnames.begin(), psnames.end());
      common_psnames = pos == 0 ? psname_set : common_psnames & psname_set;
      if (!font.vendor.isEmpty()) {
        vendors.insert(font.vendor);
      }
      versions.insert(QString("%1|%2").arg(font.version).arg(font.revision));
    }
    if (!common_psnames.isEmpty() && vendors.size() <= 1 &&
        versions.size() > 1) {
      group.kind = Kind::kVersions;
      auto newer = [&](int a, int b) {
        const auto& fa = fonts.at(members.at(a));
        const auto& fb = fonts.at(members.at(b));
        if (fa.revision != fb.revision) {
          return fa.revision > fb.revision;
        }
        return collator.compare(fa.version, fb.version) > 0;
      };
      QVector<int> order(members.size());
      std::iota(order.begin(), order.end(), 0);
      std::stable_sort(order.begin(), order.end(), newer);
      // Among the newest copies, prefer one outside the system folders.
      group.keep = order.first();
      for (int pos : order) {
        if (newer(order.first(), pos)) {
          break;
        }
        if (!IsInDirs(fonts.at(members.at(pos)).path, system_dirs)) {
          group.keep = pos;
          break;
        }
      }
    }
    report.groups.append(group);
  }

  std::stable_sort(report.groups.begin(), report.groups.end(),
                   [&](const Group& a, const Group& b) {
                     if (a.kind != b.kind) {
                       return a.kind < b.kind;
                     }
                     return collator.compare(a.title, b.title) < 0;
                   });

  return report;
}

}  // namespace duplicates
