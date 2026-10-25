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

#include "pack_dialog.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "widgets.h"

PackImportDialog::PackImportDialog(const QString& pack_dir,
                                   const QVector<Match>& matches,
                                   const QStringList& not_found,
                                   const QStringList& target_dirs,
                                   const QString& last_target, QWidget* parent)
    : QDialog(parent), matches_(matches) {
  setObjectName("page");
  setWindowTitle(tr("Import from font pack"));
  setMinimumWidth(560);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 18, 20, 18);
  layout->setSpacing(12);

  layout->addWidget(ui::Label(
      tr("%n missing font(s) found in the pack", "", matches.size()), "h2"));
  auto* pack_label = new ElidedLabel(QDir::toNativeSeparators(pack_dir));
  pack_label->setProperty("role", "caption");
  layout->addWidget(pack_label);

  QVBoxLayout* card_layout = nullptr;
  auto* card = ui::Card(&card_layout, 0, 0);

  auto* scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* list = new QWidget;
  list->setObjectName("scrollContent");
  auto* list_layout = new QVBoxLayout(list);
  list_layout->setContentsMargins(0, 0, 0, 0);
  list_layout->setSpacing(0);

  const QDir pack(pack_dir);
  for (int idx = 0; idx < matches_.size(); ++idx) {
    const Match& match = matches_.at(idx);
    if (idx != 0) {
      list_layout->addWidget(ui::Divider());
    }

    auto* row = new QWidget;
    auto* hlayout = new QHBoxLayout(row);
    hlayout->setContentsMargins(14, 10, 14, 10);
    hlayout->setSpacing(10);

    auto* check = new QCheckBox;
    check->setChecked(true);
    check->setAccessibleName(match.name);
    checks_.append(check);
    hlayout->addWidget(check, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* title_row = new QHBoxLayout;
    title_row->setSpacing(8);
    auto* name_label = ui::Label(match.name, "label");
    name_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    title_row->addWidget(name_label);
    title_row->addWidget(ui::Chip(match.style, "neutral"));
    if (match.num_missing_chars > 0) {
      title_row->addWidget(ui::Chip(
          tr("lacks %n character(s)", "", match.num_missing_chars), "warn"));
    }
    title_row->addStretch();
    text->addLayout(title_row);

    auto* path_label = new ElidedLabel(
        QDir::toNativeSeparators(pack.relativeFilePath(match.path)));
    path_label->setProperty("role", "caption");
    text->addWidget(path_label);
    hlayout->addLayout(text, 1);

    connect(check, &QCheckBox::toggled, this, [this]() { RefreshButton(); });
    list_layout->addWidget(row);
  }
  list_layout->addStretch();

  scroll->setWidget(list);
  scroll->setMinimumHeight(
      std::min(8 + 60 * static_cast<int>(matches_.size()), 240));
  scroll->setMaximumHeight(380);
  card_layout->addWidget(scroll);
  layout->addWidget(card, 1);

  if (!not_found.isEmpty()) {
    auto* not_found_label = ui::Label(
        tr("Not in this pack: %1").arg(not_found.join(tr(", "))), "caption");
    not_found_label->setWordWrap(true);
    not_found_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(not_found_label);
  }

  auto* target_row = new QHBoxLayout;
  target_row->setSpacing(10);
  target_row->addWidget(ui::Label(tr("Copy to"), "label"));
  target_combo_ = new QComboBox;
  for (const auto& dir : target_dirs) {
    target_combo_->addItem(QDir::toNativeSeparators(dir), dir);
  }
  const int last_index = target_combo_->findData(last_target);
  target_combo_->setCurrentIndex(last_index >= 0 ? last_index : 0);
  target_combo_->setAccessibleName(tr("Copy to"));
  target_combo_->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  target_row->addWidget(target_combo_, 1);
  layout->addLayout(target_row);

  layout->addWidget(ui::Label(
      tr("Files are copied, the pack is left as it is. The copies are added "
         "to the library and the subtitles are checked again."),
      "caption"));

  auto* buttons = new QHBoxLayout;
  buttons->setSpacing(8);
  buttons->addStretch();
  auto* cancel_button = ui::Button(tr("Cancel"));
  import_button_ = ui::Button(QString(), "primary");
  buttons->addWidget(cancel_button);
  buttons->addWidget(import_button_);
  layout->addLayout(buttons);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(import_button_, &QPushButton::clicked, this, &QDialog::accept);

  import_button_->setDefault(true);
  RefreshButton();
  ui::RefreshIcons(this);
}

QStringList PackImportDialog::Sources() const {
  QStringList sources;
  for (int idx = 0; idx < matches_.size(); ++idx) {
    if (checks_.at(idx)->isChecked() &&
        !sources.contains(matches_.at(idx).path)) {
      sources.append(matches_.at(idx).path);
    }
  }
  return sources;
}

QString PackImportDialog::TargetDir() const {
  return target_combo_->currentData().toString();
}

void PackImportDialog::RefreshButton() {
  const int num_files = Sources().size();
  import_button_->setText(tr("Import %n file(s)", "", num_files));
  import_button_->setEnabled(num_files > 0);
}
