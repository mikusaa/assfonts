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

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVector>

#include "assfonts.h"
#include "log_highlighter.h"
#include "widgets.h"

class LogPanel : public QFrame {
  Q_OBJECT

 public:
  explicit LogPanel(QWidget* parent = nullptr);

  // Translates the library message and appends it.
  void Append(const QString& msg, ASSFONTS_LOG_LEVEL level);

  // Starts a new section; clears the log first when auto-clear is on.
  void BeginRun(const QString& title);

  void Clear();

  void SetExpanded(bool is_expanded);
  bool IsExpanded() const { return is_expanded_; }

  void SetAutoClear(bool is_auto_clear);
  bool IsAutoClear() const;

  int NumProblems() const { return num_warn_ + num_error_; }

  void RefreshTheme();

 signals:
  void AutoClearChanged(bool is_auto_clear);

 private:
  struct Item {
    ASSFONTS_LOG_LEVEL level;
    QString text;
  };

  QVector<Item> items_;
  int num_warn_ = 0;
  int num_error_ = 0;
  bool is_expanded_ = true;

  QLabel* summary_label_;
  SegmentedControl* filter_;
  QCheckBox* auto_clear_checkbox_;
  QPushButton* collapse_button_;
  QWidget* tools_;
  QPlainTextEdit* text_;
  LogHighlighter* highlighter_;

  bool Visible(const Item& item) const;
  void AppendBlock(const Item& item);
  void Refresh();
  void UpdateCounts();
  QString PlainText() const;
};
