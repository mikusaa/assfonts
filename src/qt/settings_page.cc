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

#include "settings_page.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QUrl>

#include "assfonts.h"
#include "font_db.h"

namespace {

QWidget* Row(const QString& title, const QString& description,
             QWidget* control) {
  auto* row = new QWidget;
  auto* hlayout = new QHBoxLayout(row);
  hlayout->setContentsMargins(20, 12, 20, 12);
  hlayout->setSpacing(16);

  auto* texts = new QVBoxLayout;
  texts->setSpacing(0);
  texts->addWidget(ui::Label(title));
  if (!description.isEmpty()) {
    texts->addWidget(ui::Label(description, "caption"));
  }
  hlayout->addLayout(texts, 1);
  if (control != nullptr) {
    hlayout->addWidget(control, 0, Qt::AlignVCenter);
  }
  return row;
}

QWidget* SwitchRow(ToggleSwitch** toggle, const QString& title,
                   const QString& description = QString()) {
  auto* sw = new ToggleSwitch;
  sw->setAccessibleName(title);
  *toggle = sw;
  return Row(title, description, sw);
}

}  // namespace

SettingsPage::SettingsPage(QSettings* settings, QWidget* parent)
    : QWidget(parent), settings_(settings) {
  setObjectName("page");

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

  auto* column = new QVBoxLayout;
  column->setSpacing(16);

  column->addWidget(ui::Label(tr("Settings"), "h1"));
  column->addWidget(ui::Label(tr("Changes are saved automatically."), "muted"));

  // General.
  language_combo_ = new QComboBox;
  language_combo_->addItem(tr("Follow system"), "system");
  language_combo_->addItem("简体中文", "zh_CN");
  language_combo_->addItem("English", "en");
  language_combo_->setMinimumWidth(180);
  language_combo_->setAccessibleName(tr("Language"));

  theme_segment_ =
      new SegmentedControl({tr("Follow system"), tr("Light", "appearance"),
                            tr("Dark", "appearance")});
  theme_segment_->setAccessibleName(tr("Appearance"));

  column->addWidget(Section(
      tr("General"),
      {Row(tr("Language"), tr("The window reloads in the new language."),
           language_combo_),
       Row(tr("Appearance"), QString(), theme_segment_)}));

  // Processing.
  column->addWidget(Section(
      tr("Processing", "settings section"),
      {SwitchRow(&multi_thread_switch_, tr("Process several files at once"),
                 tr("Faster, but uses much more memory.")),
       SwitchRow(&precheck_switch_, tr("Check fonts when files are added"),
                 tr("Shows missing fonts right away instead of after "
                    "processing.")),
       SwitchRow(&open_output_switch_, tr("Open the output folder when done"),
                 tr("Only when every file succeeded."))}));

  // Log.
  column->addWidget(Section(
      tr("Log"),
      {SwitchRow(&auto_clear_switch_, tr("Clear the log before each run")),
       SwitchRow(&auto_expand_switch_,
                 tr("Show the log when there are warnings or errors"),
                 tr("Otherwise the log stays folded away."))}));

  // Data.
  auto* data_buttons = new QWidget;
  auto* data_layout = new QHBoxLayout(data_buttons);
  data_layout->setContentsMargins(0, 0, 0, 0);
  data_layout->setSpacing(8);
#ifdef _WIN32
  auto* reveal_button = ui::Button(tr("Show in Explorer"));
#elif __APPLE__
  auto* reveal_button = ui::Button(tr("Show in Finder"));
#else
  auto* reveal_button = ui::Button(tr("Open folder"));
#endif
  auto* change_button = ui::Button(tr("Change…"));
  data_layout->addWidget(reveal_button);
  data_layout->addWidget(change_button);

  auto* db_row = Row(tr("Library data location"), QString(), data_buttons);
  db_dir_label_ = ui::Label(QString(), "mono");
  qobject_cast<QBoxLayout*>(db_row->layout()->itemAt(0)->layout())
      ->addWidget(db_dir_label_);

  auto* reset_button = ui::Button(tr("Reset…"), "danger");
  auto* check_button = ui::Button(tr("Check for updates"));

  column->addWidget(Section(
      tr("Data"),
      {db_row,
       Row(tr("Reset settings"),
           tr("Your font library and its folders are kept."), reset_button),
       Row(tr("Version %1.%2.%3")
               .arg(ASSFONTS_VERSION_MAJOR)
               .arg(ASSFONTS_VERSION_MINOR)
               .arg(ASSFONTS_VERSION_PATCH),
           QString(), check_button)}));

  // Keep the settings column readable on wide windows.
  auto* center = new QHBoxLayout;
  auto* column_widget = new QWidget;
  column_widget->setLayout(column);
  column_widget->setMaximumWidth(860);
  center->addWidget(column_widget, 1);
  center->addStretch(0);
  layout->addLayout(center);
  layout->addStretch();

  // Load values.
  const int lang_index = language_combo_->findData(
      settings_->value("General/Language", "system").toString());
  language_combo_->setCurrentIndex(lang_index < 0 ? 0 : lang_index);
  theme_segment_->SetCurrentIndex(settings_->value("General/Theme", 0).toInt());
  multi_thread_switch_->setChecked(
      settings_->value("Run/MultiThread", false).toBool());
  precheck_switch_->setChecked(
      settings_->value("Run/AutoPrecheck", true).toBool());
  open_output_switch_->setChecked(
      settings_->value("Run/OpenOutput", false).toBool());
  auto_clear_switch_->setChecked(
      settings_->value("Log/AutoClear", true).toBool());
  auto_expand_switch_->setChecked(
      settings_->value("Log/AutoExpand", true).toBool());
  db_dir_label_->setText(settings_->value("Library/Database").toString());

  // Save on change.
  connect(language_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    settings_->setValue("General/Language", language_combo_->currentData());
    emit LanguageChanged();
  });
  connect(theme_segment_, &SegmentedControl::CurrentChanged, this,
          [this](int index) {
            settings_->setValue("General/Theme", index);
            emit ThemeChanged();
          });
  connect(multi_thread_switch_, &ToggleSwitch::toggled, this,
          [this](bool checked) {
            settings_->setValue("Run/MultiThread", checked);
          });
  connect(precheck_switch_, &ToggleSwitch::toggled, this,
          [this](bool checked) {
            settings_->setValue("Run/AutoPrecheck", checked);
          });
  connect(open_output_switch_, &ToggleSwitch::toggled, this,
          [this](bool checked) {
            settings_->setValue("Run/OpenOutput", checked);
          });
  connect(auto_clear_switch_, &ToggleSwitch::toggled, this,
          [this](bool checked) {
            settings_->setValue("Log/AutoClear", checked);
            emit AutoClearLogChanged(checked);
          });
  connect(auto_expand_switch_, &ToggleSwitch::toggled, this,
          [this](bool checked) {
            settings_->setValue("Log/AutoExpand", checked);
          });

  connect(reveal_button, &QPushButton::clicked, this, [this]() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        settings_->value("Library/Database").toString()));
  });

  connect(change_button, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose where to keep the font library"),
        settings_->value("Library/Database").toString());
    if (path.isEmpty()) {
      return;
    }
    const QString native = QDir::toNativeSeparators(path);
    settings_->setValue("Library/Database", native);
    db_dir_label_->setText(native);
    emit DatabaseDirChanged();
  });

  connect(reset_button, &QPushButton::clicked, this, [this]() {
    const auto answer = QMessageBox::question(
        this, tr("Reset settings"),
        tr("Reset all settings to their defaults? Your font library and its "
           "folders are kept."));
    if (answer == QMessageBox::Yes) {
      emit ResetRequested();
    }
  });

  connect(check_button, &QPushButton::clicked, this,
          &SettingsPage::CheckUpdateRequested);
}

void SettingsPage::SetAutoClearLog(bool is_auto_clear) {
  QSignalBlocker blocker(auto_clear_switch_);
  auto_clear_switch_->setChecked(is_auto_clear);
}

void SettingsPage::RefreshTheme() {
  ui::RefreshIcons(this);
  update();
}

QWidget* SettingsPage::Section(const QString& title,
                               const QList<QWidget*>& rows) {
  QVBoxLayout* layout = nullptr;
  QFrame* card = ui::Card(&layout, 0, 0);

  auto* title_label = ui::Label(title, "h2");
  title_label->setContentsMargins(20, 14, 20, 14);
  layout->addWidget(title_label);

  for (auto* row : rows) {
    layout->addWidget(ui::Divider());
    layout->addWidget(row);
  }

  return card;
}
