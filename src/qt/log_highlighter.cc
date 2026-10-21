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

#include "log_highlighter.h"

#include <algorithm>

#include <QTextDocument>

#include "theme.h"

LogHighlighter::LogHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
  SetColor();
}

void LogHighlighter::SetColor() {
  const theme::Palette& p = theme::Current();

  info_format_.setForeground(p.info);
  info_format_.setFontWeight(QFont::DemiBold);

  warn_format_.setForeground(p.warn);
  warn_format_.setFontWeight(QFont::DemiBold);

  error_format_.setForeground(p.error);
  error_format_.setFontWeight(QFont::DemiBold);

  text_format_.setForeground(p.text_muted);

  rehighlight();
}

void LogHighlighter::highlightBlock(const QString& text) {
  // The level tag runs up to the first double space.
  const int tag_len = std::max(0, static_cast<int>(text.indexOf("  ")));

  switch (currentBlockState()) {
    case ASSFONTS_INFO:
      setFormat(0, tag_len, info_format_);
      break;
    case ASSFONTS_WARN:
      setFormat(0, tag_len, warn_format_);
      break;
    case ASSFONTS_ERROR:
      setFormat(0, tag_len, error_format_);
      break;
    case ASSFONTS_TEXT:
      setFormat(0, text.size(), text_format_);
      break;
    default:
      break;
  }
}
