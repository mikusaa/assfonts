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

#include "task_runner.h"

#include <memory>
#include <thread>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QThreadPool>
#include <QtConcurrent>

#include "font_db.h"

namespace {

// Errors are collected per subtitle. The library reports every log line on
// the thread that called it, so a thread-local pointer tells which subtitle a
// line belongs to.
struct JobState {
  QString first_error;
};

thread_local JobState* g_job = nullptr;

void log_callback(const char* msg, const ASSFONTS_LOG_LEVEL log_level) {
  QString msg_string = QString::fromUtf8(msg);

  if (g_job != nullptr && log_level == ASSFONTS_ERROR &&
      g_job->first_error.isEmpty()) {
    g_job->first_error = msg_string;
  }

  emit TaskRunner::GetInstance().OnSendLog(msg_string, log_level);
}

// Keeps UTF-8 copies of a string list alive for the duration of a C call.
class CStringList {
 public:
  explicit CStringList(const QStringList& list) {
    for (const auto& str : list) {
      bytes_.push_back(str.trimmed().toUtf8());
    }
    for (const auto& bytes : bytes_) {
      ptrs_.push_back(bytes.constData());
    }
    if (ptrs_.empty()) {
      ptrs_.push_back("");
    }
  }

  const char** data() { return ptrs_.data(); }
  unsigned int size() const { return static_cast<unsigned int>(bytes_.size()); }

 private:
  std::vector<QByteArray> bytes_;
  std::vector<const char*> ptrs_;
};

}  // namespace

void TaskRunner::BuildDB(const QStringList& fonts_dirs,
                         const QString& db_path) {
  // The library refuses to build without a folder, but always adds the system
  // font folders itself, so one of those stands in when the list is empty.
  QStringList dirs = fonts_dirs;
  if (dirs.isEmpty()) {
    for (const auto& dir : FontDatabase::SystemFontDirs()) {
      if (QDir(dir).exists()) {
        dirs.append(dir);
        break;
      }
    }
  }

  QDir().mkpath(db_path);

  CStringList c_dirs(dirs);
  AssfontsBuildDB(c_dirs.data(), c_dirs.size(), db_path.toUtf8().constData(),
                  log_callback, ASSFONTS_INFO);
}

void TaskRunner::OnBuildRun(const QStringList fonts_dirs,
                            const QString db_path, const bool is_rebuild) {
  is_running_ = true;
  emit OnSendBusy(true, tr("Updating fonts database…"));

  if (is_rebuild) {
    QFile::remove(FontDatabase::DbFile(db_path));
  }

  BuildDB(fonts_dirs, db_path);

  emit OnSendBuildDone();
  emit OnSendBusy(false, QString());
  is_running_ = false;
}

void TaskRunner::OnCheckRun(const QStringList inputs, const QString db_path) {
  if (inputs.isEmpty()) {
    return;
  }

  is_running_ = true;
  emit OnSendBusy(true, tr("Checking fonts…"));

  CStringList c_inputs(inputs);
  // Only errors (unreadable files) are worth showing for a check.
  char* res = AssfontsCheck(c_inputs.data(), c_inputs.size(),
                            db_path.toUtf8().constData(), log_callback,
                            ASSFONTS_ERROR);
  QString json = QString::fromUtf8(res);
  AssfontsFreeString(res);

  emit OnSendCheckDone(inputs, json);
  emit OnSendBusy(false, QString());
  is_running_ = false;
}

void TaskRunner::OnPackScanRun(const QStringList inputs,
                               const QString pack_dir) {
  if (inputs.isEmpty()) {
    return;
  }

  is_running_ = true;
  emit OnSendBusy(true, tr("Searching the font pack…"));

  CStringList c_inputs(inputs);
  CStringList c_dirs(QStringList{pack_dir});
  char* res = AssfontsCheckFonts(c_inputs.data(), c_inputs.size(),
                                 c_dirs.data(), c_dirs.size(), log_callback,
                                 ASSFONTS_ERROR);
  QString json = QString::fromUtf8(res);
  AssfontsFreeString(res);

  emit OnSendPackScanDone(pack_dir, json);
  emit OnSendBusy(false, QString());
  is_running_ = false;
}

void TaskRunner::OnImportRun(const QStringList sources,
                             const QString target_dir, const QString db_path) {
  is_running_ = true;
  emit OnSendBusy(true, tr("Importing fonts…"));

  auto is_same_file = [](const QString& a, const QString& b) {
    QFile file_a(a);
    QFile file_b(b);
    if (file_a.size() != file_b.size() || !file_a.open(QIODevice::ReadOnly) ||
        !file_b.open(QIODevice::ReadOnly)) {
      return false;
    }
    constexpr qint64 kChunk = 1 << 20;
    while (!file_a.atEnd()) {
      if (file_a.read(kChunk) != file_b.read(kChunk)) {
        return false;
      }
    }
    return true;
  };

  QDir().mkpath(target_dir);
  const QDir dir(target_dir);

  QStringList added;
  for (const auto& source : sources) {
    const QFileInfo info(source);
    QString target = dir.filePath(info.fileName());

    if (QFileInfo::exists(target)) {
      if (is_same_file(source, target)) {
        log_callback(tr("\"%1\" is already in \"%2\"")
                         .arg(info.fileName(), target_dir)
                         .toUtf8()
                         .constData(),
                     ASSFONTS_INFO);
        added.append(target);
        continue;
      }
      // A different font with the same file name: keep both.
      for (int num = 2; QFileInfo::exists(target); ++num) {
        target = dir.filePath(QString("%1 (%2).%3")
                                  .arg(info.completeBaseName())
                                  .arg(num)
                                  .arg(info.suffix()));
      }
    }

    if (QFile::copy(source, target)) {
      log_callback(tr("Copied \"%1\" to \"%2\"")
                       .arg(source, target)
                       .toUtf8()
                       .constData(),
                   ASSFONTS_INFO);
      added.append(target);
    } else {
      log_callback(tr("Cannot copy \"%1\" to \"%2\"")
                       .arg(source, target)
                       .toUtf8()
                       .constData(),
                   ASSFONTS_WARN);
    }
  }

  if (!added.isEmpty()) {
    QDir().mkpath(db_path);
    CStringList c_files(added);
    AssfontsAddFonts(c_files.data(), c_files.size(),
                     db_path.toUtf8().constData(), log_callback, ASSFONTS_INFO);
  }

  emit OnSendBuildDone();
  emit OnSendBusy(false, QString());
  is_running_ = false;
}

void TaskRunner::OnStartRun(const QStringList inputs,
                            const QStringList outputs,
                            const QVariantMap options) {
  is_running_ = true;
  is_stop_requested_ = false;

  const QString db_path = options.value("db_path").toString();
  const QByteArray db_bytes = db_path.toUtf8();
  const unsigned int brightness = options.value("brightness").toUInt();
  const bool is_subset_only = options.value("subset_only").toBool();
  const bool is_embed_only = options.value("embed_only").toBool();
  const bool is_rename = options.value("rename").toBool();
  const bool is_combined = options.value("combined").toBool();
  const bool is_multi_thread = options.value("multi_thread").toBool();

  if (options.value("update_db").toBool()) {
    emit OnSendBusy(true, tr("Updating fonts database…"));
    BuildDB(options.value("fonts_dirs").toStringList(), db_path);
    log_callback("", ASSFONTS_TEXT);
  }

  emit OnSendBusy(true, tr("Processing subtitles…"));

  const char* no_fonts[] = {""};

  if (is_combined) {
    // Combining needs every subtitle sharing an output folder in one call.
    QMap<QString, QStringList> groups;
    for (int idx = 0; idx < inputs.size(); ++idx) {
      groups[outputs.at(idx)].append(inputs.at(idx));
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
      if (is_stop_requested_) {
        break;
      }

      for (const auto& input : it.value()) {
        emit OnSendFileStarted(input);
      }

      JobState state;
      g_job = &state;
      CStringList c_inputs(it.value());
      AssfontsRun(c_inputs.data(), c_inputs.size(),
                  it.key().toUtf8().constData(), no_fonts, 0,
                  db_bytes.constData(), brightness, is_subset_only,
                  is_embed_only, is_rename, true,
                  is_multi_thread ? std::thread::hardware_concurrency() : 1,
                  log_callback, ASSFONTS_INFO);
      g_job = nullptr;
      log_callback("", ASSFONTS_TEXT);

      for (const auto& input : it.value()) {
        emit OnSendFileDone(input, state.first_error.isEmpty(),
                            state.first_error);
      }
    }

  } else {
    auto run_one = [&](int idx) {
      if (is_stop_requested_) {
        return;
      }

      emit OnSendFileStarted(inputs.at(idx));

      JobState state;
      g_job = &state;
      QByteArray input_bytes = inputs.at(idx).toUtf8();
      const char* input_ptr[] = {input_bytes.constData()};
      AssfontsRun(input_ptr, 1, outputs.at(idx).toUtf8().constData(),
                  no_fonts, 0, db_bytes.constData(), brightness,
                  is_subset_only, is_embed_only, is_rename, false, 1,
                  log_callback, ASSFONTS_INFO);
      g_job = nullptr;
      log_callback("", ASSFONTS_TEXT);

      emit OnSendFileDone(inputs.at(idx), state.first_error.isEmpty(),
                          state.first_error);
    };

    QList<int> indexes;
    for (int idx = 0; idx < inputs.size(); ++idx) {
      indexes.append(idx);
    }

    if (is_multi_thread && inputs.size() > 1) {
      QThreadPool pool;
      pool.setMaxThreadCount(
          std::max(1, static_cast<int>(std::thread::hardware_concurrency())));
      QtConcurrent::blockingMap(&pool, indexes, run_one);
    } else {
      for (int idx : indexes) {
        run_one(idx);
      }
    }
  }

  const bool is_stopped = is_stop_requested_;
  is_stop_requested_ = false;

  emit OnSendStartDone(is_stopped);
  emit OnSendBusy(false, QString());
  is_running_ = false;
}
