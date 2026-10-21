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
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

#include "font_db.h"
#include "widgets.h"

class ProcessPage : public QWidget {
  Q_OBJECT

 public:
  ProcessPage(QSettings* settings, const FontDatabase* font_db,
              QWidget* parent = nullptr);

  // Adds subtitle files; folders are searched for .ass/.ssa files.
  void AddPaths(const QStringList& paths);

  void OnCheckDone(const QStringList& inputs, const QString& json);
  void OnFileStarted(const QString& input);
  void OnFileDone(const QString& input, bool is_ok, const QString& error);
  void OnRunDone(bool is_stopped);

  void SetBusy(bool is_busy);

  // Call after the fonts database or the font folder list changed.
  void RefreshLibraryStatus(int num_dirs);

  bool IsAutoUpdate() const;
  void SetAutoUpdate(bool is_auto_update);

  void SaveSettings();
  void RefreshTheme();

  // Re-checks every file, e.g. after the fonts database changed.
  void CheckAll();

  // Processes the given files (all files when the user presses Start).
  void Start(const QStringList& inputs);

 signals:
  void RequestCheck(QStringList inputs);
  void RequestStart(QStringList inputs, QStringList outputs,
                    QVariantMap options);
  void RequestStop();
  void RequestUpdateAndRetry(QStringList inputs);
  void RequestShowLibrary(QString search);
  void AutoUpdateChanged(bool is_auto_update);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  enum class State {
    kPending,
    kChecking,
    kReady,
    kMissing,
    kUnreadable,
    kRunning,
    kDone,
    kFailed,
    kSkipped
  };

  struct FontKey {
    QString name;
    int bold = 400;
    int italic = 0;
  };

  struct FontUse {
    FontKey key;
    qint64 num_chars = 0;
    QVector<uint> missing_codepoints;
  };

  struct FileItem {
    QString path;
    State state = State::kPending;
    int num_fonts = -1;
    QVector<FontUse> missing;
    QVector<FontUse> glyph_issues;
    QString error;
  };

  struct MissingFont {
    FontKey key;
    QStringList files;
    qint64 num_chars = 0;
  };

  struct GlyphIssue {
    FontKey key;
    QStringList files;
    QSet<uint> codepoints;
  };

  QSettings* settings_;
  const FontDatabase* font_db_;

  QVector<FileItem> files_;
  QVector<MissingFont> missing_fonts_;
  QVector<GlyphIssue> glyph_issues_;
  bool is_busy_ = false;
  bool is_running_ = false;
  int num_finished_ = 0;
  int num_to_run_ = 0;
  QString last_output_dir_;

  QLabel* file_count_label_;
  QTableWidget* table_;
  QLabel* drop_zone_;

  QFrame* missing_card_;
  QLabel* missing_count_label_;
  QVBoxLayout* missing_list_layout_;
  QPushButton* retry_button_;

  SegmentedControl* output_segment_;
  QWidget* output_dir_row_;
  QLineEdit* output_dir_line_;
  QLabel* output_hint_label_;
  SegmentedControl* mode_segment_;
  SegmentedControl* hdr_segment_;
  ToggleSwitch* rename_switch_;
  ToggleSwitch* combined_switch_;

  QLabel* library_title_label_;
  QLabel* library_detail_label_;
  ToggleSwitch* auto_update_switch_;

  QLabel* summary_label_;
  QProgressBar* progress_bar_;
  QPushButton* open_output_button_;
  QPushButton* check_button_;
  QPushButton* start_button_;

  QWidget* InitHeader();
  QWidget* InitFilesCard();
  QWidget* InitMissingCard();
  QWidget* InitOptionsCard();
  QWidget* InitLibraryStrip();
  QWidget* InitActionRow();

  int IndexOf(const QString& path) const;
  QString OutputDirFor(const QString& input) const;
  void SetState(int row, State state);
  void RefreshRow(int row);
  void RebuildTable();
  void RebuildMissing();
  void RefreshSummary();
  void RemoveRow(int row);
  QVariantMap Options() const;

  static QString StyleText(const FontKey& key);
  static QString FileList(const QStringList& files);
};
