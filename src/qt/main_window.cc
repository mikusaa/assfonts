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

#include "main_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QMetaType>
#include <QStyleHints>
#include <QUrl>
#include <ghc/filesystem.hpp>

#ifdef __APPLE__
#include "get_app_support_dir.h"
#elif _WIN32
#include <Shlobj.h>
#else
#endif

#include "theme.h"

namespace fs = ghc::filesystem;

namespace {

constexpr int kSidebarWidth = 208;
// Room for the 40px nav buttons and the sidebar's 12px margins.
constexpr int kSidebarCollapsedWidth = 64;

}  // namespace

static std::string SAVE_FILES_PATH = []() {
  fs::path path = fs::current_path();

#ifdef __APPLE__
  path = fs::path(GetAppSupportDir()) / "assfonts";
#elif __linux__
  if (getenv("HOME")) {
    path = fs::path(getenv("HOME")) / ".local" / "share" / "assfonts";
  }
#elif _WIN32
  TCHAR sz_path[MAX_PATH];
  SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, sz_path);

  path = fs::path(sz_path) / "assfonts";
#endif

  std::error_code ec;
  fs::create_directory(path, ec);

  return path.u8string();
}();

QString MainWindow::AppDataDir() {
  // Lets tests and portable setups keep settings and the library elsewhere.
  if (qEnvironmentVariableIsSet("ASSFONTS_DATA_DIR")) {
    return QDir::toNativeSeparators(qEnvironmentVariable("ASSFONTS_DATA_DIR"));
  }
  return QDir::toNativeSeparators(QString::fromStdString(SAVE_FILES_PATH));
}

MainWindow::MainWindow(QSettings* settings, QWidget* parent)
    : QMainWindow(parent), settings_(settings) {
  qRegisterMetaType<ASSFONTS_LOG_LEVEL>("ASSFONTS_LOG_LEVEL");

  setWindowTitle("assfonts");
  setWindowIcon(QIcon(":/icon.png"));

  MigrateOldSettings();
  if (settings_->value("Library/Database").toString().isEmpty()) {
    settings_->setValue("Library/Database", AppDataDir());
  }

  worker_ = &TaskRunner::GetInstance();
  font_db_.Load(DbPath());

  auto* central = new QWidget;
  central->setObjectName("central");
  auto* hlayout = new QHBoxLayout(central);
  hlayout->setContentsMargins(0, 0, 0, 0);
  hlayout->setSpacing(0);

  hlayout->addWidget(InitSidebar());

  auto* right = new QVBoxLayout;
  right->setContentsMargins(0, 0, 0, 0);
  right->setSpacing(0);

  stack_ = new QStackedWidget;
  process_page_ = new ProcessPage(settings_, &font_db_);
  library_page_ = new LibraryPage(settings_, &font_db_);
  settings_page_ = new SettingsPage(settings_);
  stack_->addWidget(process_page_);
  stack_->addWidget(library_page_);
  stack_->addWidget(settings_page_);
  right->addWidget(stack_, 1);

  auto* log_wrap = new QWidget;
  log_wrap->setObjectName("page");
  auto* log_layout = new QVBoxLayout(log_wrap);
  log_layout->setContentsMargins(28, 0, 28, 20);
  log_panel_ = new LogPanel;
  log_panel_->setMaximumHeight(240);
  log_layout->addWidget(log_panel_);
  right->addWidget(log_wrap);

  hlayout->addLayout(right, 1);
  setCentralWidget(central);

  InitConnects();
  LoadSettings();

  library_page_->Refresh();
  process_page_->RefreshLibraryStatus(library_page_->FontDirs().size());

  log_panel_->Append(QString("assfonts %1.%2.%3")
                         .arg(ASSFONTS_VERSION_MAJOR)
                         .arg(ASSFONTS_VERSION_MINOR)
                         .arg(ASSFONTS_VERSION_PATCH),
                     ASSFONTS_TEXT);

  // First start: point the user at the library.
  if (font_db_.IsEmpty() && library_page_->FontDirs().isEmpty()) {
    ShowPage(1);
  }
}

QWidget* MainWindow::InitSidebar() {
  sidebar_ = new QWidget;
  sidebar_->setObjectName("sidebar");
  sidebar_->setAttribute(Qt::WA_StyledBackground);
  sidebar_->setFixedWidth(kSidebarWidth);

  auto* layout = new QVBoxLayout(sidebar_);
  layout->setContentsMargins(12, 18, 12, 12);
  layout->setSpacing(4);

  brand_layout_ = new QHBoxLayout;
  brand_layout_->setContentsMargins(8, 0, 8, 18);
  brand_layout_->setSpacing(10);
  auto* logo = new QLabel;
  logo->setPixmap(QIcon(":/icon.png").pixmap(32, 32));
  logo->setFixedSize(32, 32);
  brand_layout_->addWidget(logo);
  brand_texts_ = new QWidget;
  auto* brand_texts = new QVBoxLayout(brand_texts_);
  brand_texts->setContentsMargins(0, 0, 0, 0);
  brand_texts->setSpacing(0);
  auto* name = new QLabel("assfonts");
  name->setObjectName("brandName");
  brand_texts->addWidget(name);
  brand_texts->addWidget(ui::Label(tr("Subtitle font embedding"), "caption"));
  brand_layout_->addWidget(brand_texts_, 1);
  layout->addLayout(brand_layout_);

  nav_group_ = new QButtonGroup(this);
  nav_group_->setExclusive(true);

  const QList<QPair<QString, QString>> items = {
      {tr("Process subtitles"), "file"},
      {tr("Font library"), "font"},
      {tr("Settings"), "settings"}};

  for (int idx = 0; idx < items.size(); ++idx) {
    auto* button = new QPushButton;
    button->setText(items.at(idx).first);
    button->setAccessibleName(items.at(idx).first);
    button->setProperty("navText", items.at(idx).first);
    button->setProperty("nav", true);
    button->setProperty("iconName", items.at(idx).second);
    button->setProperty("iconPx", 18);
    button->setCheckable(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    nav_group_->addButton(button, idx);
    layout->addWidget(button);

    connect(button, &QPushButton::toggled, this,
            [button]() { ui::RefreshIcon(button); });
  }
  nav_group_->button(0)->setChecked(true);

  layout->addStretch();

  sidebar_footer_ = new QWidget;
  auto* footer = new QHBoxLayout(sidebar_footer_);
  footer->setContentsMargins(10, 0, 10, 4);
  footer->addWidget(ui::Label(QString("v%1.%2.%3")
                                  .arg(ASSFONTS_VERSION_MAJOR)
                                  .arg(ASSFONTS_VERSION_MINOR)
                                  .arg(ASSFONTS_VERSION_PATCH),
                              "caption"));
  footer->addStretch();
  auto* update_button = ui::Button(tr("Check for updates"), "link");
  footer->addWidget(update_button);
  layout->addWidget(sidebar_footer_);

  // Kept last, so it stays in the same corner in both states.
  collapse_button_ = new QPushButton;
  collapse_button_->setProperty("nav", true);
  collapse_button_->setProperty("iconPx", 18);
  collapse_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  collapse_button_->setCursor(Qt::PointingHandCursor);
  layout->addWidget(collapse_button_);

  sidebar_anim_ = new QVariantAnimation(this);
  sidebar_anim_->setDuration(160);
  sidebar_anim_->setEasingCurve(QEasingCurve::OutCubic);

  connect(nav_group_, &QButtonGroup::idClicked, this, &MainWindow::ShowPage);
  connect(update_button, &QPushButton::clicked, this,
          &MainWindow::ShowCheckWindow);
  connect(collapse_button_, &QPushButton::clicked, this, [this]() {
    SetSidebarCollapsed(!is_sidebar_collapsed_, true);
  });
  connect(sidebar_anim_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            sidebar_->setFixedWidth(value.toInt());
          });

  UpdateSidebarContent();
  return sidebar_;
}

void MainWindow::SetSidebarCollapsed(bool is_collapsed, bool is_animated) {
  is_sidebar_collapsed_ = is_collapsed;
  // The labels go first, so they never wrap while the width changes.
  UpdateSidebarContent();

  const int target = is_collapsed ? kSidebarCollapsedWidth : kSidebarWidth;
  sidebar_anim_->stop();
  if (!is_animated) {
    sidebar_->setFixedWidth(target);
    return;
  }
  sidebar_anim_->setStartValue(sidebar_->width());
  sidebar_anim_->setEndValue(target);
  sidebar_anim_->start();
}

void MainWindow::UpdateSidebarContent() {
  const bool is_collapsed = is_sidebar_collapsed_;

  sidebar_->setProperty("collapsed", is_collapsed);
  brand_texts_->setVisible(!is_collapsed);
  sidebar_footer_->setVisible(!is_collapsed);
  brand_layout_->setContentsMargins(is_collapsed ? 4 : 8, 0,
                                    is_collapsed ? 4 : 8, 18);

  // Collapsed, the labels move into tooltips.
  for (auto* button : nav_group_->buttons()) {
    const QString text = button->property("navText").toString();
    button->setText(is_collapsed ? QString() : text);
    button->setToolTip(is_collapsed ? text : QString());
  }

  const QString toggle_text =
      is_collapsed ? tr("Expand sidebar") : tr("Collapse sidebar");
  collapse_button_->setText(is_collapsed ? QString() : toggle_text);
  collapse_button_->setToolTip(is_collapsed ? toggle_text : QString());
  collapse_button_->setAccessibleName(toggle_text);
  collapse_button_->setProperty(
      "iconName", is_collapsed ? "sidebar-expand" : "sidebar-collapse");

  ui::Repolish(sidebar_);
  for (auto* button : sidebar_->findChildren<QPushButton*>()) {
    ui::Repolish(button);
  }
  ui::RefreshIcons(sidebar_);
}

void MainWindow::InitConnects() {
  // Requests to the worker thread.
  connect(this, &MainWindow::OnSendBuild, worker_, &TaskRunner::OnBuildRun);
  connect(this, &MainWindow::OnSendCheck, worker_, &TaskRunner::OnCheckRun);
  connect(this, &MainWindow::OnSendPackScan, worker_,
          &TaskRunner::OnPackScanRun);
  connect(this, &MainWindow::OnSendImport, worker_, &TaskRunner::OnImportRun);
  connect(this, &MainWindow::OnSendStart, worker_, &TaskRunner::OnStartRun);

  // Results from the worker thread.
  connect(worker_, &TaskRunner::OnSendLog, this,
          [this](QString msg, ASSFONTS_LOG_LEVEL level) {
            log_panel_->Append(msg, level);
          });

  connect(worker_, &TaskRunner::OnSendBusy, this,
          [this](bool is_busy, QString what) {
            process_page_->SetBusy(is_busy);
            library_page_->SetBusy(is_busy);
            setWindowTitle(is_busy ? QString("assfonts · %1").arg(what)
                                   : QString("assfonts"));
          });

  connect(worker_, &TaskRunner::OnSendBuildDone, this, [this]() {
    ReloadDatabase();
    if (is_retry_pending_) {
      // The retry already queued its own check.
      is_retry_pending_ = false;
    } else {
      process_page_->CheckAll();
    }
  });

  connect(worker_, &TaskRunner::OnSendCheckDone, this,
          [this](QStringList inputs, QString json) {
            process_page_->OnCheckDone(inputs, json);
          });

  connect(worker_, &TaskRunner::OnSendPackScanDone, this,
          [this](QString pack_dir, QString json) {
            process_page_->OnPackScanDone(pack_dir, json,
                                          library_page_->FontDirs());
          });

  connect(worker_, &TaskRunner::OnSendFileStarted, process_page_,
          &ProcessPage::OnFileStarted);
  connect(worker_, &TaskRunner::OnSendFileDone, process_page_,
          &ProcessPage::OnFileDone);

  connect(worker_, &TaskRunner::OnSendStartDone, this,
          [this](bool is_stopped) {
            process_page_->OnRunDone(is_stopped);
            ReloadDatabase();

            if (log_panel_->NumProblems() > 0 &&
                settings_->value("Log/AutoExpand", true).toBool()) {
              log_panel_->SetExpanded(true);
            }

            if (!is_stopped && log_panel_->NumProblems() == 0 &&
                settings_->value("Run/OpenOutput", false).toBool()) {
              QDesktopServices::openUrl(QUrl::fromLocalFile(
                  settings_->value("Process/LastOutputDir").toString()));
            }
          });

  // Processing page.
  connect(process_page_, &ProcessPage::RequestCheck, this,
          [this](QStringList inputs) { emit OnSendCheck(inputs, DbPath()); });

  connect(process_page_, &ProcessPage::RequestStart, this,
          [this](QStringList inputs, QStringList outputs, QVariantMap options) {
            log_panel_->BeginRun(
                tr("Processing %n subtitle(s)", "", inputs.size()));

            options["db_path"] = DbPath();
            options["multi_thread"] =
                settings_->value("Run/MultiThread", false).toBool();
            options["non_ttf_quiet"] =
                !settings_->value("Run/WarnNonTtf", true).toBool();
            options["update_db"] =
                process_page_->IsAutoUpdate() && !is_retry_pending_;
            options["fonts_dirs"] = library_page_->FontDirs();
            settings_->setValue("Process/LastOutputDir", outputs.value(0));

            emit OnSendStart(inputs, outputs, options);
          });

  connect(process_page_, &ProcessPage::RequestStop, this,
          [this]() { worker_->RequestStop(); });

  connect(process_page_, &ProcessPage::RequestUpdateAndRetry, this,
          [this](QStringList inputs) {
            is_retry_pending_ = true;
            UpdateLibrary(false);
            emit OnSendCheck(inputs, DbPath());
            process_page_->Start(inputs);
          });

  connect(process_page_, &ProcessPage::RequestPackScan, this,
          [this](QStringList inputs, QString pack_dir) {
            emit OnSendPackScan(inputs, pack_dir);
          });

  connect(process_page_, &ProcessPage::RequestImport, this,
          [this](QStringList sources, QString target_dir) {
            log_panel_->BeginRun(
                tr("Importing %n font file(s)", "", sources.size()));
            emit OnSendImport(sources, target_dir, DbPath());
          });

  connect(process_page_, &ProcessPage::RequestShowLibrary, this,
          [this](QString search) {
            ShowPage(1);
            if (!search.isEmpty()) {
              library_page_->ShowSearch(search);
            }
          });

  connect(process_page_, &ProcessPage::AutoUpdateChanged, this,
          [this](bool checked) {
            library_page_->SetAutoUpdate(checked);
            settings_->setValue("Library/AutoUpdate", checked);
          });

  // Library page.
  connect(library_page_, &LibraryPage::RequestBuild, this,
          &MainWindow::UpdateLibrary);

  connect(library_page_, &LibraryPage::DirsChanged, this, [this]() {
    process_page_->RefreshLibraryStatus(library_page_->FontDirs().size());
    UpdateLibrary(false);
  });

  connect(library_page_, &LibraryPage::AutoUpdateChanged, this,
          [this](bool checked) {
            process_page_->SetAutoUpdate(checked);
            settings_->setValue("Library/AutoUpdate", checked);
          });

  connect(library_page_, &LibraryPage::ChangesScanned, this,
          [this](bool has_changes) {
            if (is_startup_scan_) {
              is_startup_scan_ = false;
              if (has_changes &&
                  settings_->value("Library/CheckOnStart", true).toBool()) {
                UpdateLibrary(false);
              }
            }
          });

  // Settings page.
  connect(settings_page_, &SettingsPage::LanguageChanged, this, [this]() {
    if (ConfirmIdle()) {
      SaveSettings();
      emit RestartRequested();
    }
  });

  connect(settings_page_, &SettingsPage::ThemeChanged, this,
          &MainWindow::ApplyTheme);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Follows the system switching between light and dark while running.
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
          this, [this]() {
            if (settings_->value("General/Theme", 0).toInt() == 0) {
              ApplyTheme();
            }
          });
#endif

  connect(settings_page_, &SettingsPage::DatabaseDirChanged, this,
          [this]() { ReloadDatabase(); });

  connect(settings_page_, &SettingsPage::AutoClearLogChanged, log_panel_,
          &LogPanel::SetAutoClear);
  connect(log_panel_, &LogPanel::AutoClearChanged, this, [this](bool checked) {
    settings_->setValue("Log/AutoClear", checked);
    settings_page_->SetAutoClearLog(checked);
  });

  connect(settings_page_, &SettingsPage::ResetRequested, this, [this]() {
    if (!ConfirmIdle()) {
      return;
    }
    const QStringList dirs = library_page_->FontDirs();
    const QString db = DbPath();
    settings_->clear();
    settings_->setValue("Library/Dirs", dirs);
    settings_->setValue("Library/Database", db);
    settings_->sync();
    emit RestartRequested();
  });

  connect(settings_page_, &SettingsPage::CheckUpdateRequested, this,
          &MainWindow::ShowCheckWindow);
}

QString MainWindow::DbPath() const {
  return settings_->value("Library/Database", AppDataDir()).toString();
}

void MainWindow::ReloadDatabase() {
  font_db_.Load(DbPath());
  library_page_->Refresh();
  process_page_->RefreshLibraryStatus(library_page_->FontDirs().size());
}

void MainWindow::ShowPage(int index) {
  stack_->setCurrentIndex(index);
  if (nav_group_->button(index) != nullptr) {
    nav_group_->button(index)->setChecked(true);
  }
}

void MainWindow::ApplyTheme() {
  theme::Apply(*qApp, static_cast<theme::Mode>(
                          settings_->value("General/Theme", 0).toInt()));
  ui::RefreshIcons(this);
  process_page_->RefreshTheme();
  library_page_->RefreshTheme();
  settings_page_->RefreshTheme();
  log_panel_->RefreshTheme();
}

void MainWindow::UpdateLibrary(bool is_rebuild) {
  log_panel_->BeginRun(is_rebuild ? tr("Rebuilding font library")
                                  : tr("Updating font library"));
  emit OnSendBuild(library_page_->FontDirs(), DbPath(), is_rebuild);
}

void MainWindow::ShowCheckWindow() {
  if (check_window_ == nullptr) {
    check_window_ = new CheckWindow(this);
    check_window_->setAttribute(Qt::WA_DeleteOnClose);
    connect(check_window_, &QDialog::destroyed, this,
            [this]() { check_window_ = nullptr; });
  }

  check_window_->show();
  check_window_->activateWindow();
}

bool MainWindow::ConfirmIdle() {
  if (!worker_->IsRunning()) {
    return true;
  }
  QMessageBox box(QMessageBox::Warning, tr("Still working"),
                  tr("Please wait until the current task finishes."),
                  QMessageBox::NoButton, this);
  box.addButton(tr("OK"), QMessageBox::AcceptRole);
  box.exec();
  return false;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (!ConfirmIdle()) {
    event->ignore();
    return;
  }

  SaveSettings();
  event->accept();
}

void MainWindow::changeEvent(QEvent* event) {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
  // Newer Qt reports this through QStyleHints::colorSchemeChanged instead.
  if (event->type() == QEvent::ThemeChange &&
      settings_->value("General/Theme", 0).toInt() == 0) {
    const bool was_dark = theme::IsDark();
    theme::Apply(*qApp, theme::Mode::kSystem);
    if (was_dark != theme::IsDark()) {
      ApplyTheme();
    }
  }
#endif

  QMainWindow::changeEvent(event);
}

void MainWindow::LoadSettings() {
  if (settings_->contains("MainWindow/Geometry")) {
    restoreGeometry(settings_->value("MainWindow/Geometry").toByteArray());
  } else {
    resize(1180, 860);
  }
  SetSidebarCollapsed(
      settings_->value("MainWindow/SidebarCollapsed", false).toBool(), false);

  const bool is_auto_update =
      settings_->value("Library/AutoUpdate", true).toBool();
  process_page_->SetAutoUpdate(is_auto_update);
  library_page_->SetAutoUpdate(is_auto_update);

  log_panel_->SetAutoClear(settings_->value("Log/AutoClear", true).toBool());
  log_panel_->SetExpanded(settings_->value("Log/Expanded", false).toBool());
}

void MainWindow::SaveSettings() {
  settings_->setValue("MainWindow/Geometry", saveGeometry());
  settings_->setValue("MainWindow/SidebarCollapsed", is_sidebar_collapsed_);
  settings_->setValue("Log/Expanded", log_panel_->IsExpanded());
  process_page_->SaveSettings();
  library_page_->SaveSettings();
  settings_->sync();
}

void MainWindow::MigrateOldSettings() {
  // Settings written by the old single-window UI.
  if (settings_->contains("InputsText/Database") &&
      !settings_->contains("Library/Database")) {
    settings_->setValue("Library/Database",
                        settings_->value("InputsText/Database"));
  }

  if (settings_->contains("InputsText/Font") &&
      !settings_->contains("Library/Dirs")) {
    QStringList dirs;
    for (const auto& dir :
         settings_->value("InputsText/Font").toString().split(';')) {
      if (!dir.trimmed().isEmpty() && QDir(dir.trimmed()).exists()) {
        dirs.append(dir.trimmed());
      }
    }
    settings_->setValue("Library/Dirs", dirs);
  }

  if (settings_->contains("Options/Hdr") &&
      !settings_->contains("Process/Hdr")) {
    settings_->setValue("Process/Hdr", settings_->value("Options/Hdr"));
    settings_->setValue("Process/Rename", settings_->value("Options/Rename"));
    int mode = 0;
    if (settings_->value("Options/Subset").toBool()) {
      mode = 1;
    } else if (settings_->value("Options/Embed").toBool()) {
      mode = 2;
    }
    settings_->setValue("Process/Mode", mode);
  }

  if (settings_->contains("Menu/MultiThread") &&
      !settings_->contains("Run/MultiThread")) {
    settings_->setValue("Run/MultiThread",
                        settings_->value("Menu/MultiThread"));
    settings_->setValue("Process/Combined",
                        settings_->value("Menu/CombinedFonts"));
  }

  for (const auto& group :
       {"MainWindow/WSize", "MainWindow/HSize", "MainWindow/WPos",
        "MainWindow/HPos"}) {
    settings_->remove(group);
  }
  settings_->remove("InputsText");
  settings_->remove("Options");
  settings_->remove("Menu");
}
