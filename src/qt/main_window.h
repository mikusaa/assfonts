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

#include <QButtonGroup>
#include <QMainWindow>
#include <QSettings>
#include <QStackedWidget>

#include "check_window.h"
#include "font_db.h"
#include "library_page.h"
#include "log_panel.h"
#include "process_page.h"
#include "settings_page.h"
#include "task_runner.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QSettings* settings, QWidget* parent = nullptr);

  // Where settings and the default fonts database live.
  static QString AppDataDir();

 signals:
  // The window has to be rebuilt, e.g. for a new language.
  void RestartRequested();

  void OnSendBuild(QStringList fonts_dirs, QString db_path, bool is_rebuild);
  void OnSendCheck(QStringList inputs, QString db_path);
  void OnSendPackScan(QStringList inputs, QString pack_dir);
  void OnSendImport(QStringList sources, QString target_dir, QString db_path);
  void OnSendStart(QStringList inputs, QStringList outputs,
                   QVariantMap options);

 protected:
  void closeEvent(QCloseEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  QSettings* settings_;
  FontDatabase font_db_;
  TaskRunner* worker_;

  QButtonGroup* nav_group_;
  QStackedWidget* stack_;
  ProcessPage* process_page_;
  LibraryPage* library_page_;
  SettingsPage* settings_page_;
  LogPanel* log_panel_;
  CheckWindow* check_window_ = nullptr;

  bool is_retry_pending_ = false;
  bool is_startup_scan_ = true;

  QWidget* InitSidebar();
  void InitConnects();

  QString DbPath() const;
  void ReloadDatabase();
  void ShowPage(int index);
  void ApplyTheme();
  void UpdateLibrary(bool is_rebuild);
  void ShowCheckWindow();
  bool ConfirmIdle();

  void LoadSettings();
  void SaveSettings();
  void MigrateOldSettings();
};
