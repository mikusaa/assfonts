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

#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "duplicates.h"
#include "font_db.h"
#include "widgets.h"

class LibraryPage : public QWidget {
  Q_OBJECT

 public:
  LibraryPage(QSettings* settings, const FontDatabase* font_db,
              QWidget* parent = nullptr);

  QStringList FontDirs() const { return dirs_; }

  void AddDirs(const QStringList& dirs);

  // Call after the database file was reloaded.
  void Refresh();

  void SetBusy(bool is_busy);

  void ShowSearch(const QString& text);

  bool IsAutoUpdate() const;
  void SetAutoUpdate(bool is_auto_update);

  // True when some folder has new or removed font files.
  bool HasChanges() const { return has_changes_; }

  void SaveSettings();
  void RefreshTheme();

 signals:
  void RequestBuild(bool is_rebuild);
  void DirsChanged();
  void AutoUpdateChanged(bool is_auto_update);
  void ChangesScanned(bool has_changes);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  QSettings* settings_;
  const FontDatabase* font_db_;
  QStringList dirs_;
  int scan_generation_ = 0;
  int num_scans_pending_ = 0;
  bool has_changes_ = false;

  QLabel* stat_dirs_label_;
  QLabel* stat_files_label_;
  QLabel* stat_families_label_;
  QLabel* stat_updated_label_;

  QPushButton* rebuild_button_;
  QPushButton* update_button_;
  QProgressBar* busy_bar_;

  QVBoxLayout* dirs_layout_;
  QLabel* system_dirs_label_;
  ToggleSwitch* auto_update_switch_;
  ToggleSwitch* check_on_start_switch_;

  duplicates::Report dupes_report_;
  QHBoxLayout* dupes_chips_layout_;
  QLabel* dupes_summary_label_;
  QPushButton* dupes_button_;

  QLineEdit* search_line_;
  QLabel* search_count_label_;
  QTableWidget* search_table_;

  QLabel* db_path_label_;

  QWidget* InitHeader();
  QWidget* InitStats();
  QWidget* InitDirsCard();
  QWidget* InitDupesCard();
  QWidget* InitSearchCard();

  void RebuildDirs();
  void RunSearch();
  void RefreshDupes();
  void RemoveDir(const QString& dir);
};
