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

#include "process_page.h"

#include <algorithm>

#include <QClipboard>
#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QScrollArea>
#include <QUrl>
#include <QUrlQuery>

#include "log_translator.h"
#include "theme.h"

namespace {

enum Column { kName = 0, kFonts, kStatus, kRemove, kNumColumns };

bool IsSubtitle(const QFileInfo& info) {
  const QString suffix = info.suffix().toLower();
  return info.isFile() && (suffix == "ass" || suffix == "ssa");
}

QString RelativeTime(const QDateTime& time) {
  const qint64 secs = time.secsTo(QDateTime::currentDateTime());
  if (secs < 60) {
    return ProcessPage::tr("just now");
  }
  if (secs < 3600) {
    return ProcessPage::tr("%1 min ago").arg(secs / 60);
  }
  if (secs < 86400) {
    return ProcessPage::tr("%1 h ago").arg(secs / 3600);
  }
  return QLocale().toString(time.date(), QLocale::ShortFormat);
}

bool IsMissingFontsError(const QString& error) {
  return error.startsWith("Found missing fonts");
}

}  // namespace

ProcessPage::ProcessPage(QSettings* settings, const FontDatabase* font_db,
                         QWidget* parent)
    : QWidget(parent), settings_(settings), font_db_(font_db) {
  setObjectName("page");
  setAcceptDrops(true);

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  outer->addWidget(scroll);

  auto* content = new QWidget;
  content->setObjectName("scrollContent");
  scroll->setWidget(content);

  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(28, 24, 28, 20);
  layout->setSpacing(16);

  layout->addWidget(InitHeader());

  auto* files_row = new QHBoxLayout;
  files_row->setSpacing(16);
  files_row->addWidget(InitFilesCard(), 3);
  files_row->addWidget(InitMissingCard(), 2);
  layout->addLayout(files_row);

  layout->addWidget(InitOptionsCard());
  layout->addWidget(InitLibraryStrip());
  layout->addWidget(InitActionRow());
  layout->addStretch();

  settings_->beginGroup("Process");
  output_segment_->SetCurrentIndex(settings_->value("OutputMode", 0).toInt());
  output_dir_line_->setText(settings_->value("OutputDir").toString());
  mode_segment_->SetCurrentIndex(settings_->value("Mode", 0).toInt());
  hdr_segment_->SetCurrentIndex(settings_->value("Hdr", 0).toInt());
  rename_switch_->setChecked(settings_->value("Rename", false).toBool());
  combined_switch_->setChecked(settings_->value("Combined", false).toBool());
  settings_->endGroup();

  output_dir_row_->setVisible(output_segment_->CurrentIndex() == 1);
  output_hint_label_->setVisible(output_segment_->CurrentIndex() == 0);

  RebuildTable();
  RebuildMissing();
  RefreshSummary();
}

QWidget* ProcessPage::InitHeader() {
  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(0, 0, 0, 0);

  auto* titles = new QVBoxLayout;
  titles->setSpacing(2);
  titles->addWidget(ui::Label(tr("Process subtitles"), "h1"));
  titles->addWidget(ui::Label(
      tr("Drop subtitles or whole folders here. The fonts they use are "
         "subset and embedded automatically."),
      "muted"));
  hlayout->addLayout(titles, 1);

  auto* clear_button = ui::Button(tr("Clear list"));
  hlayout->addWidget(clear_button, 0, Qt::AlignBottom);

  connect(clear_button, &QPushButton::clicked, this, [this]() {
    if (is_running_) {
      return;
    }
    files_.clear();
    RebuildTable();
    RebuildMissing();
    RefreshSummary();
  });

  return header;
}

QWidget* ProcessPage::InitFilesCard() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(16, 12, 12, 12);
  hlayout->setSpacing(8);
  hlayout->addWidget(ui::Label(tr("Subtitle files"), "h2"));
  file_count_label_ = ui::Chip("0", "neutral");
  hlayout->addWidget(file_count_label_);
  hlayout->addStretch();

  auto* add_files_button = ui::Button(tr("Add files"), QString(), "plus");
  auto* add_folder_button = ui::Button(tr("Add folder"), QString(), "folder");
  hlayout->addWidget(add_files_button);
  hlayout->addWidget(add_folder_button);
  layout->addWidget(header);
  layout->addWidget(ui::Divider());

  table_ = new QTableWidget(0, kNumColumns);
  table_->setHorizontalHeaderLabels(
      {tr("File"), tr("Fonts"), tr("Status"), QString()});
  table_->verticalHeader()->hide();
  table_->verticalHeader()->setDefaultSectionSize(52);
  table_->setShowGrid(false);
  table_->setSelectionMode(QAbstractItemView::NoSelection);
  table_->setFocusPolicy(Qt::NoFocus);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setWordWrap(false);
  table_->setTextElideMode(Qt::ElideMiddle);
  table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  table_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  table_->setMaximumHeight(420);

  QHeaderView* header_view = table_->horizontalHeader();
  header_view->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  header_view->setHighlightSections(false);
  header_view->setSectionResizeMode(kName, QHeaderView::Stretch);
  header_view->setSectionResizeMode(kFonts, QHeaderView::ResizeToContents);
  header_view->setSectionResizeMode(kStatus, QHeaderView::Fixed);
  header_view->setSectionResizeMode(kRemove, QHeaderView::Fixed);
  header_view->resizeSection(kStatus, 176);
  header_view->resizeSection(kRemove, 44);
  layout->addWidget(table_);

  drop_zone_ = new QLabel;
  drop_zone_->setObjectName("dropZone");
  drop_zone_->setAlignment(Qt::AlignCenter);
  drop_zone_->setWordWrap(true);

  auto* drop_wrap = new QWidget;
  auto* drop_layout = new QVBoxLayout(drop_wrap);
  drop_layout->setContentsMargins(16, 12, 16, 16);
  drop_layout->addWidget(drop_zone_);
  layout->addWidget(drop_wrap);

  connect(add_files_button, &QPushButton::clicked, this, [this]() {
    const QString start =
        settings_->value("Process/LastInputDir", QDir::homePath()).toString();
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Choose subtitle files"), start,
        tr("ASS subtitles (*.ass *.ssa)"));
    if (!paths.isEmpty()) {
      settings_->setValue("Process/LastInputDir",
                          QFileInfo(paths.first()).absolutePath());
      AddPaths(paths);
    }
  });

  connect(add_folder_button, &QPushButton::clicked, this, [this]() {
    const QString start =
        settings_->value("Process/LastInputDir", QDir::homePath()).toString();
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose a folder with subtitles"), start);
    if (!path.isEmpty()) {
      settings_->setValue("Process/LastInputDir", path);
      AddPaths({path});
    }
  });

  return card;
}

QWidget* ProcessPage::InitMissingCard() {
  QVBoxLayout* layout = nullptr;
  missing_card_ = ui::Card(&layout, 0, 0);
  missing_card_->setMinimumWidth(400);

  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(16, 12, 12, 12);
  hlayout->setSpacing(8);
  hlayout->addWidget(ui::Label(tr("Missing fonts"), "h2"));
  missing_count_label_ = ui::Chip("0", "warn");
  hlayout->addWidget(missing_count_label_);
  hlayout->addStretch();
  auto* copy_all_button = ui::Button(tr("Copy all names"), QString(), "copy");
  hlayout->addWidget(copy_all_button);
  layout->addWidget(header);
  layout->addWidget(ui::Divider());

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* list = new QWidget;
  list->setObjectName("scrollContent");
  missing_list_layout_ = new QVBoxLayout(list);
  missing_list_layout_->setContentsMargins(0, 0, 0, 0);
  missing_list_layout_->setSpacing(0);
  scroll->setWidget(list);
  scroll->setMinimumHeight(160);
  scroll->setMaximumHeight(420);
  layout->addWidget(scroll, 1);

  auto* footer = new QFrame;
  footer->setProperty("cardFooter", true);
  auto* flayout = new QVBoxLayout(footer);
  flayout->setContentsMargins(16, 12, 16, 14);
  flayout->setSpacing(8);
  flayout->addWidget(ui::Label(
      tr("Put the fonts into any font folder of the library, then update the "
         "library and process the affected files again."),
      "caption"));
  retry_button_ = ui::Button(QString(), "primary", "refresh");
  flayout->addWidget(retry_button_, 0, Qt::AlignLeft);
  layout->addWidget(footer);

  connect(copy_all_button, &QPushButton::clicked, this, [this]() {
    QStringList names;
    for (const auto& font : missing_fonts_) {
      if (!names.contains(font.key.name)) {
        names.append(font.key.name);
      }
    }
    QGuiApplication::clipboard()->setText(names.join("\n"));
  });

  connect(retry_button_, &QPushButton::clicked, this, [this]() {
    QStringList inputs;
    for (const auto& file : files_) {
      if (!file.missing.isEmpty()) {
        inputs.append(file.path);
      }
    }
    if (!inputs.isEmpty() && !is_busy_) {
      emit RequestUpdateAndRetry(inputs);
    }
  });

  return missing_card_;
}

QWidget* ProcessPage::InitOptionsCard() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 20, 14);
  layout->addWidget(ui::Label(tr("Options"), "h2"));

  auto* grid = new QGridLayout;
  grid->setHorizontalSpacing(32);
  grid->setVerticalSpacing(16);

  // Output location.
  auto* output_box = new QVBoxLayout;
  output_box->setSpacing(6);
  output_box->addWidget(ui::Label(tr("Output location"), "label"));
  output_segment_ =
      new SegmentedControl({tr("Next to subtitle"), tr("Choose folder")});
  output_box->addWidget(output_segment_, 0, Qt::AlignLeft);

  output_dir_row_ = new QWidget;
  auto* dir_layout = new QHBoxLayout(output_dir_row_);
  dir_layout->setContentsMargins(0, 0, 0, 0);
  dir_layout->setSpacing(6);
  output_dir_line_ = new QLineEdit;
  output_dir_line_->setPlaceholderText(tr("Output folder"));
  auto* browse_button = ui::Button(tr("Browse…"));
  dir_layout->addWidget(output_dir_line_, 1);
  dir_layout->addWidget(browse_button);
  output_box->addWidget(output_dir_row_);

  output_hint_label_ = ui::Label(
      tr("Each result is written to the folder of its subtitle."), "caption");
  output_box->addWidget(output_hint_label_);
  grid->addLayout(output_box, 0, 0, Qt::AlignTop);

  // Processing mode.
  auto* mode_box = new QVBoxLayout;
  mode_box->setSpacing(6);
  mode_box->addWidget(ui::Label(tr("Processing"), "label"));
  mode_segment_ = new SegmentedControl(
      {tr("Subset and embed"), tr("Subset only"), tr("Embed only")});
  mode_box->addWidget(mode_segment_, 0, Qt::AlignLeft);
  mode_box->addWidget(
      ui::Label(tr("Subset only writes the font files; embed only puts whole "
                   "fonts into the subtitle."),
                "caption"));
  grid->addLayout(mode_box, 0, 1, Qt::AlignTop);

  // HDR.
  auto* hdr_box = new QVBoxLayout;
  hdr_box->setSpacing(6);
  hdr_box->addWidget(ui::Label(tr("HDR brightness"), "label"));
  hdr_segment_ = new SegmentedControl(
      {tr("Off"), tr("Low · 100 nit"), tr("High · 203 nit")});
  hdr_box->addWidget(hdr_segment_, 0, Qt::AlignLeft);
  hdr_box->addWidget(ui::Label(
      tr("Lowers subtitle brightness for HDR video."), "caption"));
  grid->addLayout(hdr_box, 1, 0, Qt::AlignTop);

  // Switches.
  auto* switch_box = new QVBoxLayout;
  switch_box->setSpacing(10);
  switch_box->addWidget(ui::SwitchRow(
      &rename_switch_, tr("Rename subset fonts"),
      tr("Gives every subtitle its own set of fonts so players never mix "
         "them up.")));
  switch_box->addWidget(ui::SwitchRow(
      &combined_switch_, tr("Combine shared fonts (experimental)"),
      tr("Subtitles using the same font share one subset.")));
  grid->addLayout(switch_box, 1, 1, Qt::AlignTop);

  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(1, 1);
  layout->addLayout(grid);

  connect(output_segment_, &SegmentedControl::CurrentChanged, this,
          [this](int index) {
            output_dir_row_->setVisible(index == 1);
            output_hint_label_->setVisible(index == 0);
          });

  connect(browse_button, &QPushButton::clicked, this, [this]() {
    QString start = output_dir_line_->text().trimmed();
    if (start.isEmpty() || !QDir(start).exists()) {
      start = QDir::homePath();
    }
    const QString path =
        QFileDialog::getExistingDirectory(this, tr("Choose output folder"), start);
    if (!path.isEmpty()) {
      output_dir_line_->setText(QDir::toNativeSeparators(path));
    }
  });

  return card;
}

QWidget* ProcessPage::InitLibraryStrip() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* row = new QWidget;
  auto* hlayout = new QHBoxLayout(row);
  hlayout->setContentsMargins(16, 10, 12, 10);
  hlayout->setSpacing(12);

  hlayout->addWidget(ui::IconLabel("font", 20, true));

  auto* texts = new QVBoxLayout;
  texts->setSpacing(0);
  library_title_label_ = ui::Label(QString(), "label");
  library_detail_label_ = ui::Label(QString(), "caption");
  texts->addWidget(library_title_label_);
  texts->addWidget(library_detail_label_);
  hlayout->addLayout(texts, 1);

  auto_update_switch_ = new ToggleSwitch;
  auto_update_switch_->setAccessibleName(tr("Update library before processing"));
  hlayout->addWidget(auto_update_switch_);
  hlayout->addWidget(ui::Label(tr("Update library before processing")));
  hlayout->addSpacing(8);

  auto* manage_button = ui::Button(tr("Manage library"));
  hlayout->addWidget(manage_button);

  layout->addWidget(row);

  connect(auto_update_switch_, &ToggleSwitch::toggled, this,
          &ProcessPage::AutoUpdateChanged);
  connect(manage_button, &QPushButton::clicked, this,
          [this]() { emit RequestShowLibrary(QString()); });

  return card;
}

QWidget* ProcessPage::InitActionRow() {
  auto* row = new QWidget;
  auto* hlayout = new QHBoxLayout(row);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(10);

  auto* left = new QVBoxLayout;
  left->setSpacing(6);
  summary_label_ = ui::Label(QString(), "muted");
  progress_bar_ = new QProgressBar;
  progress_bar_->setTextVisible(false);
  progress_bar_->setFixedWidth(240);
  progress_bar_->hide();
  left->addWidget(summary_label_);
  left->addWidget(progress_bar_);
  hlayout->addLayout(left, 1);

  open_output_button_ = ui::Button(tr("Open output folder"), QString(), "folder");
  open_output_button_->hide();
  check_button_ = ui::Button(tr("Check fonts only"));
  check_button_->setMinimumHeight(38);
  start_button_ = ui::Button(tr("Start"), "primary", "play");

  hlayout->addWidget(open_output_button_);
  hlayout->addWidget(check_button_);
  hlayout->addWidget(start_button_);

  connect(open_output_button_, &QPushButton::clicked, this, [this]() {
    if (!last_output_dir_.isEmpty()) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(last_output_dir_));
    }
  });

  connect(check_button_, &QPushButton::clicked, this, &ProcessPage::CheckAll);

  connect(start_button_, &QPushButton::clicked, this, [this]() {
    if (is_running_) {
      emit RequestStop();
      start_button_->setEnabled(false);
      start_button_->setText(tr("Stopping…"));
      return;
    }
    QStringList inputs;
    for (const auto& file : files_) {
      inputs.append(file.path);
    }
    Start(inputs);
  });

  return row;
}

void ProcessPage::AddPaths(const QStringList& paths) {
  QStringList found;

  for (const auto& path : paths) {
    QFileInfo info(path);
    if (info.isDir()) {
      QDirIterator iter(path, {"*.ass", "*.ssa", "*.ASS", "*.SSA"},
                        QDir::Files, QDirIterator::Subdirectories);
      QStringList in_dir;
      while (iter.hasNext()) {
        in_dir.append(iter.next());
      }
      QCollator collator;
      collator.setNumericMode(true);
      std::sort(in_dir.begin(), in_dir.end(), collator);
      found.append(in_dir);
    } else if (IsSubtitle(info)) {
      found.append(path);
    }
  }

  QStringList added;
  for (const auto& path : found) {
    const QString native =
        QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (IndexOf(native) >= 0) {
      continue;
    }
    // Results written by an earlier run are not inputs.
    if (native.contains(".assfonts.", Qt::CaseInsensitive) ||
        native.contains(".hdr.", Qt::CaseInsensitive)) {
      continue;
    }
    FileItem item;
    item.path = native;
    files_.append(item);
    added.append(native);
  }

  if (added.isEmpty()) {
    return;
  }

  open_output_button_->hide();
  RebuildTable();
  RefreshSummary();

  if (settings_->value("Run/AutoPrecheck", true).toBool()) {
    for (const auto& path : added) {
      SetState(IndexOf(path), State::kChecking);
    }
    emit RequestCheck(added);
  }
}

void ProcessPage::CheckAll() {
  if (files_.isEmpty() || is_running_) {
    return;
  }
  QStringList inputs;
  for (int idx = 0; idx < files_.size(); ++idx) {
    inputs.append(files_.at(idx).path);
    SetState(idx, State::kChecking);
  }
  emit RequestCheck(inputs);
}

void ProcessPage::OnCheckDone(const QStringList& inputs, const QString& json) {
  const QJsonArray results = QJsonDocument::fromJson(json.toUtf8()).array();

  for (const auto& value : results) {
    const QJsonObject result = value.toObject();
    const int row = IndexOf(result.value("input").toString());
    if (row < 0) {
      continue;
    }

    FileItem& file = files_[row];
    file.missing.clear();
    file.glyph_issues.clear();
    file.error.clear();

    if (!result.value("ok").toBool()) {
      file.num_fonts = -1;
      file.error = LogTranslator::Translate(result.value("error").toString());
      if (file.state == State::kChecking || file.state == State::kPending) {
        file.state = State::kUnreadable;
      }
      RefreshRow(row);
      continue;
    }

    const QJsonArray fonts = result.value("fonts").toArray();
    QSet<QString> names;
    for (const auto& font_value : fonts) {
      const QJsonObject font = font_value.toObject();
      FontUse use;
      use.key.name = font.value("name").toString();
      use.key.bold = font.value("bold").toInt();
      use.key.italic = font.value("italic").toInt();
      use.num_chars = font.value("num_chars").toInteger();
      for (const auto& cp : font.value("missing_codepoints").toArray()) {
        use.missing_codepoints.append(static_cast<uint>(cp.toInteger()));
      }
      names.insert(use.key.name.toLower());

      if (!font.value("found").toBool()) {
        file.missing.append(use);
      } else if (!use.missing_codepoints.isEmpty()) {
        file.glyph_issues.append(use);
      }
    }
    file.num_fonts = names.size();

    if (file.state == State::kChecking || file.state == State::kPending ||
        file.state == State::kReady || file.state == State::kMissing ||
        file.state == State::kUnreadable) {
      file.state = file.missing.isEmpty() ? State::kReady : State::kMissing;
    }
    RefreshRow(row);
  }

  // Files the library did not report on keep a usable state.
  for (const auto& input : inputs) {
    const int row = IndexOf(input);
    if (row >= 0 && files_.at(row).state == State::kChecking) {
      SetState(row, State::kPending);
    }
  }

  RebuildMissing();
  RefreshSummary();
}

void ProcessPage::OnFileStarted(const QString& input) {
  const int row = IndexOf(input);
  if (row >= 0) {
    SetState(row, State::kRunning);
  }
}

void ProcessPage::OnFileDone(const QString& input, bool is_ok,
                             const QString& error) {
  const int row = IndexOf(input);
  if (row < 0) {
    return;
  }

  ++num_finished_;
  progress_bar_->setValue(num_finished_);

  FileItem& file = files_[row];
  if (is_ok) {
    file.state = State::kDone;
    file.error.clear();
  } else {
    // A file the check could not read stays marked as unreadable.
    file.state = file.num_fonts < 0 ? State::kUnreadable : State::kFailed;
    file.error = IsMissingFontsError(error) && !file.missing.isEmpty()
                     ? QString()
                     : LogTranslator::Translate(error);
  }
  RefreshRow(row);
  RefreshSummary();
}

void ProcessPage::OnRunDone(bool is_stopped) {
  is_running_ = false;

  for (int idx = 0; idx < files_.size(); ++idx) {
    if (files_.at(idx).state == State::kRunning ||
        (is_stopped && files_.at(idx).state == State::kPending)) {
      SetState(idx, State::kSkipped);
    }
  }

  progress_bar_->hide();
  start_button_->setEnabled(true);
  start_button_->setText(tr("Start"));
  start_button_->setProperty("iconName", "play");
  ui::RefreshIcon(start_button_);
  open_output_button_->setVisible(!last_output_dir_.isEmpty());

  RebuildTable();
  RefreshSummary();
}

void ProcessPage::SetBusy(bool is_busy) {
  is_busy_ = is_busy;
  check_button_->setEnabled(!is_busy && !files_.isEmpty());
  retry_button_->setEnabled(!is_busy);
  if (!is_running_) {
    start_button_->setEnabled(!is_busy && !files_.isEmpty());
  }
}

void ProcessPage::RefreshLibraryStatus(int num_dirs) {
  library_title_label_->setText(tr("Font library · %1 folders · %2 fonts")
                                    .arg(num_dirs)
                                    .arg(font_db_->FileCount()));
  if (font_db_->IsEmpty()) {
    library_detail_label_->setText(
        tr("No library yet. Add your font folders in Font library."));
  } else {
    library_detail_label_->setText(
        tr("Updated %1").arg(RelativeTime(font_db_->LastUpdated())));
  }
  RebuildMissing();
}

bool ProcessPage::IsAutoUpdate() const {
  return auto_update_switch_->isChecked();
}

void ProcessPage::SetAutoUpdate(bool is_auto_update) {
  QSignalBlocker blocker(auto_update_switch_);
  auto_update_switch_->setChecked(is_auto_update);
}

void ProcessPage::SaveSettings() {
  settings_->beginGroup("Process");
  settings_->setValue("OutputMode", output_segment_->CurrentIndex());
  settings_->setValue("OutputDir", output_dir_line_->text().trimmed());
  settings_->setValue("Mode", mode_segment_->CurrentIndex());
  settings_->setValue("Hdr", hdr_segment_->CurrentIndex());
  settings_->setValue("Rename", rename_switch_->isChecked());
  settings_->setValue("Combined", combined_switch_->isChecked());
  settings_->endGroup();
}

void ProcessPage::RefreshTheme() {
  ui::RefreshIcons(this);
  update();
}

void ProcessPage::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls() && !is_running_) {
    event->acceptProposedAction();
    drop_zone_->setProperty("active", true);
    ui::Repolish(drop_zone_);
  }
}

void ProcessPage::dragLeaveEvent(QDragLeaveEvent*) {
  drop_zone_->setProperty("active", false);
  ui::Repolish(drop_zone_);
}

void ProcessPage::dropEvent(QDropEvent* event) {
  drop_zone_->setProperty("active", false);
  ui::Repolish(drop_zone_);

  QStringList paths;
  for (const auto& url : event->mimeData()->urls()) {
    if (url.isLocalFile()) {
      paths.append(url.toLocalFile());
    }
  }
  AddPaths(paths);
  event->acceptProposedAction();
}

int ProcessPage::IndexOf(const QString& path) const {
  const QString native = QDir::toNativeSeparators(path);
  for (int idx = 0; idx < files_.size(); ++idx) {
#ifdef _WIN32
    if (files_.at(idx).path.compare(native, Qt::CaseInsensitive) == 0) {
#else
    if (files_.at(idx).path == native) {
#endif
      return idx;
    }
  }
  return -1;
}

QString ProcessPage::OutputDirFor(const QString& input) const {
  if (output_segment_->CurrentIndex() == 1) {
    return output_dir_line_->text().trimmed();
  }
  return QDir::toNativeSeparators(QFileInfo(input).absolutePath());
}

void ProcessPage::SetState(int row, State state) {
  if (row < 0 || row >= files_.size()) {
    return;
  }
  files_[row].state = state;
  RefreshRow(row);
}

void ProcessPage::RefreshRow(int row) {
  if (row < 0 || row >= table_->rowCount()) {
    return;
  }

  const FileItem& file = files_.at(row);
  auto* chip = qobject_cast<QLabel*>(
      table_->cellWidget(row, kStatus)->findChild<QLabel*>());

  table_->item(row, kFonts)->setText(
      file.num_fonts >= 0 ? QString::number(file.num_fonts) : "–");

  QString tooltip = file.error;
  switch (file.state) {
    case State::kPending:
      ui::SetChip(chip, tr("Not checked"), "neutral");
      break;
    case State::kChecking:
      ui::SetChip(chip, tr("Checking…"), "neutral");
      break;
    case State::kReady:
      ui::SetChip(chip, tr("All fonts found"), "ok");
      break;
    case State::kMissing:
      ui::SetChip(chip, tr("%n font(s) missing", "", file.missing.size()),
                  "warn");
      break;
    case State::kUnreadable:
      ui::SetChip(chip, tr("Cannot read"), "error");
      break;
    case State::kRunning:
      ui::SetChip(chip, tr("Processing…"), "info");
      break;
    case State::kDone:
      ui::SetChip(chip, tr("Done"), "ok");
      break;
    case State::kFailed:
      if (!file.missing.isEmpty()) {
        ui::SetChip(chip,
                    tr("%n font(s) missing, not written", "",
                       file.missing.size()),
                    "warn");
      } else {
        ui::SetChip(chip, tr("Failed"), "error");
      }
      break;
    case State::kSkipped:
      ui::SetChip(chip, tr("Skipped"), "neutral");
      break;
  }
  chip->setToolTip(tooltip);
  table_->cellWidget(row, kName)->setToolTip(
      tooltip.isEmpty() ? file.path : file.path + "\n" + tooltip);
}

void ProcessPage::RebuildTable() {
  table_->setRowCount(0);
  table_->setRowCount(files_.size());

  for (int row = 0; row < files_.size(); ++row) {
    const QFileInfo info(files_.at(row).path);

    // File name with its folder underneath.
    auto* name_cell = new QWidget;
    auto* name_layout = new QVBoxLayout(name_cell);
    name_layout->setContentsMargins(8, 4, 8, 4);
    name_layout->setSpacing(0);
    auto* name_label = new ElidedLabel(info.fileName());
    QFont name_font = name_label->font();
    name_font.setWeight(QFont::Medium);
    name_label->setFont(name_font);
    auto* folder_label =
        new ElidedLabel(QDir::toNativeSeparators(info.absolutePath()));
    folder_label->setProperty("role", "caption");
    name_layout->addWidget(name_label);
    name_layout->addWidget(folder_label);
    table_->setCellWidget(row, kName, name_cell);

    table_->setItem(row, kFonts, new QTableWidgetItem("–"));

    auto* chip_cell = new QWidget;
    auto* chip_layout = new QHBoxLayout(chip_cell);
    chip_layout->setContentsMargins(8, 0, 8, 0);
    chip_layout->addWidget(ui::Chip(QString(), "neutral"));
    chip_layout->addStretch();
    table_->setCellWidget(row, kStatus, chip_cell);

    auto* remove_button = ui::Button(QString(), "ghost", "x");
    remove_button->setToolTip(tr("Remove"));
    remove_button->setAccessibleName(tr("Remove %1").arg(info.fileName()));
    remove_button->setEnabled(!is_running_);
    auto* remove_cell = new QWidget;
    auto* remove_layout = new QHBoxLayout(remove_cell);
    remove_layout->setContentsMargins(4, 0, 4, 0);
    remove_layout->addWidget(remove_button);
    table_->setCellWidget(row, kRemove, remove_cell);

    const QString path = files_.at(row).path;
    connect(remove_button, &QPushButton::clicked, this,
            [this, path]() { RemoveRow(IndexOf(path)); });

    RefreshRow(row);
  }

  const bool is_empty = files_.isEmpty();
  table_->setVisible(!is_empty);
  file_count_label_->setText(QString::number(files_.size()));
  drop_zone_->setText(
      is_empty ? tr("Drop .ass / .ssa files or whole folders here\n"
                    "A whole season can be processed at once")
               : tr("Drop more files or folders here"));
  drop_zone_->setMinimumHeight(is_empty ? 160 : 0);
}

void ProcessPage::RemoveRow(int row) {
  if (row < 0 || is_running_) {
    return;
  }
  files_.removeAt(row);
  RebuildTable();
  RebuildMissing();
  RefreshSummary();
}

void ProcessPage::RebuildMissing() {
  missing_fonts_.clear();
  glyph_issues_.clear();

  auto same = [](const FontKey& a, const FontKey& b) {
    return a.name.compare(b.name, Qt::CaseInsensitive) == 0 &&
           a.bold == b.bold && a.italic == b.italic;
  };

  for (const auto& file : files_) {
    const QString file_name = QFileInfo(file.path).fileName();
    for (const auto& use : file.missing) {
      auto it = std::find_if(
          missing_fonts_.begin(), missing_fonts_.end(),
          [&](const MissingFont& font) { return same(font.key, use.key); });
      if (it == missing_fonts_.end()) {
        missing_fonts_.append({use.key, {file_name}, use.num_chars});
      } else {
        it->files.append(file_name);
        it->num_chars += use.num_chars;
      }
    }
    for (const auto& use : file.glyph_issues) {
      auto it = std::find_if(
          glyph_issues_.begin(), glyph_issues_.end(),
          [&](const GlyphIssue& issue) { return same(issue.key, use.key); });
      if (it == glyph_issues_.end()) {
        GlyphIssue issue{use.key, {file_name}, {}};
        for (uint cp : use.missing_codepoints) {
          issue.codepoints.insert(cp);
        }
        glyph_issues_.append(issue);
      } else {
        it->files.append(file_name);
        for (uint cp : use.missing_codepoints) {
          it->codepoints.insert(cp);
        }
      }
    }
  }

  // Clear the old list.
  while (QLayoutItem* item = missing_list_layout_->takeAt(0)) {
    if (item->widget() != nullptr) {
      item->widget()->deleteLater();
    }
    delete item;
  }

  for (const auto& font : missing_fonts_) {
    auto* box = new QWidget;
    auto* vlayout = new QVBoxLayout(box);
    vlayout->setContentsMargins(16, 14, 16, 14);
    vlayout->setSpacing(6);

    auto* name_label = ui::Label(font.key.name, "h3");
    name_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    name_label->setWordWrap(true);
    vlayout->addWidget(name_label);

    auto* usage_row = new QHBoxLayout;
    usage_row->setSpacing(8);
    usage_row->addWidget(ui::Chip(StyleText(font.key), "neutral"));
    auto* usage_label = ui::Label(tr("Used in %1 · %n character(s)", "",
                                     static_cast<int>(font.num_chars))
                                      .arg(FileList(font.files)),
                                  "caption");
    usage_row->addWidget(usage_label, 1);
    vlayout->addLayout(usage_row);

    const QStringList similar = font_db_->Similar(font.key.name, 3);
    if (!similar.isEmpty()) {
      auto* hint = ui::Label(
          tr("Similar fonts in your library: %1").arg(similar.join(tr(", "))),
          "hint");
      hint->setWordWrap(true);
      hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
      vlayout->addWidget(hint);
    }

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    auto* copy_button = ui::Button(tr("Copy name"));
    auto* web_button = ui::Button(tr("Search the web"));
    auto* library_button = ui::Button(tr("Find in library"));
    buttons->addWidget(copy_button);
    buttons->addWidget(web_button);
    buttons->addWidget(library_button);
    buttons->addStretch();
    vlayout->addLayout(buttons);

    const QString name = font.key.name;
    connect(copy_button, &QPushButton::clicked, this,
            [name]() { QGuiApplication::clipboard()->setText(name); });
    connect(web_button, &QPushButton::clicked, this, [name]() {
      QUrl url("https://www.bing.com/search");
      QUrlQuery query;
      query.addQueryItem("q", QString("\"%1\" %2").arg(name, tr("font")));
      url.setQuery(query);
      QDesktopServices::openUrl(url);
    });
    connect(library_button, &QPushButton::clicked, this,
            [this, name]() { emit RequestShowLibrary(name); });

    missing_list_layout_->addWidget(box);
    missing_list_layout_->addWidget(ui::Divider());
  }

  if (!glyph_issues_.isEmpty()) {
    auto* box = new QWidget;
    auto* vlayout = new QVBoxLayout(box);
    vlayout->setContentsMargins(16, 14, 16, 14);
    vlayout->setSpacing(6);
    vlayout->addWidget(ui::Label(tr("Missing characters"), "label"));
    vlayout->addWidget(ui::Label(
        tr("These fonts were found but lack some characters. Players may show "
           "boxes for them; processing still works."),
        "caption"));

    for (const auto& issue : glyph_issues_) {
      QList<uint> codepoints = issue.codepoints.values();
      std::sort(codepoints.begin(), codepoints.end());
      QStringList chars;
      for (int idx = 0; idx < codepoints.size() && idx < 12; ++idx) {
        const char32_t cp = codepoints.at(idx);
        chars.append(QString("%1 U+%2")
                         .arg(QString::fromUcs4(&cp, 1))
                         .arg(codepoints.at(idx), 4, 16, QChar('0'))
                         .toUpper());
      }
      if (codepoints.size() > 12) {
        chars.append(tr("… %1 more").arg(codepoints.size() - 12));
      }
      auto* line = ui::Label(QString("<b>%1</b> (%2) · %3<br>%4")
                                 .arg(issue.key.name.toHtmlEscaped(),
                                      StyleText(issue.key),
                                      FileList(issue.files).toHtmlEscaped(),
                                      chars.join("  ").toHtmlEscaped()));
      line->setWordWrap(true);
      line->setTextInteractionFlags(Qt::TextSelectableByMouse);
      vlayout->addWidget(line);
    }
    missing_list_layout_->addWidget(box);
  }

  missing_list_layout_->addStretch();

  int num_files_missing = 0;
  for (const auto& file : files_) {
    if (!file.missing.isEmpty()) {
      ++num_files_missing;
    }
  }

  missing_count_label_->setText(QString::number(missing_fonts_.size()));
  retry_button_->setText(tr("Update library and retry %n file(s)", "",
                            num_files_missing));
  retry_button_->setVisible(num_files_missing > 0);
  missing_card_->setVisible(!missing_fonts_.isEmpty() ||
                            !glyph_issues_.isEmpty());
}

void ProcessPage::RefreshSummary() {
  const bool is_empty = files_.isEmpty();
  check_button_->setEnabled(!is_empty && !is_busy_ && !is_running_);
  if (!is_running_) {
    start_button_->setEnabled(!is_empty && !is_busy_);
  }

  if (is_empty) {
    summary_label_->setText(tr("Add subtitles to get started."));
    return;
  }

  if (is_running_) {
    summary_label_->setText(
        tr("Processing %1 of %2…").arg(num_finished_ + 1 > num_to_run_
                                            ? num_to_run_
                                            : num_finished_ + 1)
                                  .arg(num_to_run_));
    return;
  }

  int num_done = 0;
  int num_failed = 0;
  int num_missing = 0;
  bool has_run = false;
  for (const auto& file : files_) {
    if (!file.missing.isEmpty()) {
      ++num_missing;
    }
    if (file.state == State::kDone) {
      ++num_done;
      has_run = true;
    } else if (file.state == State::kFailed) {
      has_run = true;
      if (file.missing.isEmpty()) {
        ++num_failed;
      }
    }
  }

  if (has_run) {
    summary_label_->setText(
        tr("Finished: %1 succeeded · %2 missing fonts · %3 failed")
            .arg(num_done)
            .arg(num_missing)
            .arg(num_failed));
  } else if (num_missing > 0) {
    summary_label_->setText(
        tr("%n file(s)", "", files_.size()) + " · " +
        tr("%n file(s) are missing fonts; add them first or process the rest",
           "", num_missing));
  } else {
    summary_label_->setText(tr("%n file(s)", "", files_.size()));
  }
}

void ProcessPage::Start(const QStringList& inputs) {
  if (inputs.isEmpty() || is_busy_ || is_running_) {
    return;
  }

  if (output_segment_->CurrentIndex() == 1) {
    const QString dir = output_dir_line_->text().trimmed();
    if (dir.isEmpty()) {
      output_dir_line_->setFocus();
      summary_label_->setText(tr("Choose an output folder first."));
      return;
    }
    QDir().mkpath(dir);
  }

  QStringList outputs;
  for (const auto& input : inputs) {
    outputs.append(OutputDirFor(input));
    const int row = IndexOf(input);
    if (row >= 0) {
      files_[row].state = State::kPending;
      files_[row].error.clear();
    }
  }
  last_output_dir_ = outputs.isEmpty() ? QString() : outputs.first();

  is_running_ = true;
  num_finished_ = 0;
  num_to_run_ = inputs.size();
  progress_bar_->setRange(0, num_to_run_);
  progress_bar_->setValue(0);
  progress_bar_->show();
  open_output_button_->hide();

  start_button_->setText(tr("Stop"));
  start_button_->setProperty("iconName", "stop");
  ui::RefreshIcon(start_button_);
  start_button_->setEnabled(true);

  RebuildTable();
  RefreshSummary();

  emit RequestStart(inputs, outputs, Options());
}

QVariantMap ProcessPage::Options() const {
  QVariantMap options;

  switch (hdr_segment_->CurrentIndex()) {
    case 1:
      options["brightness"] = 100;
      break;
    case 2:
      options["brightness"] = 203;
      break;
    default:
      options["brightness"] = 0;
      break;
  }

  options["subset_only"] = mode_segment_->CurrentIndex() == 1;
  options["embed_only"] = mode_segment_->CurrentIndex() == 2;
  options["rename"] = rename_switch_->isChecked();
  options["combined"] = combined_switch_->isChecked();

  return options;
}

QString ProcessPage::StyleText(const FontKey& key) {
  return LogTranslator::FontStyle(key.bold, key.italic);
}

QString ProcessPage::FileList(const QStringList& files) {
  if (files.size() <= 2) {
    return files.join(tr(", "));
  }
  return tr("%1, %2 and %n more", "", files.size() - 2)
      .arg(files.at(0), files.at(1));
}
