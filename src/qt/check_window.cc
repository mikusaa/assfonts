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

#include "check_window.h"

#include <httplib.h>

#include <assfonts.h>

// This fork publishes its own releases; upstream (wyzdwdz/assfonts and its
// Gitee mirror) knows nothing of them.
constexpr char DOWNLOAD_URL[] =
    "https://github.com/mikusaa/assfonts/releases/latest";

void CheckWindow::InitLayout() {
  QGridLayout* main_layout = new QGridLayout;
  main_layout->setAlignment(Qt::AlignCenter);
  main_layout->setVerticalSpacing(15);
  main_layout->setContentsMargins(25, 15, 25, 20);

  AddLabels(main_layout);

  setLayout(main_layout);
}

void CheckWindow::AddLabels(QGridLayout* layout) {
  QLabel* current_version_label = new QLabel(tr("Current version: "));
  QLabel* current_version_label_num =
      new QLabel(QString::number(ASSFONTS_VERSION_MAJOR) + "." +
                     QString::number(ASSFONTS_VERSION_MINOR) + "." +
                     QString::number(ASSFONTS_VERSION_PATCH),
                 this);

  layout->addWidget(current_version_label, 0, 0);
  layout->addWidget(current_version_label_num, 0, 1);

  QLabel* latest_version_label = new QLabel(tr("Latest version: "));
  latest_version_label_num_ = new QLabel(tr("Fetching ..."));

  layout->addWidget(latest_version_label, 1, 0);
  layout->addWidget(latest_version_label_num_, 1, 1);

  link_ = new QLabel(QString("<a href=\"") + DOWNLOAD_URL + "\">" +
                     DOWNLOAD_URL + "</a>");
  link_->setTextFormat(Qt::RichText);
  link_->setTextInteractionFlags(Qt::TextBrowserInteraction);
  link_->setOpenExternalLinks(true);

  layout->addWidget(link_, 2, 0, 1, 2);
}

void CheckWindow::InitConcurrents() {
  watcher_version_ = new QFutureWatcher<QString>(this);
  connect(watcher_version_, &QFutureWatcher<QString>::finished, this, [this]() {
    latest_version_label_num_->setText(watcher_version_->result());
  });

  QFuture<QString> future_version =
      QtConcurrent::run([this] { return CheckWindow::GetLatestVersion(); });
  watcher_version_->setFuture(future_version);

  watcher_link_ = new QFutureWatcher<QString>(this);
  connect(watcher_link_, &QFutureWatcher<QString>::finished, this,
          [this]() { link_->setText(watcher_link_->result()); });

  QFuture<QString> future_link =
      QtConcurrent::run([this] { return CheckWindow::GetDownloadLink(); });
  watcher_link_->setFuture(future_link);
}

QString CheckWindow::GetLatestVersion() {
  httplib::Client client("https://raw.githubusercontent.com");

  auto res = client.Get("/mikusaa/assfonts/HEAD/VERSION");

  if (res && res->status == 200) {
    return QString::fromStdString(res->body).trimmed();
  }

  return tr("Not found");
}

QString CheckWindow::GetDownloadLink() {
  return QString("<a href=\"") + DOWNLOAD_URL + "\">" + DOWNLOAD_URL + "</a>";
}