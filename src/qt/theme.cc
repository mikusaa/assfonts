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

#include "theme.h"

#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QStyleHints>

namespace theme {

namespace {

const Palette kLight = {
    QColor("#F5F6F8"), QColor("#ECEEF2"), QColor("#FFFFFF"),
    QColor("#FBFBFC"), QColor("#E1E4EA"), QColor("#D0D5DD"),
    QColor("#16181D"), QColor("#5B6170"), QColor("#3352D9"),
    QColor("#FFFFFF"), QColor("#E8ECFB"), QColor("#E6E8EE"),
    QColor("#FFFFFF"), QColor("#1F6B3E"), QColor("#E3F3E9"),
    QColor("#7A4A00"), QColor("#FFEDCC"), QColor("#B42318"),
    QColor("#FDE3E0"), QColor("#2F49C2")};

const Palette kDark = {
    QColor("#17191D"), QColor("#1D2025"), QColor("#23262C"),
    QColor("#1F2227"), QColor("#30343C"), QColor("#40454F"),
    QColor("#E8EAEE"), QColor("#A3A9B5"), QColor("#4C67E6"),
    QColor("#FFFFFF"), QColor("#2A3352"), QColor("#1A1C21"),
    QColor("#3A3F49"), QColor("#7DD8A4"), QColor("#1E3A2B"),
    QColor("#F4C860"), QColor("#3D3016"), QColor("#FF8F85"),
    QColor("#4A2323"), QColor("#9DB0FF")};

bool g_is_dark = false;

QString Hex(const QColor& color) {
  return color.name(QColor::HexRgb);
}

QString StyleSheet(const Palette& p) {
  QString qss = R"(
QMainWindow, QWidget#central, QWidget#page { background: @window; }
QWidget#scrollContent { background: transparent; }
QScrollArea { border: 0; background: transparent; }
QWidget { color: @text; }
QToolTip { color: @text; background: @card; border: 1px solid @border; padding: 4px 6px; }

QWidget#sidebar { background: @sidebar; border-right: 1px solid @border; }
QLabel#brandName { font-size: 15px; font-weight: 700; }
QPushButton[nav="true"] {
  border: 0; border-radius: 8px; padding: 0 12px; min-height: 40px;
  text-align: left; color: @text; background: transparent;
}
QPushButton[nav="true"]:hover { background: @border; }
QPushButton[nav="true"]:checked { background: @card; color: @accent; font-weight: 600; }

QFrame[card="true"] { background: @card; border: 1px solid @border; border-radius: 12px; }
QFrame[card="true"] QLabel { background: transparent; }
QFrame[divider="true"] { background: @border; border: 0; max-height: 1px; min-height: 1px; }
QFrame[cardFooter="true"] { background: @card_alt; border: 0; border-top: 1px solid @border;
  border-bottom-left-radius: 12px; border-bottom-right-radius: 12px; }

QLabel[role="h1"] { font-size: 22px; font-weight: 700; }
QLabel[role="h2"] { font-size: 15px; font-weight: 600; }
QLabel[role="h3"] { font-size: 15px; font-weight: 700; }
QLabel[role="label"] { font-weight: 600; }
QLabel[role="muted"] { color: @text_muted; }
QLabel[role="caption"] { color: @text_muted; font-size: 12px; }
QLabel[role="stat"] { font-size: 22px; font-weight: 700; }
QLabel[role="mono"] { color: @text_muted; font-size: 12px; }
QLabel[role="hint"] { background: @accent_soft; border-radius: 8px; padding: 8px 10px; }

QLabel[chip] { border-radius: 9px; padding: 1px 8px; font-size: 12px; font-weight: 600; }
QLabel[chip="ok"] { background: @ok_bg; color: @ok; }
QLabel[chip="warn"] { background: @warn_bg; color: @warn; }
QLabel[chip="error"] { background: @error_bg; color: @error; }
QLabel[chip="info"] { background: @accent_soft; color: @info; }
QLabel[chip="neutral"] { background: @segment_bg; color: @text_muted; }

QLabel#dropZone { border: 2px dashed @border_strong; border-radius: 10px;
  color: @text_muted; padding: 12px; background: transparent; }
QLabel#dropZone[active="true"] { border-color: @accent; color: @accent; background: @accent_soft; }

QPushButton {
  min-height: 30px; padding: 0 12px; border: 1px solid @border_strong;
  border-radius: 8px; background: @card; color: @text;
}
QPushButton:hover { background: @card_alt; border-color: @text_muted; }
QPushButton:pressed { background: @segment_bg; }
QPushButton:disabled { color: @text_muted; }
QPushButton[kind="primary"] {
  min-height: 38px; padding: 0 20px; border: 0; border-radius: 10px;
  background: @accent; color: @accent_text; font-weight: 700;
}
QPushButton[tall="true"] { min-height: 36px; }
QPushButton[kind="primary"]:hover { background: @accent_hover; }
QPushButton[kind="primary"]:disabled { background: @border_strong; color: @card; }
QPushButton[kind="ghost"] { border: 0; background: transparent; min-width: 30px; padding: 0 6px; }
QPushButton[kind="ghost"]:hover { background: @segment_bg; }
QPushButton[kind="danger"] { color: @error; }
QPushButton[kind="link"] { border: 0; background: transparent; color: @info; padding: 0; min-height: 0; }
QPushButton[kind="link"]:hover { text-decoration: underline; }

QFrame[segment="true"] { background: @segment_bg; border: 0; border-radius: 9px; }
QPushButton[segmentItem="true"] {
  border: 0; border-radius: 7px; background: transparent; color: @text_muted;
  min-height: 28px; padding: 0 12px;
}
QPushButton[segmentItem="true"]:hover { color: @text; }
QPushButton[segmentItem="true"]:checked { background: @segment_on; color: @text; }

QLineEdit, QComboBox {
  min-height: 30px; border: 1px solid @border_strong; border-radius: 8px;
  padding: 0 10px; background: @card; selection-background-color: @accent;
}
QLineEdit:focus, QComboBox:focus { border-color: @accent; }
QComboBox::drop-down { border: 0; width: 22px; }
QComboBox QAbstractItemView { background: @card; border: 1px solid @border;
  selection-background-color: @accent_soft; selection-color: @text; outline: 0; }

QTableWidget {
  border: 0; background: @card; alternate-background-color: @card;
  selection-background-color: @accent_soft; selection-color: @text; outline: 0;
}
QTableWidget::item { padding: 0 8px; border-bottom: 1px solid @border; }
QHeaderView { background: @card; }
QHeaderView::section {
  background: @card; color: @text_muted; border: 0; border-bottom: 1px solid @border;
  padding: 6px 8px; font-size: 12px;
}

QPlainTextEdit#log { border: 0; background: @card_alt; padding: 4px 8px; }

QCheckBox { spacing: 6px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @border_strong; border-radius: 3px; min-height: 30px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border_strong; border-radius: 3px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QProgressBar { border: 0; background: @segment_bg; border-radius: 3px; max-height: 6px; }
QProgressBar::chunk { background: @accent; border-radius: 3px; }

QSplitter::handle { background: transparent; }
QMenu { background: @card; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 6px 18px; border-radius: 6px; }
QMenu::item:selected { background: @accent_soft; }
)";

  const QHash<QString, QColor> tokens = {
      {"@window", p.window},
      {"@sidebar", p.sidebar},
      {"@card_alt", p.card_alt},
      {"@card", p.card},
      {"@border_strong", p.border_strong},
      {"@border", p.border},
      {"@text_muted", p.text_muted},
      {"@text", p.text},
      {"@accent_text", p.accent_text},
      {"@accent_soft", p.accent_soft},
      {"@accent_hover", g_is_dark ? p.accent.lighter(115) : p.accent.darker(112)},
      {"@accent", p.accent},
      {"@segment_bg", p.segment_bg},
      {"@segment_on", p.segment_on},
      {"@ok_bg", p.ok_bg},
      {"@ok", p.ok},
      {"@warn_bg", p.warn_bg},
      {"@warn", p.warn},
      {"@error_bg", p.error_bg},
      {"@error", p.error},
      {"@info", p.info}};

  // Longer token names are listed before their prefixes, so a plain ordered
  // replace is enough.
  const QStringList order = {
      "@window",     "@sidebar",       "@card_alt",    "@card",
      "@border_strong", "@border",     "@text_muted",  "@text",
      "@accent_text", "@accent_soft",  "@accent_hover", "@accent",
      "@segment_bg", "@segment_on",    "@ok_bg",       "@ok",
      "@warn_bg",    "@warn",          "@error_bg",    "@error",
      "@info"};

  for (const auto& token : order) {
    qss.replace(token, Hex(tokens.value(token)));
  }

  return qss;
}

QPalette MakePalette(const Palette& p) {
  QPalette pal;
  pal.setColor(QPalette::Window, p.window);
  pal.setColor(QPalette::WindowText, p.text);
  pal.setColor(QPalette::Base, p.card);
  pal.setColor(QPalette::AlternateBase, p.card_alt);
  pal.setColor(QPalette::Text, p.text);
  pal.setColor(QPalette::Button, p.card);
  pal.setColor(QPalette::ButtonText, p.text);
  pal.setColor(QPalette::Highlight, p.accent);
  pal.setColor(QPalette::HighlightedText, p.accent_text);
  pal.setColor(QPalette::ToolTipBase, p.card);
  pal.setColor(QPalette::ToolTipText, p.text);
  pal.setColor(QPalette::PlaceholderText, p.text_muted);
  pal.setColor(QPalette::Link, p.info);
  pal.setColor(QPalette::Mid, p.border_strong);
  pal.setColor(QPalette::Dark, p.border_strong);
  pal.setColor(QPalette::Disabled, QPalette::Text, p.text_muted);
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.text_muted);
  pal.setColor(QPalette::Disabled, QPalette::WindowText, p.text_muted);
  return pal;
}

bool SystemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  const QColor window = QGuiApplication::palette().color(QPalette::Window);
  return window.lightness() < 128;
#endif
}

// Each icon is a list of commands on a 24x24 grid:
//   M x y / L x y / Z     polyline
//   O cx cy r             circle
//   R x y w h r           rounded rectangle
//   A cx cy r start span  arc (degrees, counter-clockwise)
//   F                     fill the whole shape instead of stroking it
const QHash<QString, QString>& IconSpecs() {
  static const QHash<QString, QString> specs = {
      {"logo", "R 3 5 18 14 3 M 7 14 L 11 14 M 14 14 L 17 14 M 7 10 L 17 10"},
      {"file",
       "M 14 3 L 6.5 3 L 5 4.5 L 5 19.5 L 6.5 21 L 17.5 21 L 19 19.5 L 19 8 Z "
       "M 14 3 L 14 8 L 19 8 M 9 13 L 15 13 M 9 17 L 13 17"},
      {"font", "M 4 7 L 4 5 L 20 5 L 20 7 M 12 5 L 12 19 M 9 19 L 15 19"},
      {"settings",
       "M 4 6 L 13 6 M 17 6 L 20 6 M 4 12 L 7 12 M 11 12 L 20 12 M 4 18 L 15 18 "
       "M 19 18 L 20 18 O 15 6 2 O 9 12 2 O 17 18 2"},
      {"plus", "M 12 5 L 12 19 M 5 12 L 19 12"},
      {"folder",
       "M 3 6.5 L 4.5 5 L 9 5 L 11 7 L 19.5 7 L 21 8.5 L 21 17.5 L 19.5 19 "
       "L 4.5 19 L 3 17.5 Z"},
      {"play", "F M 8 5 L 8 19 L 19 12 Z"},
      {"stop", "F R 6 6 12 12 2"},
      {"refresh",
       "A 12 12 7 20 140 M 3.5 7.5 L 5.4 9.6 L 8.2 8.4 A 12 12 7 200 140 "
       "M 20.5 16.5 L 18.6 14.4 L 15.8 15.6"},
      {"x", "M 6 6 L 18 18 M 18 6 L 6 18"},
      {"check", "M 5 12 L 10 17 L 20 7"},
      {"alert", "M 12 4 L 21 20 L 3 20 Z M 12 10 L 12 14 M 12 17 L 12 17.2"},
      {"copy", "R 9 9 11 11 2 M 5 15 L 5 5 L 7 3 L 15 3"},
      {"download", "M 12 4 L 12 15 M 7 10 L 12 15 L 17 10 M 5 20 L 19 20"},
      {"upload", "M 12 16 L 12 4 M 7 9 L 12 4 L 17 9 M 5 20 L 19 20"},
      {"trash",
       "M 4 7 L 20 7 M 10 11 L 10 17 M 14 11 L 14 17 M 6 7 L 7 20 L 17 20 "
       "L 18 7 M 9 7 L 9 4 L 15 4 L 15 7"},
      {"search", "O 11 11 6 M 20 20 L 15.5 15.5"},
      {"chevron-down", "M 6 9 L 12 15 L 18 9"},
      {"chevron-up", "M 6 15 L 12 9 L 18 15"},
      {"monitor", "R 3 4 18 12 2 M 8 20 L 16 20 M 12 16 L 12 20"},
      {"globe", "O 12 12 9 M 3 12 L 21 12 A 12 12 9 90 0 M 12 3 L 12 21"},
  };
  return specs;
}

QPainterPath BuildPath(const QString& spec, bool& is_fill) {
  QPainterPath path;
  const QStringList tokens = spec.split(' ', Qt::SkipEmptyParts);
  is_fill = false;

  int idx = 0;
  auto num = [&]() -> qreal {
    return idx < tokens.size() ? tokens.at(idx++).toDouble() : 0.0;
  };

  while (idx < tokens.size()) {
    const QString cmd = tokens.at(idx++);
    if (cmd == "F") {
      is_fill = true;
    } else if (cmd == "M") {
      qreal x = num();
      qreal y = num();
      path.moveTo(x, y);
    } else if (cmd == "L") {
      qreal x = num();
      qreal y = num();
      path.lineTo(x, y);
    } else if (cmd == "Z") {
      path.closeSubpath();
    } else if (cmd == "O") {
      qreal cx = num();
      qreal cy = num();
      qreal r = num();
      path.addEllipse(QPointF(cx, cy), r, r);
    } else if (cmd == "R") {
      qreal x = num();
      qreal y = num();
      qreal w = num();
      qreal h = num();
      qreal r = num();
      path.addRoundedRect(QRectF(x, y, w, h), r, r);
    } else if (cmd == "A") {
      qreal cx = num();
      qreal cy = num();
      qreal r = num();
      qreal start = num();
      qreal span = num();
      QRectF rect(cx - r, cy - r, 2 * r, 2 * r);
      if (span == 0) {
        // A vertical ellipse through the center, used for the globe.
        path.addEllipse(QPointF(cx, cy), r * 0.45, r);
      } else {
        path.arcMoveTo(rect, start);
        path.arcTo(rect, start, span);
      }
    }
  }

  return path;
}

}  // namespace

void Apply(QApplication& app, Mode mode) {
  switch (mode) {
    case Mode::kLight:
      g_is_dark = false;
      break;
    case Mode::kDark:
      g_is_dark = true;
      break;
    default:
      g_is_dark = SystemIsDark();
      break;
  }

  const Palette& p = Current();
  app.setPalette(MakePalette(p));
  app.setStyleSheet(StyleSheet(p));
}

bool IsDark() {
  return g_is_dark;
}

const Palette& Current() {
  return g_is_dark ? kDark : kLight;
}

QIcon Icon(const QString& name, const QColor& color, int size) {
  bool is_fill = false;
  const QPainterPath path = BuildPath(IconSpecs().value(name), is_fill);

  QIcon icon;
  for (const qreal dpr : {1.0, 2.0}) {
    QPixmap pixmap(QSize(size, size) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);

    if (is_fill) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(color);
    } else {
      QPen pen(color, 1.8);
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
    }
    painter.drawPath(path);
    painter.end();

    icon.addPixmap(pixmap);
  }

  return icon;
}

}  // namespace theme
