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
#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVector>

// Shows which missing fonts a font pack provides and lets the user pick the
// ones to copy into a library folder.
class PackImportDialog : public QDialog {
  Q_OBJECT

 public:
  struct Match {
    QString name;
    QString style;
    QString path;
    int num_missing_chars = 0;
  };

  PackImportDialog(const QString& pack_dir, const QVector<Match>& matches,
                   const QStringList& not_found, const QStringList& target_dirs,
                   const QString& last_target, QWidget* parent = nullptr);

  // Checked font files, each listed once even if it provides several fonts.
  QStringList Sources() const;

  QString TargetDir() const;

 private:
  QVector<Match> matches_;
  QVector<QCheckBox*> checks_;
  QComboBox* target_combo_;
  QPushButton* import_button_;

  void RefreshButton();
};
