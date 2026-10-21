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

#include "log_translator.h"

#include <functional>
#include <vector>

#include <QRegularExpression>

namespace {

using Match = QRegularExpressionMatch;

struct Rule {
  QRegularExpression pattern;
  std::function<QString(const Match&)> format;
};

// Patterns are plain literals (no raw strings) so lupdate can parse the file.
// QUOTED matches a quoted path or name, STYLE an ASS "(bold,italic)" pair.
#define QUOTED "\"(.*)\""
#define STYLE "\\((-?\\d+),(-?\\d+)\\)"

}  // namespace

QString LogTranslator::FontStyle(int bold, int italic) {
  // ASS uses -1/1 for plain bold and a weight (100-900) otherwise.
  int weight = bold;
  if (bold == 1 || bold == -1) {
    weight = 700;
  } else if (bold == 0) {
    weight = 400;
  }

  QString style;
  if (weight >= 600) {
    style = tr("Bold");
  } else if (weight <= 300) {
    style = tr("Light");
  } else {
    style = tr("Regular");
  }

  if (italic != 0) {
    style += tr(" · Italic");
  }

  return style;
}

QString LogTranslator::Translate(const QString& msg) {
  static const std::vector<Rule> rules = {
      {QRegularExpression("^Found (\\d+) font files in " QUOTED
                          "\\. Parsing font files\\.$"),
       [](const Match& m) {
         return tr("Found %1 font files in \"%2\", reading them")
             .arg(m.captured(1), m.captured(2));
       }},
      {QRegularExpression("^Load fonts database " QUOTED "$"),
       [](const Match& m) {
         return tr("Loaded fonts database \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^Fonts database " QUOTED " doesn't exists\\.$"),
       [](const Match& m) {
         return tr("Fonts database \"%1\" doesn't exist yet")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Cannot load fonts database: " QUOTED "$"),
       [](const Match& m) {
         return tr("Cannot read fonts database \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^Fonts database has been saved in " QUOTED "$"),
       [](const Match& m) {
         return tr("Fonts database saved to \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED
                          " already exists\\. It will be overwritten\\.$"),
       [](const Match& m) {
         return tr("\"%1\" already exists and will be overwritten")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^No font is found\\. Nothing to save\\.$"),
       [](const Match&) { return tr("No font found, nothing to save"); }},
      {QRegularExpression("^Reading input file: " QUOTED "$"),
       [](const Match& m) {
         return tr("Reading \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^Detect input file encoding:\\s*" QUOTED "$"),
       [](const Match& m) {
         return tr("Detected file encoding: %1").arg(m.captured(1));
       }},
      {QRegularExpression("^Recode to \"UTF-8\" failed\\.$"),
       [](const Match&) { return tr("Cannot convert the file to UTF-8"); }},
      {QRegularExpression("^Failed to parse " QUOTED "\\. Format error\\.$"),
       [](const Match& m) {
         return tr("Cannot parse \"%1\": format error").arg(m.captured(1));
       }},
      {QRegularExpression("^Failed to parse " QUOTED
                          "\\. Incorrect number of field\\.$"),
       [](const Match& m) {
         return tr("Cannot parse \"%1\": wrong number of fields")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Failed to parse " QUOTED
                          "\\. No Event Title found\\.$"),
       [](const Match& m) {
         return tr("Cannot parse \"%1\": no [Events] section")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Failed to parse " QUOTED
                          "\\. No Style Title found\\.$"),
       [](const Match& m) {
         return tr("Cannot parse \"%1\": no [V4+ Styles] section")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Style " QUOTED " not found\\. \\(Line (\\d+)\\)$"),
       [](const Match& m) {
         return tr("Style \"%1\" is not defined (line %2)")
             .arg(m.captured(1), m.captured(2));
       }},
      {QRegularExpression("^Missing the font: " QUOTED " " STYLE "$"),
       [](const Match& m) {
         return tr("Missing font \"%1\" (%2)")
             .arg(m.captured(1), FontStyle(m.captured(2).toInt(),
                                           m.captured(3).toInt()));
       }},
      {QRegularExpression("^Found font: " QUOTED " " STYLE " --> " QUOTED
                          "\\[(\\d+)\\]$"),
       [](const Match& m) {
         return tr("Found font \"%1\" (%2) → %3")
             .arg(m.captured(1),
                  FontStyle(m.captured(2).toInt(), m.captured(3).toInt()),
                  m.captured(4));
       }},
      {QRegularExpression("^Missing codepoints for " QUOTED " " STYLE
                          ": (.*)$"),
       [](const Match& m) {
         return tr("\"%1\" (%2) has no glyph for: %3")
             .arg(m.captured(1),
                  FontStyle(m.captured(2).toInt(), m.captured(3).toInt()),
                  m.captured(4));
       }},
      {QRegularExpression(
           "^Found missing fonts\\. Check warning info above\\.$"),
       [](const Match&) {
         return tr("Some fonts are missing, see the warnings above");
       }},
      {QRegularExpression("^Create subset fonts directory: " QUOTED "$"),
       [](const Match& m) {
         return tr("Created folder for subset fonts \"%1\"")
             .arg(m.captured(1));
       }},
      {QRegularExpression(
           "^Create subset fonts directory failed\\. Error code: (.*)$"),
       [](const Match& m) {
         return tr("Cannot create folder for subset fonts: %1")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Subset failed: " QUOTED "\\[(\\d+)\\]$"),
       [](const Match& m) {
         return tr("Failed to subset \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^Create font-embeded subtitle: " QUOTED "$"),
       [](const Match& m) {
         return tr("Wrote subtitle with embedded fonts \"%1\"")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Create font-renamed subtitle: " QUOTED "$"),
       [](const Match& m) {
         return tr("Wrote subtitle with renamed fonts \"%1\"")
             .arg(m.captured(1));
       }},
      {QRegularExpression("^Failed to write the file: (.*)$"),
       [](const Match& m) {
         return tr("Cannot write file %1").arg(m.captured(1));
       }},
      {QRegularExpression("^Recolor failed: (.*)$"),
       [](const Match& m) {
         return tr("HDR brightness adjustment failed: %1").arg(m.captured(1));
       }},
      {QRegularExpression("^Recolored ass file has been saved in " QUOTED
                          "$"),
       [](const Match& m) {
         return tr("Wrote HDR-adjusted subtitle \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED " cannot be created\\.$"),
       [](const Match& m) {
         return tr("Cannot create \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED " cannot be opened\\.$"),
       [](const Match& m) {
         return tr("Cannot open \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED " has no parsable name\\.$"),
       [](const Match& m) {
         return tr("Cannot read the font name of \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED " is inaccessible\\.$"),
       [](const Match& m) {
         return tr("Cannot access \"%1\"").arg(m.captured(1));
       }},
      {QRegularExpression("^" QUOTED " is not a \\.ttf font\\.$"),
       [](const Match& m) {
         return tr("\"%1\" is not a TrueType font").arg(m.captured(1));
       }},
      {QRegularExpression(
           "^Failed in searching font files\\. Error code: (.*)$"),
       [](const Match& m) {
         return tr("Failed to search for font files: %1").arg(m.captured(1));
       }},
      {QRegularExpression("^No database directory\\.$"),
       [](const Match&) { return tr("No fonts database folder set"); }},
      {QRegularExpression("^No font directory\\.$"),
       [](const Match&) { return tr("No font folder added"); }},
      {QRegularExpression("^No input ASS file\\.$"),
       [](const Match&) { return tr("No subtitle file added"); }},
      {QRegularExpression("^No output directory\\.$"),
       [](const Match&) { return tr("No output folder set"); }},
  };

  for (const auto& rule : rules) {
    const auto match = rule.pattern.match(msg);
    if (match.hasMatch()) {
      return rule.format(match);
    }
  }

  return msg;
}
