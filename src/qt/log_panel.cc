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

#include "log_panel.h"

#include <QBoxLayout>
#include <QClipboard>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextStream>

#include "log_translator.h"

LogPanel::LogPanel(QWidget* parent) : QFrame(parent) {
  setProperty("card", true);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* header = new QWidget;
  auto* hlayout = new QHBoxLayout(header);
  hlayout->setContentsMargins(16, 8, 8, 8);
  hlayout->setSpacing(12);

  hlayout->addWidget(ui::Label(tr("Log"), "h2"));

  summary_label_ = ui::Label(QString(), "caption");
  hlayout->addWidget(summary_label_);

  tools_ = new QWidget;
  auto* tools_layout = new QHBoxLayout(tools_);
  tools_layout->setContentsMargins(0, 0, 0, 0);
  tools_layout->setSpacing(12);

  filter_ = new SegmentedControl({tr("All"), tr("Warnings"), tr("Errors")});
  filter_->setAccessibleName(tr("Filter by level"));
  tools_layout->addWidget(filter_);

  auto_clear_checkbox_ = new QCheckBox(tr("Clear before each run"));
  auto_clear_checkbox_->setChecked(true);
  tools_layout->addWidget(auto_clear_checkbox_);

  hlayout->addWidget(tools_);
  hlayout->addStretch();

  auto* copy_button = ui::Button(QString(), "ghost", "copy");
  copy_button->setToolTip(tr("Copy log"));
  copy_button->setAccessibleName(tr("Copy log"));
  auto* export_button = ui::Button(QString(), "ghost", "download");
  export_button->setToolTip(tr("Save log to file"));
  export_button->setAccessibleName(tr("Save log to file"));
  auto* clear_button = ui::Button(QString(), "ghost", "trash");
  clear_button->setToolTip(tr("Clear log"));
  clear_button->setAccessibleName(tr("Clear log"));
  collapse_button_ = ui::Button(QString(), "ghost", "chevron-down");
  collapse_button_->setToolTip(tr("Collapse"));
  collapse_button_->setAccessibleName(tr("Collapse"));

  hlayout->addWidget(copy_button);
  hlayout->addWidget(export_button);
  hlayout->addWidget(clear_button);
  hlayout->addWidget(collapse_button_);

  layout->addWidget(header);

  text_ = new QPlainTextEdit;
  text_->setObjectName("log");
  text_->setReadOnly(true);
  text_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  text_->setMinimumHeight(120);
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setPixelSize(12);
  text_->setFont(mono);
  highlighter_ = new LogHighlighter(text_->document());
  layout->addWidget(text_, 1);

  connect(filter_, &SegmentedControl::CurrentChanged, this,
          [this]() { Refresh(); });
  connect(auto_clear_checkbox_, &QCheckBox::toggled, this,
          &LogPanel::AutoClearChanged);
  connect(copy_button, &QPushButton::clicked, this, [this]() {
    QGuiApplication::clipboard()->setText(PlainText());
  });
  connect(export_button, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save log"),
        "assfonts-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
            ".log",
        tr("Log files (*.log *.txt)"));
    if (path.isEmpty()) {
      return;
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream stream(&file);
      stream << PlainText();
    }
  });
  connect(clear_button, &QPushButton::clicked, this, &LogPanel::Clear);
  connect(collapse_button_, &QPushButton::clicked, this,
          [this]() { SetExpanded(!is_expanded_); });

  UpdateCounts();
}

void LogPanel::Append(const QString& msg, ASSFONTS_LOG_LEVEL level) {
  // Empty lines are the library's separators; sections are marked by
  // BeginRun instead.
  if (msg.trimmed().isEmpty()) {
    return;
  }

  Item item{level, LogTranslator::Translate(msg)};
  items_.push_back(item);

  if (level == ASSFONTS_WARN) {
    ++num_warn_;
  } else if (level == ASSFONTS_ERROR) {
    ++num_error_;
  }

  if (Visible(item)) {
    AppendBlock(item);
  }
  UpdateCounts();
}

void LogPanel::BeginRun(const QString& title) {
  if (auto_clear_checkbox_->isChecked()) {
    Clear();
  }

  Item item{ASSFONTS_TEXT,
            QString("── %1 · %2 ──")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), title)};
  items_.push_back(item);
  AppendBlock(item);
}

void LogPanel::Clear() {
  items_.clear();
  num_warn_ = 0;
  num_error_ = 0;
  text_->clear();
  UpdateCounts();
}

void LogPanel::SetExpanded(bool is_expanded) {
  is_expanded_ = is_expanded;
  text_->setVisible(is_expanded);
  tools_->setVisible(is_expanded);
  collapse_button_->setProperty("iconName",
                                is_expanded ? "chevron-down" : "chevron-up");
  collapse_button_->setToolTip(is_expanded ? tr("Collapse") : tr("Expand"));
  ui::RefreshIcon(collapse_button_);
  UpdateCounts();
}

void LogPanel::SetAutoClear(bool is_auto_clear) {
  auto_clear_checkbox_->setChecked(is_auto_clear);
}

bool LogPanel::IsAutoClear() const {
  return auto_clear_checkbox_->isChecked();
}

void LogPanel::RefreshTheme() {
  highlighter_->SetColor();
  ui::RefreshIcons(this);
}

bool LogPanel::Visible(const Item& item) const {
  switch (filter_->CurrentIndex()) {
    case 1:
      return item.level == ASSFONTS_WARN || item.level == ASSFONTS_ERROR ||
             item.level == ASSFONTS_TEXT;
    case 2:
      return item.level == ASSFONTS_ERROR || item.level == ASSFONTS_TEXT;
    default:
      return true;
  }
}

void LogPanel::AppendBlock(const Item& item) {
  QString line;
  switch (item.level) {
    case ASSFONTS_INFO:
      line = tr("INFO") + "  ";
      break;
    case ASSFONTS_WARN:
      line = tr("WARN") + "  ";
      break;
    case ASSFONTS_ERROR:
      line = tr("ERROR") + "  ";
      break;
    default:
      break;
  }
  line += item.text;

  QScrollBar* bar = text_->verticalScrollBar();
  const bool is_at_bottom = bar->value() >= bar->maximum() - 4;

  text_->appendPlainText(line);
  QTextBlock block = text_->document()->lastBlock();
  block.setUserState(item.level);
  highlighter_->rehighlightBlock(block);

  if (is_at_bottom) {
    bar->setValue(bar->maximum());
  }
}

void LogPanel::Refresh() {
  text_->setUpdatesEnabled(false);
  text_->clear();
  for (const auto& item : items_) {
    if (Visible(item)) {
      AppendBlock(item);
    }
  }
  text_->setUpdatesEnabled(true);
  text_->verticalScrollBar()->setValue(text_->verticalScrollBar()->maximum());
}

void LogPanel::UpdateCounts() {
  filter_->SetItemText(0, tr("All %1").arg(items_.size()));
  filter_->SetItemText(1, tr("Warnings %1").arg(num_warn_));
  filter_->SetItemText(2, tr("Errors %1").arg(num_error_));

  if (is_expanded_) {
    summary_label_->clear();
    summary_label_->hide();
  } else {
    summary_label_->setText(tr("%1 warnings · %2 errors")
                                .arg(num_warn_)
                                .arg(num_error_));
    summary_label_->show();
  }
}

QString LogPanel::PlainText() const {
  QString res;
  for (const auto& item : items_) {
    switch (item.level) {
      case ASSFONTS_INFO:
        res += "[" + tr("INFO") + "] ";
        break;
      case ASSFONTS_WARN:
        res += "[" + tr("WARN") + "] ";
        break;
      case ASSFONTS_ERROR:
        res += "[" + tr("ERROR") + "] ";
        break;
      default:
        break;
    }
    res += item.text + "\n";
  }
  return res;
}
