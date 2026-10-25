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

#include "library_page.h"

#include <algorithm>

#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QScrollArea>
#include <QtConcurrent>

#include "duplicates_dialog.h"
#include "log_translator.h"
#include "theme.h"

namespace {

QWidget* StatTile(const QString& title, QLabel** value) {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 14, 2);
  layout->addWidget(ui::Label(title, "caption"));
  *value = ui::Label("–", "stat");
  layout->addWidget(*value);
  return card;
}

QString PathKey(const QString& path) {
  QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef _WIN32
  key = key.toLower();
#endif
  return key;
}

}  // namespace

LibraryPage::LibraryPage(QSettings* settings, const FontDatabase* font_db,
                         QWidget* parent)
    : QWidget(parent), settings_(settings), font_db_(font_db) {
  setObjectName("page");
  setAcceptDrops(true);

  dirs_ = settings_->value("Library/Dirs").toStringList();

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
  layout->addWidget(InitStats());
  layout->addWidget(InitDirsCard());
  layout->addWidget(InitDupesCard());
  layout->addWidget(InitSearchCard());

  db_path_label_ = ui::Label(QString(), "caption");
  db_path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(db_path_label_);
  layout->addStretch();

  auto_update_switch_->setChecked(
      settings_->value("Library/AutoUpdate", true).toBool());
  check_on_start_switch_->setChecked(
      settings_->value("Library/CheckOnStart", true).toBool());

  RebuildDirs();
}

QWidget* LibraryPage::InitHeader() {
  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(8);

  auto* titles = new QVBoxLayout;
  titles->setSpacing(2);
  titles->addWidget(ui::Label(tr("Font library"), "h1"));
  titles->addWidget(ui::Label(
      tr("Add your font folders once. Updates only read new or changed "
         "font files, so they take seconds."),
      "muted"));
  hlayout->addLayout(titles, 1);

  busy_bar_ = new QProgressBar;
  busy_bar_->setRange(0, 0);
  busy_bar_->setTextVisible(false);
  busy_bar_->setFixedWidth(120);
  busy_bar_->hide();
  hlayout->addWidget(busy_bar_, 0, Qt::AlignBottom | Qt::AlignVCenter);

  rebuild_button_ = ui::Button(tr("Rebuild from scratch"));
  rebuild_button_->setToolTip(
      tr("Forget everything and read all font files again"));
  rebuild_button_->setProperty("tall", true);
  update_button_ = ui::Button(tr("Update library"), "primary", "refresh");
  hlayout->addWidget(rebuild_button_, 0, Qt::AlignBottom);
  hlayout->addWidget(update_button_, 0, Qt::AlignBottom);

  connect(rebuild_button_, &QPushButton::clicked, this,
          [this]() { emit RequestBuild(true); });
  connect(update_button_, &QPushButton::clicked, this,
          [this]() { emit RequestBuild(false); });

  return header;
}

QWidget* LibraryPage::InitStats() {
  auto* row = new QWidget;
  auto* hlayout = new QHBoxLayout(row);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(12);
  hlayout->addWidget(StatTile(tr("Font folders"), &stat_dirs_label_));
  hlayout->addWidget(StatTile(tr("Font files"), &stat_files_label_));
  hlayout->addWidget(StatTile(tr("Font names"), &stat_families_label_));
  hlayout->addWidget(StatTile(tr("Last updated"), &stat_updated_label_));
  return row;
}

QWidget* LibraryPage::InitDirsCard() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(16, 12, 12, 12);
  hlayout->addWidget(ui::Label(tr("Font folders"), "h2"));
  hlayout->addStretch();
  auto* add_button = ui::Button(tr("Add folder"), QString(), "plus");
  hlayout->addWidget(add_button);
  layout->addWidget(header);
  layout->addWidget(ui::Divider());

  auto* dirs_widget = new QWidget;
  dirs_layout_ = new QVBoxLayout(dirs_widget);
  dirs_layout_->setContentsMargins(0, 0, 0, 0);
  dirs_layout_->setSpacing(0);
  layout->addWidget(dirs_widget);

  // System fonts are always read by the library.
  auto* system_row = new QWidget;
  auto* srow = new QHBoxLayout(system_row);
  srow->setContentsMargins(16, 10, 16, 10);
  srow->setSpacing(12);
  srow->addWidget(ui::IconLabel("monitor", 20));
  auto* stexts = new QVBoxLayout;
  stexts->setSpacing(0);
  stexts->addWidget(ui::Label(tr("System fonts"), "label"));
  system_dirs_label_ = ui::Label(
      FontDatabase::SystemFontDirs().join("  ·  "), "mono");
  stexts->addWidget(system_dirs_label_);
  srow->addLayout(stexts, 1);
  srow->addWidget(ui::Chip(tr("Always included"), "neutral"));
  layout->addWidget(system_row);

  auto* drop_wrap = new QWidget;
  auto* drop_layout = new QVBoxLayout(drop_wrap);
  drop_layout->setContentsMargins(16, 4, 16, 14);
  auto* drop_zone = new QLabel(tr("Drop font folders here to add them"));
  drop_zone->setObjectName("dropZone");
  drop_zone->setAlignment(Qt::AlignCenter);
  drop_layout->addWidget(drop_zone);
  layout->addWidget(drop_wrap);

  auto* footer = new QFrame;
  footer->setProperty("cardFooter", true);
  auto* flayout = new QHBoxLayout(footer);
  flayout->setContentsMargins(16, 12, 16, 12);
  flayout->setSpacing(28);
  flayout->addWidget(ui::SwitchRow(&auto_update_switch_,
                                   tr("Update before processing subtitles")));
  flayout->addWidget(ui::SwitchRow(&check_on_start_switch_,
                                   tr("Look for changes on startup")));
  flayout->addStretch();
  layout->addWidget(footer);

  connect(add_button, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose a font folder"),
        dirs_.isEmpty() ? QDir::homePath() : dirs_.last());
    if (!path.isEmpty()) {
      AddDirs({path});
    }
  });

  connect(auto_update_switch_, &ToggleSwitch::toggled, this,
          &LibraryPage::AutoUpdateChanged);

  return card;
}

QWidget* LibraryPage::InitDupesCard() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 16, 8);

  auto* header = new QHBoxLayout;
  header->setSpacing(8);
  header->addWidget(ui::Label(tr("Duplicates and name conflicts"), "h2"));
  dupes_chips_layout_ = new QHBoxLayout;
  dupes_chips_layout_->setSpacing(6);
  header->addLayout(dupes_chips_layout_);
  header->addStretch();
  dupes_button_ = ui::Button(tr("View report"));
  header->addWidget(dupes_button_);
  layout->addLayout(header);

  dupes_summary_label_ = ui::Label(QString(), "caption");
  dupes_summary_label_->setWordWrap(true);
  layout->addWidget(dupes_summary_label_);

  connect(dupes_button_, &QPushButton::clicked, this, [this]() {
    DuplicatesDialog dialog(font_db_, dupes_report_, dirs_, settings_, this);
    dialog.exec();
    if (dialog.HasMoved()) {
      emit RequestBuild(false);
    }
  });

  return card;
}

QWidget* LibraryPage::InitSearchCard() {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(16, 12, 16, 12);
  hlayout->setSpacing(12);
  hlayout->addWidget(ui::Label(tr("Find a font"), "h2"));

  search_line_ = new QLineEdit;
  search_line_->setPlaceholderText(
      tr("Font name, PostScript name or file name"));
  search_line_->setClearButtonEnabled(true);
  search_line_->addAction(theme::Icon("search", theme::Current().text_muted, 16),
                          QLineEdit::LeadingPosition);
  search_line_->setAccessibleName(tr("Find a font"));
  hlayout->addWidget(search_line_, 1);

  search_count_label_ = ui::Label(QString(), "caption");
  hlayout->addWidget(search_count_label_);
  layout->addWidget(header);
  layout->addWidget(ui::Divider());

  search_table_ = new QTableWidget(0, 4);
  search_table_->setHorizontalHeaderLabels(
      {tr("Font name"), tr("Weight / style"), tr("PostScript name"),
       tr("File")});
  search_table_->verticalHeader()->hide();
  search_table_->verticalHeader()->setDefaultSectionSize(34);
  search_table_->setShowGrid(false);
  search_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  search_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  search_table_->setTextElideMode(Qt::ElideMiddle);
  search_table_->setWordWrap(false);
  search_table_->setMinimumHeight(200);
  QHeaderView* header_view = search_table_->horizontalHeader();
  header_view->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  header_view->setSectionResizeMode(0, QHeaderView::Stretch);
  header_view->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  header_view->setSectionResizeMode(2, QHeaderView::Stretch);
  header_view->setSectionResizeMode(3, QHeaderView::Stretch);
  search_table_->hide();
  layout->addWidget(search_table_);

  connect(search_line_, &QLineEdit::textChanged, this,
          [this]() { RunSearch(); });

  return card;
}

void LibraryPage::AddDirs(const QStringList& dirs) {
  bool is_changed = false;
  for (const auto& dir : dirs) {
    if (!QFileInfo(dir).isDir()) {
      continue;
    }
    const QString native = QDir::toNativeSeparators(QDir(dir).absolutePath());
    const bool exists =
        std::any_of(dirs_.begin(), dirs_.end(), [&](const QString& d) {
          return PathKey(d) == PathKey(native);
        });
    if (!exists) {
      dirs_.append(native);
      is_changed = true;
    }
  }

  if (is_changed) {
    SaveSettings();
    RebuildDirs();
    emit DirsChanged();
  }
}

void LibraryPage::RemoveDir(const QString& dir) {
  dirs_.removeAll(dir);
  SaveSettings();
  RebuildDirs();
  emit DirsChanged();
}

void LibraryPage::Refresh() {
  stat_dirs_label_->setText(QString::number(dirs_.size()));
  stat_files_label_->setText(QLocale().toString(font_db_->FileCount()));
  stat_families_label_->setText(QLocale().toString(font_db_->FamilyCount()));
  stat_updated_label_->setText(
      font_db_->LastUpdated().isValid()
          ? QLocale().toString(font_db_->LastUpdated(), QLocale::ShortFormat)
          : tr("Never"));

  db_path_label_->setText(
      tr("The library is saved in %1")
          .arg(QDir::toNativeSeparators(FontDatabase::DbFile(
              settings_->value("Library/Database").toString()))));

  RebuildDirs();
  RunSearch();
  RefreshDupes();
}

void LibraryPage::RefreshDupes() {
  using duplicates::Kind;

  dupes_report_ =
      duplicates::Find(font_db_->Fonts(), FontDatabase::SystemFontDirs());

  while (QLayoutItem* item = dupes_chips_layout_->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  QHash<int, int> counts;
  for (const auto& group : dupes_report_.groups) {
    ++counts[static_cast<int>(group.kind)];
  }
  const QVector<QPair<Kind, QPair<QString, QString>>> chips = {
      {Kind::kConflict, {tr("%n conflict(s)", "", counts.value(0)), "error"}},
      {Kind::kVersions, {tr("%n with versions", "", counts.value(1)), "warn"}},
      {Kind::kRepackaged, {tr("%n renamed", "", counts.value(2)), "info"}},
      {Kind::kIdentical,
       {tr("%n identical", "", counts.value(3)), "neutral"}}};
  for (const auto& chip : chips) {
    if (counts.value(static_cast<int>(chip.first)) > 0) {
      dupes_chips_layout_->addWidget(
          ui::Chip(chip.second.first, chip.second.second));
    }
  }

  QString summary;
  if (font_db_->IsEmpty()) {
    summary = tr("Checked after the library is built.");
  } else if (dupes_report_.groups.isEmpty()) {
    summary = tr("No duplicates or name conflicts found.");
  } else {
    summary = tr("Name conflicts can make a subtitle get the wrong font. "
                 "Identical copies can be moved out of the library.");
  }
  if (dupes_report_.num_unchecked > 0) {
    summary += " " + tr("%n font(s) were read by an older version and are "
                        "not checked. Update the library to include them.",
                        "", dupes_report_.num_unchecked);
  }
  dupes_summary_label_->setText(summary);
  dupes_button_->setEnabled(!dupes_report_.groups.isEmpty());
}

void LibraryPage::SetBusy(bool is_busy) {
  update_button_->setEnabled(!is_busy);
  rebuild_button_->setEnabled(!is_busy);
  busy_bar_->setVisible(is_busy);
}

void LibraryPage::ShowSearch(const QString& text) {
  search_line_->setText(text);
  search_line_->setFocus();
}

bool LibraryPage::IsAutoUpdate() const {
  return auto_update_switch_->isChecked();
}

void LibraryPage::SetAutoUpdate(bool is_auto_update) {
  QSignalBlocker blocker(auto_update_switch_);
  auto_update_switch_->setChecked(is_auto_update);
}

void LibraryPage::SaveSettings() {
  settings_->setValue("Library/Dirs", dirs_);
  settings_->setValue("Library/AutoUpdate", auto_update_switch_->isChecked());
  settings_->setValue("Library/CheckOnStart",
                      check_on_start_switch_->isChecked());
}

void LibraryPage::RefreshTheme() {
  ui::RefreshIcons(this);
  search_line_->actions().first()->setIcon(
      theme::Icon("search", theme::Current().text_muted, 16));
}

void LibraryPage::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void LibraryPage::dropEvent(QDropEvent* event) {
  QStringList dirs;
  for (const auto& url : event->mimeData()->urls()) {
    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
      dirs.append(url.toLocalFile());
    }
  }
  AddDirs(dirs);
  event->acceptProposedAction();
}

void LibraryPage::RebuildDirs() {
  while (QLayoutItem* item = dirs_layout_->takeAt(0)) {
    if (item->widget() != nullptr) {
      item->widget()->deleteLater();
    }
    delete item;
  }

  stat_dirs_label_->setText(QString::number(dirs_.size()));

  ++scan_generation_;
  const int generation = scan_generation_;
  num_scans_pending_ = dirs_.size();
  has_changes_ = false;

  // The scan runs on a copy, so a reload of the database can't race it.
  const FontDatabase snapshot = *font_db_;

  for (const auto& dir : dirs_) {
    auto* row = new QWidget;
    auto* hlayout = new QHBoxLayout(row);
    hlayout->setContentsMargins(16, 10, 8, 10);
    hlayout->setSpacing(12);
    hlayout->addWidget(ui::IconLabel("folder", 20));

    auto* texts = new QVBoxLayout;
    texts->setSpacing(0);
    auto* path_label = ui::Label(dir);
    path_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path_label->setWordWrap(true);
    auto* count_label = ui::Label(tr("Counting font files…"), "caption");
    texts->addWidget(path_label);
    texts->addWidget(count_label);
    hlayout->addLayout(texts, 1);

    auto* chip = ui::Chip(tr("Scanning…"), "neutral");
    hlayout->addWidget(chip);

    auto* remove_button = ui::Button(QString(), "ghost", "trash");
    remove_button->setToolTip(tr("Remove from library"));
    remove_button->setAccessibleName(tr("Remove %1 from library").arg(dir));
    hlayout->addWidget(remove_button);

    dirs_layout_->addWidget(row);
    dirs_layout_->addWidget(ui::Divider());

    connect(remove_button, &QPushButton::clicked, this,
            [this, dir]() { RemoveDir(dir); });

    if (!QFileInfo(dir).isDir()) {
      count_label->setText(tr("The folder can't be found"));
      ui::SetChip(chip, tr("Not found"), "error");
      --num_scans_pending_;
      continue;
    }

    auto* watcher = new QFutureWatcher<FontDatabase::DirStats>(this);
    connect(watcher, &QFutureWatcher<FontDatabase::DirStats>::finished, this,
            [this, watcher, generation, count_label, chip]() {
              watcher->deleteLater();
              if (generation != scan_generation_) {
                return;
              }
              const auto stats = watcher->result();
              count_label->setText(
                  tr("%n font file(s), including subfolders", "",
                     stats.num_files));
              if (stats.num_new > 0) {
                ui::SetChip(chip, tr("%n new file(s)", "", stats.num_new),
                            "info");
                has_changes_ = true;
              } else if (stats.num_removed > 0) {
                ui::SetChip(chip,
                            tr("%n file(s) removed", "", stats.num_removed),
                            "info");
                has_changes_ = true;
              } else {
                ui::SetChip(chip, tr("Up to date"), "ok");
              }
              if (--num_scans_pending_ == 0) {
                emit ChangesScanned(has_changes_);
              }
            });
    watcher->setFuture(QtConcurrent::run(
        [snapshot, dir]() { return snapshot.ScanDir(dir); }));
  }

  if (dirs_.isEmpty()) {
    auto* empty = ui::Label(
        tr("No font folders yet. Add the folders where you keep fonts for "
           "subtitles."),
        "muted");
    empty->setContentsMargins(16, 14, 16, 14);
    dirs_layout_->addWidget(empty);
    dirs_layout_->addWidget(ui::Divider());
  }
}

void LibraryPage::RunSearch() {
  const QString text = search_line_->text().trimmed();
  if (text.isEmpty()) {
    search_table_->hide();
    search_count_label_->clear();
    return;
  }

  constexpr int kLimit = 200;
  const auto fonts = font_db_->Search(text, kLimit);

  search_table_->setRowCount(fonts.size());
  for (int row = 0; row < fonts.size(); ++row) {
    const auto& font = fonts.at(row);
    QString name = font.families.isEmpty() ? QString() : font.families.first();
    if (font.families.size() > 1) {
      name += "  (" + font.families.mid(1).join(", ") + ")";
    }
    search_table_->setItem(row, 0, new QTableWidgetItem(name));
    search_table_->setItem(
        row, 1,
        new QTableWidgetItem(QString("%1 · %2").arg(
            LogTranslator::FontStyle(font.weight, font.slant != 0 ? 1 : 0))
                                 .arg(font.weight)));
    search_table_->setItem(
        row, 2,
        new QTableWidgetItem(font.psnames.isEmpty() ? QString()
                                                    : font.psnames.first()));
    auto* file_item = new QTableWidgetItem(font.path);
    file_item->setToolTip(font.path);
    file_item->setForeground(theme::Current().text_muted);
    search_table_->setItem(row, 3, file_item);
  }

  search_table_->setVisible(!fonts.isEmpty());
  search_count_label_->setText(
      fonts.size() >= kLimit
          ? tr("First %1 matches").arg(kLimit)
          : tr("%n match(es)", "", fonts.size()));
}
