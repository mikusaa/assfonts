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

#include <QCoreApplication>
#include <QString>

// The library logs in English; this maps its known messages to the UI
// language.
class LogTranslator {
  Q_DECLARE_TR_FUNCTIONS(LogTranslator)

 public:
  static QString Translate(const QString& msg);

  // "Bold", "Regular · Italic", ... for an ASS font description.
  static QString FontStyle(int bold, int italic);
};
