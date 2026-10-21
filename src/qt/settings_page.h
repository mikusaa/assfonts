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

#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QWidget>

#include "widgets.h"

// Every change is written to the settings right away.
class SettingsPage : public QWidget {
  Q_OBJECT

 public:
  SettingsPage(QSettings* settings, QWidget* parent = nullptr);

  void SetAutoClearLog(bool is_auto_clear);

  void RefreshTheme();

 signals:
  void LanguageChanged();
  void ThemeChanged();
  void DatabaseDirChanged();
  void AutoClearLogChanged(bool is_auto_clear);
  void ResetRequested();
  void CheckUpdateRequested();

 private:
  QSettings* settings_;

  QComboBox* language_combo_;
  SegmentedControl* theme_segment_;
  ToggleSwitch* multi_thread_switch_;
  ToggleSwitch* precheck_switch_;
  ToggleSwitch* open_output_switch_;
  ToggleSwitch* auto_clear_switch_;
  ToggleSwitch* auto_expand_switch_;
  QLabel* db_dir_label_;

  QWidget* Section(const QString& title, const QList<QWidget*>& rows);
};
