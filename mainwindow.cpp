/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "mainwindow.h"

#include "DownloadGamePage.h"
#include "DownloadModsPage.h"
#include "DownloadCenterPage.h"
#include "HeaderBar.h"
#include "HomePage.h"
#include "LogoMark.h"
#include "MorePage.h"
#include "MultiplayerPage.h"
#include "ModsDrawer.h"
#include "BackgroundStore.h"
#include "NavigationBar.h"
#include "SampleData.h"
#include "SettingsPage.h"
#include "Theme.h"
#include "LoadingIndicator.h"
#include "account/AccountDialog.h"
#include "account/AccountManager.h"
#include "account/AuthlibInjector.h"
#include "account/ExternalAuth.h"
#include "minecraft/GameLauncher.h"
#include "minecraft/JavaRuntimeInstaller.h"
#include "platform/WindowEffects.h"
#include "settings/GlobalSettings.h"
#include "settings/VersionSettings.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEnterEvent>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QImageReader>
#include <QKeySequence>
#include <QLabel>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QStackedWidget>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <string>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {

#ifdef Q_OS_WIN
struct GameWindowSearch {
    DWORD processId = 0;
    bool found = false;
};

BOOL CALLBACK findGameWindow(HWND window, LPARAM parameter)
{
    auto *search = reinterpret_cast<GameWindowSearch *>(parameter);
    if (!IsWindowVisible(window)) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (search->processId != 0 && processId != search->processId) {
        return TRUE;
    }

    wchar_t className[256];
    const int classLength = GetClassNameW(window, className, 255);
    if (classLength > 0) {
        className[classLength] = L'\0';
    } else {
        className[0] = L'\0';
    }

    wchar_t title[512];
    int copied = 0;
    const int length = GetWindowTextLengthW(window);
    if (length > 0) {
        copied = GetWindowTextW(window, title, 511);
        if (copied < 0) {
            copied = 0;
        }
        title[copied] = L'\0';
    }

    const QString text = QString::fromWCharArray(title, copied);
    const QString windowClass = QString::fromWCharArray(className, classLength);
    const bool looksLikeMinecraft =
        text.contains(QStringLiteral("Minecraft"), Qt::CaseInsensitive)
        || windowClass.contains(QStringLiteral("GLFW"), Qt::CaseInsensitive)
        || windowClass.contains(QStringLiteral("LWJGL"), Qt::CaseInsensitive);

    if (search->processId == 0) {
        if (!text.contains(QStringLiteral("Minecraft"), Qt::CaseInsensitive)) {
            return TRUE;
        }
    } else if (!looksLikeMinecraft) {
        return TRUE;
    }

    search->found = true;
    return FALSE;
}

bool gameWindowVisible(qint64 processId)
{
    GameWindowSearch search;
    search.processId = static_cast<DWORD>(qMax<qint64>(0, processId));
    EnumWindows(findGameWindow, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

bool processStillRunning(qint64 processId)
{
    if (processId <= 0) {
        return true;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) {
        const DWORD error = GetLastError();
        return error != ERROR_INVALID_PARAMETER;
    }
    DWORD exitCode = 0;
    const bool queried = GetExitCodeProcess(process, &exitCode) != FALSE;
    CloseHandle(process);
    return !queried || exitCode == STILL_ACTIVE;
}

void terminateProcess(qint64 processId)
{
    if (processId <= 0) {
        return;
    }
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) {
        return;
    }
    TerminateProcess(process, 1);
    WaitForSingleObject(process, 3000);
    CloseHandle(process);
}
#else
bool gameWindowVisible(qint64)
{
    return true;
}

bool processStillRunning(qint64)
{
    return true;
}

void terminateProcess(qint64)
{
}
#endif

void trimLauncherWorkingSet()
{
#ifdef Q_OS_WIN
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1),
                             static_cast<SIZE_T>(-1));
#endif
}

bool setProcessPriority(qint64 processId, const QString &priority)
{
#ifdef Q_OS_WIN
    if (processId <= 0) {
        return false;
    }
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    if (priority == QStringLiteral("high")) {
        priorityClass = HIGH_PRIORITY_CLASS;
    } else if (priority == QStringLiteral("above_normal")) {
        priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
    } else if (priority == QStringLiteral("below_normal")) {
        priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
    } else if (priority == QStringLiteral("low")) {
        priorityClass = IDLE_PRIORITY_CLASS;
    }
    HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process) {
        return false;
    }
    const bool applied = SetPriorityClass(process, priorityClass) != FALSE;
    CloseHandle(process);
    return applied;
#else
    Q_UNUSED(processId)
    Q_UNUSED(priority)
    return false;
#endif
}

#ifdef Q_OS_WIN
struct WindowTitleSearch {
    DWORD processId = 0;
    std::wstring title;
    bool found = false;
};

BOOL CALLBACK setWindowTitleCallback(HWND window, LPARAM parameter)
{
    auto *search = reinterpret_cast<WindowTitleSearch *>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != search->processId || !IsWindowVisible(window)) {
        return TRUE;
    }
    SetWindowTextW(window, search->title.c_str());
    search->found = true;
    return FALSE;
}
#endif

void setGameWindowTitle(qint64 processId, const QString &title)
{
    if (processId <= 0 || title.trimmed().isEmpty()) {
        return;
    }
#ifdef Q_OS_WIN
    WindowTitleSearch search;
    search.processId = static_cast<DWORD>(processId);
    search.title = title.toStdWString();
    EnumWindows(setWindowTitleCallback, reinterpret_cast<LPARAM>(&search));
#else
    Q_UNUSED(processId)
    Q_UNUSED(title)
#endif
}

void applyHighPerformanceGpuPreference(const QString &javaPath)
{
#ifdef Q_OS_WIN
    if (javaPath.isEmpty()) {
        return;
    }
    QSettings preferences(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\DirectX\\UserGpuPreferences"),
        QSettings::NativeFormat);
    preferences.setValue(QDir::toNativeSeparators(QFileInfo(javaPath).absoluteFilePath()),
                         QStringLiteral("GpuPreference=2;"));
    preferences.sync();
#else
    Q_UNUSED(javaPath)
#endif
}

#ifdef Q_OS_WIN
struct BorderlessWindowSearch {
    DWORD processId = 0;
};

BOOL CALLBACK makeWindowBorderless(HWND window, LPARAM parameter)
{
    auto *search = reinterpret_cast<BorderlessWindowSearch *>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != search->processId || !IsWindowVisible(window)) {
        return TRUE;
    }

    wchar_t className[256];
    const int classLength = GetClassNameW(window, className, 255);
    className[qMax(0, classLength)] = L'\0';
    wchar_t title[512];
    const int titleLength = GetWindowTextW(window, title, 511);
    title[qMax(0, titleLength)] = L'\0';
    const QString titleText = QString::fromWCharArray(title, qMax(0, titleLength));
    const QString classText = QString::fromWCharArray(className, qMax(0, classLength));
    if (!titleText.contains(QStringLiteral("Minecraft"), Qt::CaseInsensitive)
        && !classText.contains(QStringLiteral("GLFW"), Qt::CaseInsensitive)
        && !classText.contains(QStringLiteral("LWJGL"), Qt::CaseInsensitive)) {
        return TRUE;
    }

    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_POPUP | WS_VISIBLE;
    SetWindowLongPtrW(window, GWL_STYLE, style);

    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        const RECT &bounds = monitorInfo.rcMonitor;
        SetWindowPos(window, HWND_TOP, bounds.left, bounds.top,
                     bounds.right - bounds.left, bounds.bottom - bounds.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    return FALSE;
}
#endif

void applyBorderlessWindow(qint64 processId)
{
    if (processId <= 0) {
        return;
    }
#ifdef Q_OS_WIN
    BorderlessWindowSearch search;
    search.processId = static_cast<DWORD>(processId);
    EnumWindows(makeWindowBorderless, reinterpret_cast<LPARAM>(&search));
#else
    Q_UNUSED(processId)
#endif
}

QString readLaunchLog(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QByteArray contents = file.readAll();
    constexpr qsizetype maxCharacters = 6000;
    if (contents.size() > maxCharacters) {
        contents = contents.right(maxCharacters);
    }
    return QString::fromLocal8Bit(contents).trimmed();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("MysticTurtle"));
    setWindowIcon(LogoMark::appIcon());
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint
                   | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(false);
    setMinimumSize(1060, 680);
    resize(1280, 720);

    const QSettings settings;
    m_acrylicEnabled = settings.value(QStringLiteral("ui/acrylic"), false).toBool();
    m_animationsEnabled = settings.value(QStringLiteral("ui/animations"), true).toBool();
    m_liquidGlassEnabled =
        settings.value(QStringLiteral("ui/liquidGlass"), true).toBool();
    m_buttonSpotlightEnabled =
        settings.value(QStringLiteral("ui/buttonSpotlight"), true).toBool();
    Theme::setAnimationsEnabled(m_animationsEnabled);
    Theme::setLiquidGlassEnabled(m_liquidGlassEnabled);
    Theme::setButtonSpotlightEnabled(m_buttonSpotlightEnabled);
    m_backgroundOpacity =
        qBound(0, settings.value(QStringLiteral("ui/backgroundOpacity"), 35).toInt(), 100);
    const QString backgroundImage =
        settings.value(QStringLiteral("ui/backgroundImage")).toString();
    if (!backgroundImage.isEmpty()) {
        QImageReader reader(backgroundImage);
        reader.setAutoTransform(true);
        m_backgroundImage = reader.read();
    }
    BackgroundStore::instance().setBackground(m_backgroundImage, m_backgroundOpacity);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        const QSize target = QSize(1280, 720).boundedTo(available.size() - QSize(80, 80))
                                 .expandedTo(minimumSize());
        resize(target);
        move(available.center() - rect().center());
    }

    setupUi();
    updateWindowChrome();
    if (!backgroundImage.isEmpty() && m_backgroundImage.isNull()) {
        showToast(QStringLiteral("无法加载已保存的背景图片，请重新选择或清除。"));
    }
    QTimer::singleShot(900, this, [this]() {
        showToast(QStringLiteral(
            "版本隔离已开启：各版本模组分开存放；根目录 mods 不会自动迁移或删除。"));
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new HeaderBar(this);
    root->addWidget(m_header);

    m_pages = new QStackedWidget(this);
    m_pages->setAttribute(Qt::WA_NoSystemBackground);
    m_pages->setAutoFillBackground(false);
    m_pageOpacityEffect = new QGraphicsOpacityEffect(m_pages);
    m_pageOpacityEffect->setOpacity(1.0);
    m_pages->setGraphicsEffect(m_pageOpacityEffect);
    m_homePage = new HomePage(m_pages);
    m_downloadCenterPage = new DownloadCenterPage(m_pages);
    m_downloadGamePage = m_downloadCenterPage->gamePage();
    m_downloadModsPage = m_downloadCenterPage->modsPage();
    m_settingsPage = new SettingsPage(m_pages);
    m_multiplayerPage = new MultiplayerPage(m_pages);
    m_morePage = new MorePage(m_pages);
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_downloadCenterPage);
    m_pages->addWidget(m_settingsPage);
    m_pages->addWidget(m_multiplayerPage);
    m_pages->addWidget(m_morePage);
    m_pages->setCurrentIndex(0);
    m_homePage->setAnimationsEnabled(m_animationsEnabled);
    m_morePage->setAnimationsEnabled(m_animationsEnabled);
    m_header->navigationBar()->setAnimationsEnabled(m_animationsEnabled);
    m_downloadCenterPage->setGameDirectory(m_homePage->gameDirectory());
    m_settingsPage->setGameDirectory(m_homePage->gameDirectory());
    root->addWidget(m_pages, 1);

    AccountManager &accountManager = AccountManager::instance();
    const auto refreshHeaderAccount = [this]() {
        const PlayerAccount account = AccountManager::instance().account();
        m_header->setAccount(account.isValid() ? account.username
                                               : QStringLiteral("未添加账户"),
                             account.isValid() ? account.typeLabel() : QString());
    };
    refreshHeaderAccount();
    connect(&accountManager, &AccountManager::accountChanged,
            this, [refreshHeaderAccount]() { refreshHeaderAccount(); });
    connect(m_header, &HeaderBar::accountRequested,
            m_homePage, &HomePage::openAccountDialog);

    connect(m_header, &HeaderBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_header, &HeaderBar::maximizeRestoreRequested, this, &MainWindow::toggleMaximize);
    connect(m_header, &HeaderBar::closeRequested, this, &QWidget::close);
    NavigationBar *navigation = m_header->navigationBar();
    connect(navigation, &NavigationBar::navigationRequested, this, &MainWindow::navigateTo);
    for (int i = 0; i < 5; ++i) {
        navigation->setItemEnabled(i, true);
    }

    connect(m_homePage, &HomePage::detailsRequested,
            this, &MainWindow::openVersionDetails);
    connect(m_homePage, &HomePage::notifyRequested, this, &MainWindow::showToast);
    connect(m_homePage, &HomePage::launchRequested,
            this, &MainWindow::launchSelectedVersion);
    connect(m_homePage, &HomePage::gameDirectoryChanged, this,
            [this](const QString &directory) {
                m_downloadCenterPage->setGameDirectory(directory);
                m_settingsPage->setGameDirectory(directory);
            });
    connect(m_morePage, &MorePage::downloadModpacksRequested, this, [this]() {
        m_downloadCenterPage->showModpacks();
        navigateTo(1);
    });
    connect(m_settingsPage, &SettingsPage::versionIsolationChanged,
            m_homePage, &HomePage::reloadLocalVersions);
    connect(m_settingsPage, &SettingsPage::homepageChanged,
            m_homePage, &HomePage::refreshHomepageContent);
    connect(m_downloadGamePage, &DownloadGamePage::installSucceeded, this,
            [this](const QString &) {
                m_homePage->reloadLocalVersions();
            });
    connect(m_downloadGamePage, &DownloadGamePage::baseInstallFinished, this,
            [this](const QString &) {
                m_homePage->reloadLocalVersions();
            });
    connect(m_downloadGamePage, &DownloadGamePage::notifyRequested,
            this, &MainWindow::showToast);
    connect(m_downloadCenterPage, &DownloadCenterPage::notifyRequested,
            this, &MainWindow::showToast);
    connect(m_downloadCenterPage, &DownloadCenterPage::modsChanged,
            m_homePage, &HomePage::reloadLocalVersions);
    connect(m_settingsPage, &SettingsPage::notifyRequested, this, &MainWindow::showToast);
    connect(m_settingsPage, &SettingsPage::acrylicEnabledChanged,
            this, &MainWindow::applyAcrylic);
    connect(m_settingsPage, &SettingsPage::backgroundChanged,
            this, &MainWindow::applyBackground);
    connect(m_settingsPage, &SettingsPage::animationsEnabledChanged,
            this, [this](bool enabled) {
                m_animationsEnabled = enabled;
                Theme::setAnimationsEnabled(enabled);
                if (!enabled) {
                    stopPageTransition();
                }
                m_homePage->setAnimationsEnabled(enabled);
                m_morePage->setAnimationsEnabled(enabled);
                m_header->navigationBar()->setAnimationsEnabled(enabled);
                for (QWidget *widget : findChildren<QWidget *>()) {
                    if (auto *indicator = dynamic_cast<LoadingIndicator *>(widget)) {
                        indicator->setAnimationsEnabled(enabled);
                    }
                }
                QSettings settings;
                settings.setValue(QStringLiteral("ui/animations"), enabled);
                showToast(enabled ? QStringLiteral("已启用丝滑动画。")
                                  : QStringLiteral("已关闭丝滑动画。"));
            });
    connect(m_settingsPage, &SettingsPage::liquidGlassChanged,
            this, [this](bool enabled) {
                m_liquidGlassEnabled = enabled;
                Theme::setLiquidGlassEnabled(enabled);
                for (QWidget *widget : findChildren<QWidget *>()) {
                    widget->update();
                }
                QSettings settings;
                settings.setValue(QStringLiteral("ui/liquidGlass"), enabled);
                showToast(enabled ? QStringLiteral("已启用液态玻璃质感。")
                                  : QStringLiteral("已关闭液态玻璃质感。"));
            });
    connect(m_settingsPage, &SettingsPage::buttonSpotlightChanged,
            this, [this](bool enabled) {
                m_buttonSpotlightEnabled = enabled;
                Theme::setButtonSpotlightEnabled(enabled);
                for (QWidget *widget : findChildren<QWidget *>()) {
                    widget->update();
                }
                QSettings settings;
                settings.setValue(QStringLiteral("ui/buttonSpotlight"), enabled);
                showToast(enabled ? QStringLiteral("已启用按钮光标光效。")
                                  : QStringLiteral("已关闭按钮光标光效。"));
            });
    connect(m_settingsPage, &SettingsPage::gameDirectoryChanged, this,
            [this](const QString &directory) {
                m_homePage->setGameDirectory(directory);
                m_downloadCenterPage->setGameDirectory(directory);
                m_settingsPage->setGameDirectory(directory);
                showToast(QStringLiteral("游戏目录已更新。"));
            });

    for (int i = 0; i < 5; ++i) {
        if (!navigation->isItemEnabled(i)) {
            continue;
        }
        auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i]() { navigateTo(i); });
    }

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        if (!m_toast || !m_toast->isVisible()) {
            return;
        }
        if (m_toastAnimation) {
            m_toastAnimation->stop();
            m_toastAnimation->deleteLater();
            m_toastAnimation = nullptr;
        }
        auto *fade = new QPropertyAnimation(m_toastEffect, "opacity", this);
        fade->setDuration(220);
        fade->setStartValue(m_toastEffect->opacity());
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InCubic);
        connect(fade, &QPropertyAnimation::finished, this, [this, fade]() {
            if (m_toast) {
                m_toast->hide();
            }
            if (m_toastAnimation == fade) {
                m_toastAnimation = nullptr;
            }
            fade->deleteLater();
        });
        m_toastAnimation = fade;
        fade->start();
    });
}

void MainWindow::navigateTo(int index)
{
    if (index < 0 || index >= 5 || index >= m_pages->count()) {
        return;
    }
    if (!m_header->navigationBar()->isItemEnabled(index)) {
        return;
    }
    if (index == 4) {
        m_morePage->showOverview();
    }
    animatePageTo(index);
    m_header->navigationBar()->setCurrentIndex(index, true);
}

void MainWindow::animatePageTo(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count()) {
        return;
    }

    stopPageTransition();
    if (index == m_pages->currentIndex()) {
        return;
    }
    if (!m_animationsEnabled || !Theme::animationsEnabled()
        || !m_pageOpacityEffect) {
        m_pages->setCurrentIndex(index);
        return;
    }

    m_pendingPageIndex = index;
    auto *fadeOut = new QPropertyAnimation(m_pageOpacityEffect, "opacity", this);
    fadeOut->setDuration(130);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InCubic);
    m_pageFadeAnimation = fadeOut;
    connect(fadeOut, &QPropertyAnimation::finished, this, [this, fadeOut]() {
        if (m_pageFadeAnimation != fadeOut || !m_pages
            || m_pendingPageIndex < 0) {
            fadeOut->deleteLater();
            return;
        }

        m_pages->setCurrentIndex(m_pendingPageIndex);
        m_pendingPageIndex = -1;
        fadeOut->deleteLater();

        auto *fadeIn = new QPropertyAnimation(m_pageOpacityEffect, "opacity", this);
        fadeIn->setDuration(220);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::OutCubic);
        m_pageFadeAnimation = fadeIn;
        connect(fadeIn, &QPropertyAnimation::finished, this, [this, fadeIn]() {
            if (m_pageFadeAnimation == fadeIn) {
                m_pageFadeAnimation = nullptr;
            }
            if (m_pageOpacityEffect) {
                m_pageOpacityEffect->setOpacity(1.0);
            }
            fadeIn->deleteLater();
        });
        fadeIn->start();
    });
    fadeOut->start();
}

void MainWindow::stopPageTransition()
{
    if (m_pageFadeAnimation) {
        m_pageFadeAnimation->stop();
        m_pageFadeAnimation->deleteLater();
        m_pageFadeAnimation = nullptr;
    }
    if (m_pendingPageIndex >= 0 && m_pages
        && m_pendingPageIndex < m_pages->count()) {
        m_pages->setCurrentIndex(m_pendingPageIndex);
    }
    m_pendingPageIndex = -1;
    if (m_pageOpacityEffect) {
        m_pageOpacityEffect->setOpacity(1.0);
    }
}

void MainWindow::toggleMaximize()
{
    if (isMaximized() || isFullScreen()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::updateWindowChrome()
{
    const bool maximized = isMaximized() || isFullScreen();
    const int margin = maximized ? 0 : 10;
    if (auto *root = layout()) {
        root->setContentsMargins(margin, margin, margin, margin);
    }
    if (m_header) {
        m_header->setMaximized(maximized);
    }
    if (m_nativeEffectsApplied) {
        WindowEffects::setRoundedCorners(this, false);
    }
    update();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_nativeEffectsApplied) {
        m_nativeEffectsApplied = true;
        WindowEffects::apply(this, m_acrylicEnabled, false);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateWindowChrome();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionToast();
}

void MainWindow::applyAcrylic(bool enabled)
{
    m_acrylicEnabled = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("ui/acrylic"), enabled);
    if (isVisible()) {
        WindowEffects::setAcrylic(this, enabled);
    }
    update();
    showToast(enabled ? QStringLiteral("已启用 Acrylic 毛玻璃背景。")
                      : QStringLiteral("已关闭毛玻璃，切换为实心背景。"));
}

void MainWindow::openVersionDetails(int versionIndex)
{
    const VersionInfo version = m_homePage->versionAt(versionIndex);
    if (version.minecraft.isEmpty()) {
        return;
    }
    if (m_drawer) {
        m_drawer->raise();
        return;
    }

    auto *drawer = new ModsDrawer(version, this);
    m_drawer = drawer;
    connect(drawer, &ModsDrawer::modsChanged, this, [this]() {
        m_homePage->reloadLocalVersions();
    });
    connect(drawer, &ModsDrawer::notifyRequested, this, &MainWindow::showToast);
    drawer->setGeometry(rect());
    drawer->show();
    drawer->raise();
    drawer->open();
}

void MainWindow::launchSelectedVersion()
{
    m_launchCancelled = false;
    m_launchPid = 0;
    m_pendingLaunchToast.clear();
    const VersionInfo version = m_homePage->selectedVersion();
    if (version.minecraft.isEmpty() || version.versionId.isEmpty()) {
        showToast(QStringLiteral("请先选择一个本地游戏版本。"));
        return;
    }

    AccountManager &manager = AccountManager::instance();
    if (manager.accounts().isEmpty()) {
        AccountDialog dialog(this);
        dialog.exec();
        if (!manager.hasAccount()) {
            showToast(QStringLiteral("启动游戏前需要先创建一个账户。"));
            return;
        }
    } else {
        QStringList labels;
        int activeIndex = 0;
        const QVector<PlayerAccount> accounts = manager.accounts();
        for (int i = 0; i < accounts.size(); ++i) {
            QString label = QStringLiteral("%1 · %2")
                                .arg(accounts.at(i).username,
                                     accounts.at(i).typeLabel());
            if (accounts.at(i).type == PlayerAccount::Type::External
                && !accounts.at(i).serverLabel().isEmpty()) {
                label += QStringLiteral(" · ") + accounts.at(i).serverLabel();
            }
            labels.append(label);
            if (accounts.at(i).id == manager.activeAccountId()) {
                activeIndex = i;
            }
        }

        bool ok = false;
        const QString selected = QInputDialog::getItem(
            this, QStringLiteral("选择用户"), QStringLiteral("以哪个用户启动游戏？"),
            labels, activeIndex, false, &ok);
        if (!ok || selected.isEmpty()) {
            return;
        }
        const int selectedIndex = labels.indexOf(selected);
        if (selectedIndex < 0 || selectedIndex >= accounts.size()) {
            return;
        }
        manager.setActiveAccount(accounts.at(selectedIndex).id);
    }

    const PlayerAccount account = manager.account();
    if (!account.isValid()) {
        showToast(QStringLiteral("请选择一个有效账户。"));
        return;
    }

    QTimer::singleShot(0, this, [this, version, account]() {
        startSelectedVersion(version, account);
    });
}

void MainWindow::startSelectedVersion(const VersionInfo &version,
                                      const PlayerAccount &account)
{
    const QString gameDirectory = m_homePage->gameDirectory();
    m_settingsPage->flushGlobalSettings();
    QString versionSettingsError;
    const VersionSettings versionSettings =
        VersionSettings::load(version.versionDirectory, &versionSettingsError);
    if (!versionSettingsError.isEmpty()) {
        closeLaunchProgress();
        QMessageBox::warning(this, QStringLiteral("版本独立设置无效"),
                             versionSettingsError);
        return;
    }
    const GlobalSettings globalSettings =
        versionSettings.applyTo(GlobalSettings::load());
    m_launchVisibility = globalSettings.launch.launcherVisibility;

    PlayerAccount launchAccount = account;
    QString authlibInjectorJar;
    if (launchAccount.type == PlayerAccount::Type::External) {
        updateLaunchProgress(6, QStringLiteral("正在验证外置账户…"));
        QString authError;
        if (!ExternalAuth::authenticate(
                &launchAccount, &authError,
                [this]() { return m_launchCancelled; })) {
            closeLaunchProgress();
            if (m_launchCancelled) {
                showToast(QStringLiteral("游戏启动已取消。"));
            } else {
                QMessageBox::warning(
                    this, QStringLiteral("外置登录失败"),
                    QStringLiteral("无法通过该账户的认证服务器登录：\n%1")
                        .arg(authError));
            }
            return;
        }
        QString sessionError;
        if (!AccountManager::instance().updateExternalSession(
                launchAccount.id, launchAccount.accessToken,
                launchAccount.clientToken, launchAccount.uuid,
                launchAccount.username, &sessionError)) {
            closeLaunchProgress();
            QMessageBox::warning(this, QStringLiteral("保存账户失败"), sessionError);
            return;
        }
        if (m_launchCancelled) {
            closeLaunchProgress();
            showToast(QStringLiteral("游戏启动已取消。"));
            return;
        }

        updateLaunchProgress(10, QStringLiteral("正在准备 authlib-injector…"));
        QString injectorError;
        authlibInjectorJar = AuthlibInjector::ensureJar(
            &injectorError, [this]() { return m_launchCancelled; });
        if (authlibInjectorJar.isEmpty()) {
            closeLaunchProgress();
            if (m_launchCancelled) {
                showToast(QStringLiteral("游戏启动已取消。"));
            } else {
                QMessageBox::warning(
                    this, QStringLiteral("外置登录组件缺失"),
                    QStringLiteral("无法准备 authlib-injector.jar：\n%1\n\n"
                                   "也可以将 authlib-injector.jar 放到启动器目录下重试。")
                        .arg(injectorError));
            }
            return;
        }
    }

    updateLaunchProgress(authlibInjectorJar.isEmpty() ? 8 : 16,
                         QStringLiteral("正在检查 Java 运行时…"));
    const int requiredJava =
        GameLauncher::requiredJavaMajor(gameDirectory, version.versionId);
    QString javaError;
    QString javaPath;
    const QString configuredJava = globalSettings.advanced.javaPath;
    if (!configuredJava.isEmpty() && QFileInfo::exists(configuredJava)) {
        const int configuredMajor = GameLauncher::javaMajorVersion(configuredJava);
        const bool compatible = requiredJava <= 0
            || (requiredJava <= 8 ? configuredMajor == requiredJava
                                  : configuredMajor >= requiredJava);
        if (compatible) {
            javaPath = configuredJava;
        }
    }
    if (javaPath.isEmpty()) {
        javaPath = GameLauncher::findCompatibleJava(gameDirectory, requiredJava, &javaError);
    }
    if (m_launchCancelled) {
        closeLaunchProgress();
        showToast(QStringLiteral("游戏启动已取消。"));
        return;
    }

    if (javaPath.isEmpty()) {
        closeLaunchProgress();
        if (!globalSettings.advanced.autoDownloadJava) {
            QMessageBox::warning(
                this, QStringLiteral("缺少 Java"),
                QStringLiteral("没有找到适用于 Minecraft %1 的 Java。\n\n"
                               "「允许启动器自动下载 Java 运行时」已关闭，请先在高级设置中选择 Java。")
                    .arg(version.minecraft));
            return;
        }
        const int downloadMajor = requiredJava > 0 ? requiredJava : 17;
        const QString javaDiagnostic = javaError.isEmpty()
            ? QStringLiteral("未找到可运行且版本兼容的 Java。")
            : javaError;
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("需要安装 Java"),
            QStringLiteral("没有找到适用于 Minecraft %1 的 Java %2。\n\n"
                           "检测详情：%3\n\n"
                           "是否自动下载并安装 Temurin JDK %2？\n"
                           "下载大小约 150–250 MB。")
                .arg(version.minecraft)
                .arg(downloadMajor)
                .arg(javaDiagnostic),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes) {
            showToast(QStringLiteral("已取消启动：需要安装 Java。"));
            return;
        }

        QString installError;
        if (!JavaRuntimeInstaller::download(gameDirectory, downloadMajor, this,
                                            &javaPath, &installError)) {
            if (installError.contains(QStringLiteral("已取消"))) {
                showToast(QStringLiteral("Java 下载已取消，游戏未启动。"));
                return;
            }
            QMessageBox::warning(this, QStringLiteral("Java 安装失败"),
                                 QStringLiteral("%1\n\nJava 检测详情：%2")
                                     .arg(installError, javaDiagnostic));
            return;
        }
        if (javaPath.isEmpty() || !QFileInfo::exists(javaPath)) {
            closeLaunchProgress();
            QMessageBox::warning(
                this, QStringLiteral("Java 安装失败"),
                QStringLiteral("安装程序返回成功，但找不到 Java 可执行文件：%1")
                    .arg(javaPath.isEmpty() ? QStringLiteral("（路径为空）") : javaPath));
            return;
        }
        updateLaunchProgress(30, QStringLiteral("Java 安装完成，正在准备启动…"));
    } else {
        updateLaunchProgress(25, QStringLiteral("已找到兼容的 Java，正在准备启动…"));
    }

    if (!javaPath.isEmpty()) {
        QSettings settings;
        settings.setValue(QStringLiteral("game/javaPath"), javaPath);
        settings.sync();
        m_settingsPage->setJavaPath(javaPath);
    }

    QString error;
    qint64 processId = 0;
    const bool isolated = globalSettings.isolatesVersion(version.loader);
    const QString gameDataDirectory = isolated
        ? (version.gameDataDirectory.trimmed().isEmpty()
               ? QDir(version.versionDirectory).absolutePath()
               : version.gameDataDirectory)
        : gameDirectory;
    // Minecraft resolves saves, resource packs, options.txt and servers.dat
    // relative to --gameDir, so using the instance directory redirects these
    // paths without mutating or replacing data in the shared game root.
    const QStringList instanceDirectories = {
        QStringLiteral("mods"), QStringLiteral("saves"),
        QStringLiteral("resourcepacks"), QStringLiteral("shaderpacks"),
        QStringLiteral("config"), QStringLiteral("logs")
    };
    if (!QDir().mkpath(gameDataDirectory)) {
        closeLaunchProgress();
        showToast(QStringLiteral("无法创建游戏数据目录：%1").arg(gameDataDirectory));
        return;
    }
    for (const QString &directory : instanceDirectories) {
        if (!QDir().mkpath(QDir(gameDataDirectory).filePath(directory))) {
            closeLaunchProgress();
            showToast(QStringLiteral("无法创建版本隔离目录：%1")
                          .arg(QDir(gameDataDirectory).filePath(directory)));
            return;
        }
    }
    m_launchStderrLogPath =
        QDir(gameDataDirectory).filePath(
            QStringLiteral("logs/mysticturtle-launcher.stderr.log"));

    const QString preLaunchCommand = globalSettings.advanced.preLaunchCommand.trimmed();
    if (!preLaunchCommand.isEmpty()) {
        updateLaunchProgress(28, QStringLiteral("正在执行启动前命令…"));
        QProcess commandProcess;
#ifdef Q_OS_WIN
        commandProcess.setProgram(QStringLiteral("cmd.exe"));
        commandProcess.setArguments({QStringLiteral("/c"), preLaunchCommand});
#else
        commandProcess.setProgram(QStringLiteral("/bin/sh"));
        commandProcess.setArguments({QStringLiteral("-c"), preLaunchCommand});
#endif
        commandProcess.setWorkingDirectory(gameDataDirectory);
        commandProcess.start();
        if (!commandProcess.waitForStarted(5000)
            || !commandProcess.waitForFinished(30000)
            || commandProcess.exitStatus() != QProcess::NormalExit
            || commandProcess.exitCode() != 0) {
            const QString output = QString::fromLocal8Bit(
                commandProcess.readAllStandardError()
                + commandProcess.readAllStandardOutput()).trimmed();
            closeLaunchProgress();
            QMessageBox::warning(
                this, QStringLiteral("启动前命令失败"),
                QStringLiteral("命令未成功执行：\n%1\n\n%2")
                    .arg(preLaunchCommand,
                         output.isEmpty() ? QStringLiteral("（无输出）") : output));
            return;
        }
    }

    if (globalSettings.memory.optimizeBeforeLaunch) {
        trimLauncherWorkingSet();
    }
    if (globalSettings.advanced.preferHighPerformanceGpu) {
        applyHighPerformanceGpuPreference(javaPath);
    }

    const bool launched = GameLauncher::launch(
        gameDirectory, version.versionId, launchAccount, javaPath,
        [this](int percent, const QString &status) {
            updateLaunchProgress(30 + qRound(percent * 0.7), status);
        },
        &processId,
        &error,
        gameDataDirectory,
        [this]() { return m_launchCancelled; },
        authlibInjectorJar);

    if (!launched) {
        closeLaunchProgress();
        if (m_launchCancelled) {
            showToast(QStringLiteral("游戏启动已取消。"));
            return;
        }
        showToast(QStringLiteral("启动失败：%1").arg(error));
        return;
    }

    if (m_launchCancelled) {
        terminateProcess(processId);
        closeLaunchProgress();
        showToast(QStringLiteral("游戏启动已取消。"));
        return;
    }

    if (processId > 0
        && !setProcessPriority(processId, globalSettings.launch.processPriority)) {
        showToast(QStringLiteral("无法应用进程优先级设置。"));
    }

    updateLaunchProgress(100, QStringLiteral("等待窗口出现"));
    m_pendingLaunchToast = QStringLiteral("已使用%1「%2」启动 Minecraft %3。")
                               .arg(launchAccount.typeLabel(),
                                    launchAccount.username, version.minecraft);
    startGameWindowWait(processId);
}

void MainWindow::updateLaunchProgress(int percent, const QString &status)
{
    if (!m_launchProgress) {
        auto *dialog = new QDialog(this);
        m_launchProgress = dialog;
        dialog->setObjectName(QStringLiteral("launchProgressDialog"));
        dialog->setWindowModality(Qt::ApplicationModal);
        m_launchProgress->setWindowTitle(QStringLiteral("启动游戏"));
        dialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setMinimumSize(680, 200);
        dialog->resize(680, 200);

        auto *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(26, 22, 26, 22);
        layout->setSpacing(15);
        auto *label = new QLabel(status, dialog);
        label->setObjectName(QStringLiteral("launchProgressLabel"));
        label->setFont(Theme::font(13, QFont::DemiBold));
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        layout->addWidget(label, 1);

        auto *bar = new QProgressBar(dialog);
        bar->setObjectName(QStringLiteral("launchProgressBar"));
        bar->setRange(0, 100);
        bar->setTextVisible(false);
        bar->setFixedHeight(20);
        bar->setStyleSheet(QStringLiteral(
            "QProgressBar { min-height: 20px; max-height: 20px;"
            "border: none; border-radius: 10px; background: #1B2530; }"
            "QProgressBar::chunk { border-radius: 10px; background: #6EE7B7; }"));
        layout->addWidget(bar);

        auto *cancelButton = new QPushButton(QStringLiteral("取消启动"), dialog);
        cancelButton->setObjectName(QStringLiteral("cancelLaunchButton"));
        layout->addWidget(cancelButton, 0, Qt::AlignHCenter);
        connect(cancelButton, &QPushButton::clicked, this, [this]() {
            m_launchCancelled = true;
            if (m_launchPid > 0) {
                terminateProcess(m_launchPid);
                m_pendingLaunchToast.clear();
                closeLaunchProgress();
                showToast(QStringLiteral("已中断游戏启动。"));
            }
        });
        connect(dialog, &QDialog::rejected, this, [this]() {
            if (m_closingLaunchProgress) {
                return;
            }
            m_launchCancelled = true;
            m_pendingLaunchToast.clear();
            if (m_launchPid > 0) {
                terminateProcess(m_launchPid);
            }
        });
        dialog->show();
    }

    if (QLabel *label = m_launchProgress->findChild<QLabel *>(
            QStringLiteral("launchProgressLabel"))) {
        label->setText(status);
    }
    if (QProgressBar *bar = m_launchProgress->findChild<QProgressBar *>(
            QStringLiteral("launchProgressBar"))) {
        Theme::setProgressValue(bar, qBound(0, percent, 100));
    }
    QApplication::processEvents();
}

void MainWindow::closeLaunchProgress()
{
    if (m_gameWindowTimer) {
        m_gameWindowTimer->stop();
    }
    if (m_launchProgress) {
        QDialog *dialog = m_launchProgress.data();
        m_launchProgress = nullptr;
        m_closingLaunchProgress = true;
        dialog->close();
        m_closingLaunchProgress = false;
    }
}

void MainWindow::startGameWindowWait(qint64 processId)
{
    m_launchPid = processId;
    m_gameWindowWaitTicks = 0;

    if (!m_gameWindowTimer) {
        m_gameWindowTimer = new QTimer(this);
        m_gameWindowTimer->setInterval(400);
        connect(m_gameWindowTimer, &QTimer::timeout,
                this, &MainWindow::checkGameWindow);
    }
    m_gameWindowTimer->start();
    QTimer::singleShot(0, this, &MainWindow::checkGameWindow);
}

void MainWindow::checkGameWindow()
{
    if (!m_launchProgress) {
        if (m_gameWindowTimer) {
            m_gameWindowTimer->stop();
        }
        return;
    }

    ++m_gameWindowWaitTicks;
    if (m_launchCancelled) {
        terminateProcess(m_launchPid);
        closeLaunchProgress();
        m_pendingLaunchToast.clear();
        showToast(QStringLiteral("已中断游戏启动。"));
        return;
    }

    const bool byProcess = gameWindowVisible(m_launchPid);
    // Some modded launch chains create the final window from a child process.
    // Fall back to a Minecraft-titled window after a short grace period.
    const bool byTitle = m_gameWindowWaitTicks > 15 && gameWindowVisible(0);
    if (byProcess || byTitle) {
        closeLaunchProgress();
        const GlobalSettings launchSettings = GlobalSettings::load();
        QString title = launchSettings.launch.windowTitle.trimmed();
        const QString customInfo = launchSettings.launch.customInfo.trimmed();
        if (!customInfo.isEmpty()) {
            title = title.isEmpty() ? customInfo
                                    : QStringLiteral("%1 | %2").arg(title, customInfo);
        }
        if (m_launchPid > 0 && !title.isEmpty()) {
            setGameWindowTitle(m_launchPid, title);
        }
        if (launchSettings.launch.windowSize == QStringLiteral("borderless")
            && m_launchPid > 0) {
            applyBorderlessWindow(m_launchPid);
        }
        if (m_launchVisibility == QStringLiteral("minimize")) {
            showMinimized();
        } else if (m_launchVisibility == QStringLiteral("hide")
                   && m_launchPid > 0) {
            m_hiddenForGame = true;
            hide();
            if (!m_gameExitTimer) {
                m_gameExitTimer = new QTimer(this);
                m_gameExitTimer->setInterval(1500);
                connect(m_gameExitTimer, &QTimer::timeout,
                        this, &MainWindow::checkGameExit);
            }
            m_gameExitTimer->start();
        }
        if (!m_pendingLaunchToast.isEmpty()) {
            showToast(m_pendingLaunchToast);
            m_pendingLaunchToast.clear();
        }

        return;
    }

    if (m_gameWindowWaitTicks > 15
        && m_launchPid > 0
        && !processStillRunning(m_launchPid)) {
        closeLaunchProgress();
        QString details = readLaunchLog(m_launchStderrLogPath);
        if (details.isEmpty()) {
            const QString stdoutPath = m_launchStderrLogPath
                .left(m_launchStderrLogPath.size()
                      - QStringLiteral("stderr.log").size())
                + QStringLiteral("stdout.log");
            details = readLaunchLog(stdoutPath);
        }
        if (details.isEmpty()) {
            details = QStringLiteral("Java 已退出，但没有写入启动错误输出。");
        }
        const QString crashBehavior =
            GlobalSettings::load().launch.crashBehavior;
        if (crashBehavior == QStringLiteral("silent")) {
            return;
        }
        if (crashBehavior == QStringLiteral("keep_open")) {
            showToast(QStringLiteral("游戏进程已退出，启动器保持打开。"));
            return;
        }
        QMessageBox::warning(
            this, QStringLiteral("游戏启动失败"),
            QStringLiteral("Minecraft Java 进程已退出，未能创建游戏窗口。\n\n%1\n\n"
                           "启动错误日志：%2")
                .arg(details, m_launchStderrLogPath));
        return;
    }

    if (m_gameWindowWaitTicks >= 2250) {
        closeLaunchProgress();
        showToast(QStringLiteral("游戏已启动，但等待窗口出现超时。"));
    }
}

void MainWindow::checkGameExit()
{
    if (m_launchPid > 0 && processStillRunning(m_launchPid)) {
        return;
    }
    if (m_gameExitTimer) {
        m_gameExitTimer->stop();
    }
    if (m_hiddenForGame) {
        m_hiddenForGame = false;
        showNormal();
        raise();
        activateWindow();
    }
}

void MainWindow::positionToast()
{
    if (!m_toast || !m_toast->isVisible()) {
        return;
    }
    const int x = (width() - m_toast->width()) / 2;
    const int y = height() - 82;
    m_toast->move(x, y);
}

void MainWindow::showToast(const QString &message)
{
    if (!m_toast) {
        m_toast = new QLabel(this);
        m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_toast->setAlignment(Qt::AlignCenter);
        m_toast->setFont(Theme::font(11, QFont::DemiBold));
        m_toast->setFixedHeight(38);
        m_toast->setStyleSheet(Theme::isDarkMode()
            ? QStringLiteral(
                "QLabel {"
                "background: rgba(22, 32, 45, 246);"
                "border: 1px solid rgba(59, 142, 243, 110);"
                "border-radius: 19px;"
                "color: #EAF1F7;"
                "padding-left: 16px;"
                "padding-right: 16px;"
                "}")
            : QStringLiteral(
                "QLabel {"
                "background: rgba(255, 255, 255, 246);"
                "border: 1px solid rgba(47, 128, 237, 95);"
                "border-radius: 19px;"
                "color: #17243A;"
                "padding-left: 16px;"
                "padding-right: 16px;"
                "}"));
        m_toastEffect = new QGraphicsOpacityEffect(m_toast);
        m_toastEffect->setOpacity(0.0);
        m_toast->setGraphicsEffect(m_toastEffect);
    }

    m_toastTimer->stop();
    if (m_toastAnimation) {
        m_toastAnimation->stop();
        m_toastAnimation->deleteLater();
        m_toastAnimation = nullptr;
    }

    m_toast->setText(message);
    const int textWidth = m_toast->fontMetrics().horizontalAdvance(message);
    const int toastWidth = qBound(300, textWidth + 42, qMin(720, width() - 80));
    m_toast->setFixedWidth(toastWidth);
    positionToast();
    m_toast->raise();
    m_toast->show();
    m_toastEffect->setOpacity(0.0);

    auto *fadeIn = new QPropertyAnimation(m_toastEffect, "opacity", this);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    connect(fadeIn, &QPropertyAnimation::finished, this, [this, fadeIn]() {
        if (m_toastAnimation == fadeIn) {
            m_toastAnimation = nullptr;
        }
        fadeIn->deleteLater();
    });
    m_toastAnimation = fadeIn;
    fadeIn->start();
    m_toastTimer->start(2500);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool maximized = isMaximized() || isFullScreen();
    const int margin = 0;
    const qreal radius = 0.0;
    const QRectF surface = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

    QPainterPath surfacePath;
    surfacePath.addRoundedRect(surface, radius, radius);

    painter.save();
    painter.setClipPath(surfacePath);

    QLinearGradient background(surface.topLeft(), surface.bottomRight());
    if (Theme::isDarkMode()) {
        if (m_acrylicEnabled) {
            background.setColorAt(0.0, QColor(17, 22, 29, 215));
            background.setColorAt(0.52, QColor(13, 17, 23, 225));
            background.setColorAt(1.0, QColor(8, 11, 16, 238));
        } else {
            background.setColorAt(0.0, QColor(17, 23, 31, 255));
            background.setColorAt(0.48, QColor(13, 17, 23, 255));
            background.setColorAt(1.0, QColor(8, 11, 16, 255));
        }
    } else if (m_acrylicEnabled) {
        background.setColorAt(0.0, QColor(244, 249, 255, 210));
        background.setColorAt(0.52, QColor(235, 244, 255, 220));
        background.setColorAt(1.0, QColor(222, 236, 253, 232));
    } else {
        background.setColorAt(0.0, QColor(250, 253, 255, 255));
        background.setColorAt(0.48, QColor(242, 248, 255, 255));
        background.setColorAt(1.0, QColor(226, 239, 253, 255));
    }
    painter.fillPath(surfacePath, background);

    if (!m_backgroundImage.isNull() && m_backgroundOpacity > 0) {
        const qreal targetAspect = surface.width() / surface.height();
        const qreal imageAspect = static_cast<qreal>(m_backgroundImage.width())
            / m_backgroundImage.height();
        QRectF sourceRect(m_backgroundImage.rect());
        if (imageAspect > targetAspect) {
            const qreal croppedWidth = m_backgroundImage.height() * targetAspect;
            sourceRect.setLeft((m_backgroundImage.width() - croppedWidth) / 2.0);
            sourceRect.setWidth(croppedWidth);
        } else {
            const qreal croppedHeight = m_backgroundImage.width() / targetAspect;
            sourceRect.setTop((m_backgroundImage.height() - croppedHeight) / 2.0);
            sourceRect.setHeight(croppedHeight);
        }

        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setOpacity(m_backgroundOpacity / 100.0);
        painter.drawImage(surface, m_backgroundImage, sourceRect);
        painter.setOpacity(1.0);
    }

    painter.restore();

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Theme::isDarkMode() ? QColor(255, 255, 255, 22)
                                           : QColor(23, 36, 58, 26),
                        1.0));
    painter.drawPath(surfacePath);

    if (!maximized) {
        QLinearGradient edge(surface.left() + radius, surface.top(),
                             surface.right() - radius, surface.top());
        edge.setColorAt(0.0, QColor(255, 255, 255, 0));
        edge.setColorAt(0.25, QColor(255, 255, 255, Theme::isDarkMode() ? 44 : 235));
        edge.setColorAt(0.72, QColor(255, 255, 255, Theme::isDarkMode() ? 22 : 160));
        edge.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(QPen(QBrush(edge), 1.0));
        painter.drawLine(QPointF(surface.left() + radius, surface.top() + 1.0),
                         QPointF(surface.right() - radius, surface.top() + 1.0));
    }
}

void MainWindow::applyBackground(const QString &imagePath, int opacity)
{
    QImage image;
    if (!imagePath.isEmpty()) {
        QImageReader reader(imagePath);
        reader.setAutoTransform(true);
        image = reader.read();
        if (image.isNull()) {
            showToast(QStringLiteral("无法加载背景图片：%1").arg(reader.errorString()));
            return;
        }
    }

    m_backgroundImage = image;
    m_backgroundOpacity = qBound(0, opacity, 100);
    BackgroundStore::instance().setBackground(m_backgroundImage, m_backgroundOpacity);
    if (m_drawer) {
        m_drawer->refreshBackground();
    }
    update();
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
#ifdef Q_OS_WIN
    if (!isMaximized() && !isFullScreen() && result) {
        auto *nativeMessage = static_cast<MSG *>(message);
        if (nativeMessage && nativeMessage->message == WM_NCHITTEST) {
            const int globalX = GET_X_LPARAM(nativeMessage->lParam);
            const int globalY = GET_Y_LPARAM(nativeMessage->lParam);
            const QPoint local = mapFromGlobal(QPoint(globalX, globalY));
            const int border = 8;
            const bool left = local.x() >= 0 && local.x() < border;
            const bool right = local.x() <= width() && local.x() > width() - border;
            const bool top = local.y() >= 0 && local.y() < border;
            const bool bottom = local.y() <= height() && local.y() > height() - border;

            if (top && left) {
                *result = HTTOPLEFT;
                return true;
            }
            if (top && right) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (bottom && left) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (bottom && right) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (left) {
                *result = HTLEFT;
                return true;
            }
            if (right) {
                *result = HTRIGHT;
                return true;
            }
            if (top) {
                *result = HTTOP;
                return true;
            }
            if (bottom) {
                *result = HTBOTTOM;
                return true;
            }
        }
    }
#else
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
