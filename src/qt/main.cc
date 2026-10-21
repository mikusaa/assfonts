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

#include <functional>

#include <QApplication>
#include <QDir>
#include <QLocale>
#include <QPointer>
#include <QStyleFactory>
#include <QThread>
#include <QTranslator>
#include <QtPlugin>

#include "main_window.h"
#include "task_runner.h"
#include "theme.h"

// Static Qt builds (vcpkg) must import the platform plugins explicitly.
#ifdef ASSFONTS_STATIC_QT
#ifdef __linux__
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
Q_IMPORT_PLUGIN(QWaylandIntegrationPlugin)
Q_IMPORT_PLUGIN(QWaylandEglPlatformIntegrationPlugin)
Q_IMPORT_PLUGIN(QIbusPlatformInputContextPlugin)
Q_IMPORT_PLUGIN(QComposePlatformInputContextPlugin)
Q_IMPORT_PLUGIN(QFcitx5PlatformInputContextPlugin)
#elif _WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#elif __APPLE__
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#endif
#endif

namespace {

void InstallTranslator(QApplication& app, QTranslator& translator,
                       const QString& setting) {
  app.removeTranslator(&translator);

  QString language = setting;
  if (language.isEmpty() || language == "system") {
    language = QLocale::system().name().startsWith("zh") ? "zh_CN" : "en";
  }

  if (language != "en" &&
      translator.load(":/i18n/assfonts_" + language + ".qm")) {
    app.installTranslator(&translator);
  }
}

void SetAppFont(QApplication& app) {
  QFont font = app.font();
#ifdef _WIN32
  // Segoe UI for Latin text, YaHei for CJK, so Chinese never falls back to
  // SimSun.
  font.setFamilies({"Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei"});
#endif
  font.setPixelSize(14);
  app.setFont(font);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  app.setStyle(QStyleFactory::create("fusion"));
  SetAppFont(app);

  QSettings settings(QDir(MainWindow::AppDataDir()).filePath("settings.ini"),
                     QSettings::IniFormat);

  theme::Apply(app, static_cast<theme::Mode>(
                        settings.value("General/Theme", 0).toInt()));

  QTranslator translator;
  InstallTranslator(app, translator,
                    settings.value("General/Language").toString());

  // The worker lives as long as the application, so the window can be
  // rebuilt (e.g. to switch language) without touching it.
  QThread worker_thread;
  TaskRunner::GetInstance().moveToThread(&worker_thread);
  worker_thread.start();

  QPointer<MainWindow> window;

  std::function<void()> create_window = [&]() {
    QPointer<MainWindow> old = window;

    window = new MainWindow(&settings);
    window->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(window, &MainWindow::RestartRequested, &app, [&]() {
      settings.sync();
      InstallTranslator(app, translator,
                        settings.value("General/Language").toString());
      theme::Apply(app, static_cast<theme::Mode>(
                            settings.value("General/Theme", 0).toInt()));
      QMetaObject::invokeMethod(&app, create_window, Qt::QueuedConnection);
    });

    if (old != nullptr) {
      window->setGeometry(old->geometry());
      old->setAttribute(Qt::WA_DeleteOnClose, false);
      old->hide();
      old->deleteLater();
    }

    window->show();
  };

  create_window();

  const int ret = app.exec();

  worker_thread.quit();
  worker_thread.wait();

  return ret;
}
