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

#include "widgets.h"

#include <QBoxLayout>
#include <QPainter>
#include <QStyle>

#include "theme.h"

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);
}

QSize ToggleSwitch::sizeHint() const {
  return QSize(36, 20);
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
  const theme::Palette& p = theme::Current();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QRectF track(0, (height() - 20) / 2.0, 36, 20);
  QColor track_color = isChecked() ? p.accent : p.border_strong;
  if (!isEnabled()) {
    track_color.setAlphaF(0.45);
  }

  painter.setPen(Qt::NoPen);
  painter.setBrush(track_color);
  painter.drawRoundedRect(track, 10, 10);

  if (hasFocus()) {
    QPen focus_pen(p.accent, 1.5);
    painter.setPen(focus_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(track.adjusted(-2, -2, 2, 2), 12, 12);
    painter.setPen(Qt::NoPen);
  }

  const qreal knob_x = isChecked() ? track.right() - 18 : track.left() + 2;
  painter.setBrush(Qt::white);
  painter.drawEllipse(QRectF(knob_x, track.top() + 2, 16, 16));
}

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent) {
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

QSize ElidedLabel::minimumSizeHint() const {
  return QSize(0, QLabel::minimumSizeHint().height());
}

void ElidedLabel::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setPen(palette().color(foregroundRole()));
  painter.setFont(font());
  const QString elided =
      fontMetrics().elidedText(text(), Qt::ElideMiddle, contentsRect().width());
  painter.drawText(contentsRect(), alignment() | Qt::AlignVCenter, elided);
}

SegmentedControl::SegmentedControl(const QStringList& items, QWidget* parent)
    : QFrame(parent) {
  setProperty("segment", true);
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(3, 3, 3, 3);
  layout->setSpacing(2);

  group_ = new QButtonGroup(this);
  group_->setExclusive(true);

  for (int idx = 0; idx < items.size(); ++idx) {
    auto* button = new QPushButton(items.at(idx));
    button->setProperty("segmentItem", true);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    group_->addButton(button, idx);
    layout->addWidget(button);
  }

  if (group_->button(0) != nullptr) {
    group_->button(0)->setChecked(true);
  }

  connect(group_, &QButtonGroup::idClicked, this,
          &SegmentedControl::CurrentChanged);
}

int SegmentedControl::CurrentIndex() const {
  return group_->checkedId();
}

void SegmentedControl::SetCurrentIndex(int index) {
  if (group_->button(index) != nullptr) {
    group_->button(index)->setChecked(true);
  }
}

void SegmentedControl::SetItemText(int index, const QString& text) {
  if (group_->button(index) != nullptr) {
    group_->button(index)->setText(text);
  }
}

namespace ui {

QFrame* Card(QVBoxLayout** layout, int margin, int spacing) {
  auto* card = new QFrame;
  card->setProperty("card", true);

  auto* vlayout = new QVBoxLayout(card);
  vlayout->setContentsMargins(margin, margin, margin, margin);
  vlayout->setSpacing(spacing);

  if (layout != nullptr) {
    *layout = vlayout;
  }

  return card;
}

QLabel* Label(const QString& text, const QString& role, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  if (!role.isEmpty()) {
    label->setProperty("role", role);
  }
  if (role == "caption" || role == "muted" || role == "mono") {
    label->setWordWrap(true);
  }
  if (role == "mono") {
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  }
  return label;
}

QLabel* Chip(const QString& text, const QString& kind) {
  auto* label = new QLabel(text);
  label->setProperty("chip", kind);
  label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  return label;
}

void SetChip(QLabel* label, const QString& text, const QString& kind) {
  label->setText(text);
  if (label->property("chip").toString() != kind) {
    label->setProperty("chip", kind);
    Repolish(label);
  }
}

QPushButton* Button(const QString& text, const QString& kind,
                    const QString& icon) {
  auto* button = new QPushButton(text);
  button->setCursor(Qt::PointingHandCursor);
  if (!kind.isEmpty()) {
    button->setProperty("kind", kind);
  }
  if (!icon.isEmpty()) {
    button->setProperty("iconName", icon);
    RefreshIcon(button);
  }
  return button;
}

QFrame* Divider() {
  auto* line = new QFrame;
  line->setProperty("divider", true);
  line->setFrameShape(QFrame::NoFrame);
  return line;
}

QWidget* SwitchRow(ToggleSwitch** toggle, const QString& title,
                   const QString& description) {
  auto* row = new QWidget;
  auto* hlayout = new QHBoxLayout(row);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(12);

  auto* text_layout = new QVBoxLayout;
  text_layout->setSpacing(0);
  auto* title_label = Label(title);
  text_layout->addWidget(title_label);
  if (!description.isEmpty()) {
    text_layout->addWidget(Label(description, "caption"));
  }

  auto* sw = new ToggleSwitch;
  sw->setAccessibleName(title);
  title_label->setBuddy(sw);

  hlayout->addLayout(text_layout, 1);
  hlayout->addWidget(sw, 0, Qt::AlignVCenter);

  if (toggle != nullptr) {
    *toggle = sw;
  }

  return row;
}

void Repolish(QWidget* widget) {
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}

void RefreshIcon(QAbstractButton* button) {
  const QString name = button->property("iconName").toString();
  if (name.isEmpty()) {
    return;
  }

  const theme::Palette& p = theme::Current();
  QColor color = p.text;
  const QString kind = button->property("kind").toString();
  if (kind == "primary") {
    color = p.accent_text;
  } else if (kind == "danger") {
    color = p.error;
  } else if (button->property("nav").toBool() && button->isChecked()) {
    color = p.accent;
  }

  const int size = button->property("iconPx").isValid()
                       ? button->property("iconPx").toInt()
                       : 16;
  button->setIcon(theme::Icon(name, color, size));
  button->setIconSize(QSize(size, size));
}

void RefreshIcons(QWidget* root) {
  for (auto* button : root->findChildren<QAbstractButton*>()) {
    RefreshIcon(button);
  }
  for (auto* label : root->findChildren<QLabel*>()) {
    const QString name = label->property("iconName").toString();
    if (!name.isEmpty()) {
      const int size = label->property("iconPx").toInt();
      const QColor color = label->property("iconAccent").toBool()
                               ? theme::Current().accent
                               : theme::Current().text_muted;
      label->setPixmap(theme::Icon(name, color, size).pixmap(size, size));
    }
  }
}

QLabel* IconLabel(const QString& name, int size, bool accent) {
  auto* label = new QLabel;
  label->setProperty("iconName", name);
  label->setProperty("iconPx", size);
  label->setProperty("iconAccent", accent);
  label->setFixedSize(size, size);
  const QColor color =
      accent ? theme::Current().accent : theme::Current().text_muted;
  label->setPixmap(theme::Icon(name, color, size).pixmap(size, size));
  return label;
}

}  // namespace ui
