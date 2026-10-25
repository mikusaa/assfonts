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

#include "duplicates_dialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

#include "log_translator.h"

namespace {

using duplicates::Kind;

constexpr Kind kKinds[] = {Kind::kConflict, Kind::kVersions,
                           Kind::kRepackaged, Kind::kIdentical};

QString KindName(Kind kind) {
  switch (kind) {
    case Kind::kConflict:
      return DuplicatesDialog::tr("Name conflict");
    case Kind::kVersions:
      return DuplicatesDialog::tr("Versions");
    case Kind::kRepackaged:
      return DuplicatesDialog::tr("Renamed copy");
    case Kind::kIdentical:
      return DuplicatesDialog::tr("Identical");
  }
  return QString();
}

QString KindChip(Kind kind) {
  switch (kind) {
    case Kind::kConflict:
      return "error";
    case Kind::kVersions:
      return "warn";
    case Kind::kRepackaged:
      return "info";
    case Kind::kIdentical:
      return "neutral";
  }
  return "neutral";
}

QString KindHelp(Kind kind) {
  switch (kind) {
    case Kind::kConflict:
      return DuplicatesDialog::tr(
          "Different fonts answer to the same name. A subtitle asking for "
          "it may get either one. Keep the right one in the library.");
    case Kind::kVersions:
      return DuplicatesDialog::tr(
          "Releases of the same font. A subtitle may get the older one; "
          "the newest usually has more characters.");
    case Kind::kRepackaged:
      return DuplicatesDialog::tr(
          "The same glyphs with other names or details. Subtitles may "
          "ask for any of the names, so keep them unless you are sure.");
    case Kind::kIdentical:
      return DuplicatesDialog::tr(
          "The same font in several files. One copy is enough.");
  }
  return QString();
}

void RevealInFolder(const QString& path) {
#ifdef _WIN32
  QProcess::startDetached("explorer.exe",
                          {"/select," + QDir::toNativeSeparators(path)});
#elif __APPLE__
  QProcess::startDetached("open", {"-R", path});
#else
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

}  // namespace

DuplicatesDialog::DuplicatesDialog(const FontDatabase* font_db,
                                   const duplicates::Report& report,
                                   const QStringList& library_dirs,
                                   QSettings* settings, QWidget* parent)
    : QDialog(parent),
      font_db_(font_db),
      library_dirs_(library_dirs),
      settings_(settings) {
  setObjectName("page");
  setWindowTitle(tr("Duplicates and name conflicts"));
  resize(820, 680);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 18, 20, 18);
  layout->setSpacing(12);

  layout->addWidget(ui::Label(tr("Duplicates and name conflicts"), "h2"));

  for (const auto& font : font_db_->Fonts()) {
    ++faces_per_file_[font.path];
  }

  QHash<int, int> counts;
  for (const auto& group : report.groups) {
    ++counts[static_cast<int>(group.kind)];
  }
  QStringList filters = {tr("All %1").arg(report.groups.size())};
  for (Kind kind : kKinds) {
    filters.append(QString("%1 %2").arg(KindName(kind)).arg(
        counts.value(static_cast<int>(kind))));
  }
  filter_segment_ = new SegmentedControl(filters);
  filter_segment_->setAccessibleName(tr("Show"));
  layout->addWidget(filter_segment_, 0, Qt::AlignLeft);

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* list = new QWidget;
  list->setObjectName("scrollContent");
  auto* list_layout = new QVBoxLayout(list);
  list_layout->setContentsMargins(0, 0, 0, 0);
  list_layout->setSpacing(12);
  for (const auto& group : report.groups) {
    QWidget* widget = GroupWidget(group);
    group_widgets_.append({group.kind, widget});
    list_layout->addWidget(widget);
  }
  if (report.groups.isEmpty()) {
    list_layout->addWidget(
        ui::Label(tr("No duplicates or name conflicts found."), "muted"));
  }
  list_layout->addStretch();
  scroll->setWidget(list);
  layout->addWidget(scroll, 1);

  if (report.num_unchecked > 0) {
    auto* unchecked = ui::Label(
        tr("%n font(s) were read by an older version and are not checked. "
           "Update the library to include them.",
           "", report.num_unchecked),
        "hint");
    unchecked->setWordWrap(true);
    layout->addWidget(unchecked);
  }

  auto* footer = new QHBoxLayout;
  footer->setSpacing(8);
  auto* footer_label = ui::Label(
      tr("Picked files are moved to a folder you choose, outside the "
         "library. Fonts in system folders cannot be moved here."),
      "caption");
  footer_label->setWordWrap(true);
  footer->addWidget(footer_label, 1);
  auto* close_button = ui::Button(tr("Close"));
  move_button_ = ui::Button(QString(), "primary");
  footer->addWidget(close_button);
  footer->addWidget(move_button_);
  layout->addLayout(footer);

  connect(filter_segment_, &SegmentedControl::CurrentChanged, this,
          [this]() { ApplyFilter(); });
  connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
  connect(move_button_, &QPushButton::clicked, this,
          &DuplicatesDialog::MoveSelected);

  RefreshButton();
  ui::RefreshIcons(this);
}

QWidget* DuplicatesDialog::GroupWidget(const duplicates::Group& group) {
  const auto& fonts = font_db_->Fonts();
  const QStringList system_dirs = FontDatabase::SystemFontDirs();

  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* header = new QWidget;
  auto* hlayout = new QVBoxLayout(header);
  hlayout->setContentsMargins(16, 12, 16, 12);
  hlayout->setSpacing(4);
  auto* title_row = new QHBoxLayout;
  title_row->setSpacing(8);
  title_row->addWidget(ui::Chip(KindName(group.kind), KindChip(group.kind)));
  auto* title = ui::Label(group.title, "h3");
  title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  title_row->addWidget(title, 1);
  hlayout->addLayout(title_row);
  auto* help = ui::Label(KindHelp(group.kind), "caption");
  help->setWordWrap(true);
  hlayout->addWidget(help);
  if (!group.shared_names.isEmpty()) {
    auto* names = ui::Label(
        tr("Shared names: %1").arg(group.shared_names.join(tr(", "))),
        "caption");
    names->setWordWrap(true);
    names->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hlayout->addWidget(names);
  }
  layout->addWidget(header);

  for (int pos = 0; pos < group.fonts.size(); ++pos) {
    const auto& font = fonts.at(group.fonts.at(pos));
    const bool is_system = duplicates::IsInDirs(font.path, system_dirs);

    layout->addWidget(ui::Divider());
    auto* row = new QWidget;
    auto* rlayout = new QHBoxLayout(row);
    rlayout->setContentsMargins(16, 8, 10, 8);
    rlayout->setSpacing(10);

    auto* check = new QCheckBox;
    check->setEnabled(!is_system);
    check->setChecked(!is_system && group.removable.contains(pos));
    check->setAccessibleName(QFileInfo(font.path).fileName());
    if (is_system) {
      check->setToolTip(tr("In a system font folder"));
    }
    checks_by_path_[font.path].append(check);
    rlayout->addWidget(check, 0, Qt::AlignTop);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto* name_row = new QHBoxLayout;
    name_row->setSpacing(8);
    auto* name = ui::Label(QFileInfo(font.path).fileName(), "label");
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    name_row->addWidget(name);
    if (pos == group.keep) {
      name_row->addWidget(ui::Chip(tr("Keep"), "ok"));
    }
    if (is_system) {
      name_row->addWidget(ui::Chip(tr("System"), "neutral"));
    }
    name_row->addStretch();
    texts->addLayout(name_row);

    QStringList details;
    details.append(LogTranslator::FontStyle(font.weight, font.slant));
    if (!font.fullnames.isEmpty() && group.kind != Kind::kIdentical) {
      details.append(font.fullnames.first());
    }
    if (!font.version.isEmpty()) {
      details.append(font.version);
    }
    if (!font.vendor.isEmpty()) {
      details.append(font.vendor);
    }
    details.append(tr("%n glyph(s)", "", font.num_glyphs));
    if (faces_per_file_.value(font.path) > 1) {
      details.append(
          tr("file holds %n font(s)", "", faces_per_file_.value(font.path)));
    }
    auto* detail_label = ui::Label(details.join("  ·  "), "caption");
    detail_label->setWordWrap(true);
    texts->addWidget(detail_label);
    auto* path_label =
        new ElidedLabel(QDir::toNativeSeparators(font.path));
    path_label->setProperty("role", "mono");
    texts->addWidget(path_label);
    rlayout->addLayout(texts, 1);

    auto* reveal = ui::Button(QString(), "ghost", "folder");
    reveal->setToolTip(tr("Show in folder"));
    reveal->setAccessibleName(tr("Show in folder"));
    rlayout->addWidget(reveal, 0, Qt::AlignTop);

    const QString path = font.path;
    connect(reveal, &QPushButton::clicked, this,
            [path]() { RevealInFolder(path); });
    // A file can show up in several groups; its boxes move together.
    connect(check, &QCheckBox::toggled, this, [this, path](bool checked) {
      for (QCheckBox* other : checks_by_path_.value(path)) {
        const QSignalBlocker blocker(other);
        other->setChecked(checked);
      }
      RefreshButton();
    });

    layout->addWidget(row);
  }

  return card;
}

QStringList DuplicatesDialog::Selected() const {
  QStringList paths;
  for (auto it = checks_by_path_.begin(); it != checks_by_path_.end(); ++it) {
    if (!it.value().isEmpty() && it.value().first()->isChecked()) {
      paths.append(it.key());
    }
  }
  paths.sort();
  return paths;
}

void DuplicatesDialog::ApplyFilter() {
  const int index = filter_segment_->CurrentIndex();
  for (const auto& entry : group_widgets_) {
    entry.second->setVisible(index == 0 || kKinds[index - 1] == entry.first);
  }
}

void DuplicatesDialog::RefreshButton() {
  const int num_files = Selected().size();
  move_button_->setText(tr("Move %n file(s)…", "", num_files));
  move_button_->setEnabled(num_files > 0);
}

void DuplicatesDialog::MoveSelected() {
  const QStringList sources = Selected();
  if (sources.isEmpty()) {
    return;
  }

  const QString target_dir = QFileDialog::getExistingDirectory(
      this, tr("Choose where to move the files"),
      settings_->value("Library/DuplicatesDir", QDir::homePath()).toString());
  if (target_dir.isEmpty()) {
    return;
  }

  // Inside the library the files would just be read again.
  if (duplicates::IsInDirs(target_dir + "/x", library_dirs_) ||
      duplicates::IsInDirs(target_dir + "/x",
                           FontDatabase::SystemFontDirs())) {
    QMessageBox::warning(
        this, windowTitle(),
        tr("Choose a folder outside the font library and the system font "
           "folders."));
    return;
  }
  settings_->setValue("Library/DuplicatesDir", target_dir);

  const QStringList failed = duplicates::MoveFiles(sources, target_dir);
  for (const auto& source : sources) {
    if (failed.contains(source)) {
      continue;
    }
    has_moved_ = true;
    for (QCheckBox* check : checks_by_path_.value(source)) {
      const QSignalBlocker blocker(check);
      check->setChecked(false);
      check->setEnabled(false);
    }
  }

  RefreshButton();

  if (!failed.isEmpty()) {
    QMessageBox::warning(this, windowTitle(),
                         tr("These files could not be moved:\n%1")
                             .arg(QDir::toNativeSeparators(failed.join("\n"))));
  } else {
    accept();
  }
}
