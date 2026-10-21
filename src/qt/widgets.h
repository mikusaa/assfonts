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

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

class QBoxLayout;
class QVBoxLayout;

class ToggleSwitch : public QAbstractButton {
  Q_OBJECT

 public:
  explicit ToggleSwitch(QWidget* parent = nullptr);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

// A one-line label that shortens long text with "…" in the middle.
class ElidedLabel : public QLabel {
  Q_OBJECT

 public:
  explicit ElidedLabel(const QString& text, QWidget* parent = nullptr);

  QSize minimumSizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

class SegmentedControl : public QFrame {
  Q_OBJECT

 public:
  explicit SegmentedControl(const QStringList& items,
                            QWidget* parent = nullptr);

  int CurrentIndex() const;
  void SetCurrentIndex(int index);
  void SetItemText(int index, const QString& text);

 signals:
  void CurrentChanged(int index);

 private:
  QButtonGroup* group_;
};

namespace ui {

// A white rounded panel. Content goes into the returned layout.
QFrame* Card(QVBoxLayout** layout, int margin = 16, int spacing = 12);

QLabel* Label(const QString& text, const QString& role = QString(),
              QWidget* parent = nullptr);

QLabel* Chip(const QString& text, const QString& kind);

void SetChip(QLabel* label, const QString& text, const QString& kind);

QPushButton* Button(const QString& text, const QString& kind = QString(),
                    const QString& icon = QString());

QFrame* Divider();

// A switch with a title and an optional description beside it.
QWidget* SwitchRow(ToggleSwitch** toggle, const QString& title,
                   const QString& description = QString());

// Re-applies the style sheet after a dynamic property changed.
void Repolish(QWidget* widget);

// Buttons and labels carrying an "iconName" property get their icon painted
// in the current theme's colors; call RefreshIcons after a theme change.
void RefreshIcon(QAbstractButton* button);

void RefreshIcons(QWidget* root);

QLabel* IconLabel(const QString& name, int size, bool accent = false);

}  // namespace ui
