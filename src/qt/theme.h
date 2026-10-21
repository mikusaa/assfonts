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

#include <QColor>
#include <QIcon>
#include <QString>

class QApplication;

namespace theme {

enum class Mode { kSystem = 0, kLight, kDark };

struct Palette {
  QColor window;
  QColor sidebar;
  QColor card;
  QColor card_alt;
  QColor border;
  QColor border_strong;
  QColor text;
  QColor text_muted;
  QColor accent;
  QColor accent_text;
  QColor accent_soft;
  QColor segment_bg;
  QColor segment_on;
  QColor ok;
  QColor ok_bg;
  QColor warn;
  QColor warn_bg;
  QColor error;
  QColor error_bg;
  QColor info;
};

void Apply(QApplication& app, Mode mode);

bool IsDark();

const Palette& Current();

// Draws a 24x24 stroke icon by name, tinted with the given color.
QIcon Icon(const QString& name, const QColor& color, int size = 18);

}  // namespace theme
