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

#include <QCheckBox>
#include <QDialog>
#include <QHash>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QVector>

#include "duplicates.h"
#include "font_db.h"
#include "widgets.h"

// Lists duplicate fonts and name conflicts, and moves the files the user
// picks out of the library.
class DuplicatesDialog : public QDialog {
  Q_OBJECT

 public:
  DuplicatesDialog(const FontDatabase* font_db,
                   const duplicates::Report& report,
                   const QStringList& library_dirs, QSettings* settings,
                   QWidget* parent = nullptr);

  // True after files were moved; the library needs an update then.
  bool HasMoved() const { return has_moved_; }

 private:
  const FontDatabase* font_db_;
  QStringList library_dirs_;
  QSettings* settings_;
  bool has_moved_ = false;
  QHash<QString, int> faces_per_file_;

  SegmentedControl* filter_segment_;
  QVector<QPair<duplicates::Kind, QWidget*>> group_widgets_;
  QHash<QString, QVector<QCheckBox*>> checks_by_path_;
  QPushButton* move_button_;

  QWidget* GroupWidget(const duplicates::Group& group);
  QStringList Selected() const;
  void ApplyFilter();
  void RefreshButton();
  void MoveSelected();
};
