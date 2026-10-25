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

#include <atomic>

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include "assfonts.h"

// Runs library calls on a worker thread. Slots are invoked through queued
// connections, so requests are handled one after another.
class TaskRunner : public QObject {
  Q_OBJECT

 public:
  static TaskRunner& GetInstance() {
    static TaskRunner instance;
    return instance;
  }

  TaskRunner(const TaskRunner&) = delete;
  TaskRunner(TaskRunner&&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
  TaskRunner& operator=(TaskRunner&&) = delete;

  inline bool IsRunning() { return is_running_; }

  // Stops before the next subtitle starts. Safe to call from any thread.
  void RequestStop() { is_stop_requested_ = true; }

  // Rescans the font folders; unchanged files are taken from the database.
  void OnBuildRun(const QStringList fonts_dirs, const QString db_path,
                  const bool is_rebuild);

  void OnCheckRun(const QStringList inputs, const QString db_path);

  // Checks the inputs against the fonts in `pack_dir` only.
  void OnPackScanRun(const QStringList inputs, const QString pack_dir);

  // Copies the font files into `target_dir` and adds the copies to the fonts
  // database. A file already there with the same content is reused; one with
  // different content is kept and the copy gets a new name.
  void OnImportRun(const QStringList sources, const QString target_dir,
                   const QString db_path);

  // `outputs` holds the output folder of each input. Options: db_path,
  // brightness, subset_only, embed_only, rename, combined, non_ttf_quiet,
  // multi_thread,
  // update_db and fonts_dirs (used when update_db is set).
  void OnStartRun(const QStringList inputs, const QStringList outputs,
                  const QVariantMap options);

 signals:
  void OnSendLog(QString msg, ASSFONTS_LOG_LEVEL log_level);
  void OnSendBusy(bool is_busy, QString what);
  void OnSendBuildDone();
  void OnSendCheckDone(QStringList inputs, QString json);
  void OnSendPackScanDone(QString pack_dir, QString json);
  void OnSendFileStarted(QString input);
  void OnSendFileDone(QString input, bool is_ok, QString error);
  void OnSendStartDone(bool is_stopped);

 private:
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_stop_requested_{false};

  TaskRunner(QObject* parent = nullptr) : QObject(parent){};
  ~TaskRunner() = default;

  void BuildDB(const QStringList& fonts_dirs, const QString& db_path);
};
