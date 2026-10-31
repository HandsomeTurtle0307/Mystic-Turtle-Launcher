/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "mainwindow.h"

#include "AnimatedStackedWidget.h"
#include "DownloadGamePage.h"
#include "DownloadModsPage.h"
#include "HeaderBar.h"
#include "HomePage.h"
#include "LogoMark.h"
#include "ModsDrawer.h"
#include "NavigationBar.h"
#include "SampleData.h"
#include "SettingsPage.h"
#include "AboutPage.h"
#include "Theme.h"
#include "account/AccountDialog.h"
#include "account/AccountManager.h"
#include "minecraft/GameLauncher.h"
#include "minecraft/JavaRuntimeInstaller.h"
#include "platform/WindowEffects.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

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
#else
bool gameWindowVisible(qint64)
{
    return true;
}
#endif

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

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        const QSize target = QSize(1280, 720).boundedTo(available.size() - QSize(80, 80))
                                 .expandedTo(minimumSize());
        resize(target);
        move(available.center() - rect().center());
    }

    setupUi();
    updateWindowChrome();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new HeaderBar(this);
    root->addWidget(m_header);

    m_pages = new AnimatedStackedWidget(this);
    m_homePage = new HomePage(m_pages);
    m_downloadGamePage = new DownloadGamePage(m_pages);
    m_downloadModsPage = new DownloadModsPage(m_pages);
    m_settingsPage = new SettingsPage(m_pages);
    m_aboutPage = new AboutPage(m_pages);
    m_pages->addPage(m_homePage);
    m_pages->addPage(m_downloadGamePage);
    m_pages->addPage(m_downloadModsPage);
    m_pages->addPage(m_settingsPage);
    m_pages->addPage(m_aboutPage);
    m_pages->setCurrentIndex(0, false);
    m_pages->setAnimationsEnabled(m_animationsEnabled);
    m_header->navigationBar()->setAnimationsEnabled(m_animationsEnabled);
    m_downloadGamePage->setGameDirectory(m_homePage->gameDirectory());
    root->addWidget(m_pages, 1);

    connect(m_header, &HeaderBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_header, &HeaderBar::maximizeRestoreRequested, this, &MainWindow::toggleMaximize);
    connect(m_header, &HeaderBar::closeRequested, this, &QWidget::close);
    connect(m_header, &HeaderBar::accountRequested,
            m_homePage, &HomePage::openAccountDialog);

    AccountManager &accountManager = AccountManager::instance();
    m_header->setAccountName(accountManager.account().username);
    connect(&accountManager, &AccountManager::accountChanged, this, [this]() {
        m_header->setAccountName(AccountManager::instance().account().username);
    });

    NavigationBar *navigation = m_header->navigationBar();
    connect(navigation, &NavigationBar::navigationRequested, this, &MainWindow::navigateTo);
    for (int i = 1; i < m_pages->count(); ++i) {
        navigation->setItemEnabled(i, false);
    }
    // Game downloads and settings are available; the unfinished mod browser stays locked.
    navigation->setItemEnabled(1, true);
    navigation->setItemEnabled(3, true);
    navigation->setItemEnabled(4, true);

    connect(m_homePage, &HomePage::detailsRequested,
            this, &MainWindow::openVersionDetails);
    connect(m_homePage, &HomePage::notifyRequested, this, &MainWindow::showToast);
    connect(m_homePage, &HomePage::launchRequested,
            this, &MainWindow::launchSelectedVersion);
    connect(m_homePage, &HomePage::gameDirectoryChanged, this,
            [this](const QString &directory) {
                m_downloadGamePage->setGameDirectory(directory);
                m_settingsPage->setGameDirectory(directory);
            });
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
    connect(m_downloadModsPage, &DownloadModsPage::notifyRequested,
            this, &MainWindow::showToast);
    connect(m_settingsPage, &SettingsPage::notifyRequested, this, &MainWindow::showToast);
    connect(m_settingsPage, &SettingsPage::acrylicEnabledChanged,
            this, &MainWindow::applyAcrylic);
    connect(m_settingsPage, &SettingsPage::animationsEnabledChanged,
            this, [this](bool enabled) {
                m_animationsEnabled = enabled;
                m_pages->setAnimationsEnabled(enabled);
                m_header->navigationBar()->setAnimationsEnabled(enabled);
                QSettings settings;
                settings.setValue(QStringLiteral("ui/animations"), enabled);
                showToast(enabled ? QStringLiteral("已启用丝滑动画。")
                                  : QStringLiteral("已关闭界面过渡动画。"));
            });
    connect(m_settingsPage, &SettingsPage::gameDirectoryChanged, this,
            [this](const QString &directory) {
                m_homePage->setGameDirectory(directory);
                m_downloadGamePage->setGameDirectory(directory);
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
    if (index < 0 || index >= m_pages->count()) {
        return;
    }
    if (!m_header->navigationBar()->isItemEnabled(index)) {
        return;
    }
    m_pages->setCurrentIndex(index, true);
    m_header->navigationBar()->setCurrentIndex(index, true);
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
            labels.append(QStringLiteral("%1 · %2")
                              .arg(accounts.at(i).username, accounts.at(i).typeLabel()));
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

    const QString gameDirectory = m_homePage->gameDirectory();

    updateLaunchProgress(8, QStringLiteral("正在检查 Java 运行时…"));
    const int requiredJava =
        GameLauncher::requiredJavaMajor(gameDirectory, version.versionId);
    QString javaError;
    QString javaPath = GameLauncher::findCompatibleJava(
        gameDirectory, requiredJava, &javaError);

    if (javaPath.isEmpty()) {
        closeLaunchProgress();
        const int downloadMajor = requiredJava > 0 ? requiredJava : 17;
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, QStringLiteral("需要安装 Java"),
            QStringLiteral("没有找到适用于 Minecraft %1 的 Java %2。\n\n"
                           "是否自动下载并安装 Temurin JDK %2？\n"
                           "下载大小约 150–250 MB。")
                .arg(version.minecraft)
                .arg(downloadMajor),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes) {
            showToast(QStringLiteral("已取消启动：需要安装 Java。"));
            return;
        }

        QString installError;
        if (!JavaRuntimeInstaller::download(gameDirectory, downloadMajor, this,
                                            &javaPath, &installError)) {
            QMessageBox::warning(this, QStringLiteral("Java 安装失败"), installError);
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
    const bool launched = GameLauncher::launch(
        gameDirectory, version.versionId, account, javaPath,
        [this](int percent, const QString &status) {
            updateLaunchProgress(30 + qRound(percent * 0.7), status);
        },
        &processId,
        &error);

    if (!launched) {
        closeLaunchProgress();
        showToast(QStringLiteral("启动失败：%1").arg(error));
        return;
    }

    updateLaunchProgress(100, QStringLiteral("等待窗口出现"));
    m_pendingLaunchToast = QStringLiteral("已使用%1「%2」启动 Minecraft %3。")
                               .arg(account.typeLabel(), account.username, version.minecraft);
    startGameWindowWait(processId);
}

void MainWindow::updateLaunchProgress(int percent, const QString &status)
{
    if (!m_launchProgress) {
        m_launchProgress = new QProgressDialog(status, QString(), 0, 100, this);
        m_launchProgress->setWindowTitle(QStringLiteral("启动游戏"));
        m_launchProgress->setWindowModality(Qt::ApplicationModal);
        m_launchProgress->setCancelButton(nullptr);
        m_launchProgress->setMinimumDuration(0);
        m_launchProgress->setAutoClose(false);
        m_launchProgress->setAutoReset(false);
        m_launchProgress->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_launchProgress->setAttribute(Qt::WA_DeleteOnClose);
        m_launchProgress->setMinimumSize(680, 200);
        m_launchProgress->resize(680, 200);

        if (QProgressBar *bar = m_launchProgress->findChild<QProgressBar *>()) {
            bar->setFixedHeight(20);
            bar->setTextVisible(false);
            bar->setStyleSheet(QStringLiteral(
                "QProgressBar { min-height: 20px; max-height: 20px;"
                "border: none; border-radius: 10px; background: #1B2530; }"
                "QProgressBar::chunk { border-radius: 10px; background: #6EE7B7; }"));
        }
        if (QLabel *label = m_launchProgress->findChild<QLabel *>()) {
            label->setFont(Theme::font(13, QFont::DemiBold));
            label->setAlignment(Qt::AlignCenter);
            label->setWordWrap(true);
        }
        m_launchProgress->show();
    }

    m_launchProgress->setLabelText(status);
    m_launchProgress->setValue(qBound(0, percent, 100));
    QApplication::processEvents();
}

void MainWindow::closeLaunchProgress()
{
    if (m_gameWindowTimer) {
        m_gameWindowTimer->stop();
    }
    if (m_launchProgress) {
        QProgressDialog *dialog = m_launchProgress.data();
        m_launchProgress = nullptr;
        dialog->close();
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

    if (gameWindowVisible(m_launchPid)) {
        closeLaunchProgress();
        if (!m_pendingLaunchToast.isEmpty()) {
            showToast(m_pendingLaunchToast);
            m_pendingLaunchToast.clear();
        }
        return;
    }

    ++m_gameWindowWaitTicks;
    if (m_gameWindowWaitTicks >= 300) {
        if (m_gameWindowTimer) {
            m_gameWindowTimer->stop();
        }
        closeLaunchProgress();
        showToast(QStringLiteral("游戏已启动，但等待窗口出现超时。"));
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

    QRadialGradient topGlow(QPointF(surface.right() - 180, surface.top() + 18), 640);
    const QColor topGlowColor = Theme::isDarkMode() ? Theme::Accent : Theme::Blue;
    topGlow.setColorAt(0.0, QColor(topGlowColor.red(), topGlowColor.green(),
                                   topGlowColor.blue(), Theme::isDarkMode() ? 22 : 26));
    topGlow.setColorAt(0.48, QColor(topGlowColor.red(), topGlowColor.green(),
                                    topGlowColor.blue(), Theme::isDarkMode() ? 8 : 9));
    topGlow.setColorAt(1.0, QColor(topGlowColor.red(), topGlowColor.green(),
                                   topGlowColor.blue(), 0));
    painter.fillRect(surface, topGlow);

    QRadialGradient bottomGlow(QPointF(surface.left() - 40, surface.bottom() + 20), 520);
    const QColor bottomGlowColor = Theme::isDarkMode() ? Theme::Blue : Theme::Blue;
    bottomGlow.setColorAt(0.0, QColor(bottomGlowColor.red(), bottomGlowColor.green(),
                                      bottomGlowColor.blue(), Theme::isDarkMode() ? 14 : 18));
    bottomGlow.setColorAt(1.0, QColor(bottomGlowColor.red(), bottomGlowColor.green(),
                                      bottomGlowColor.blue(), 0));
    painter.fillRect(surface, bottomGlow);

    const QColor dotColor = Theme::isDarkMode() ? Theme::Accent : QColor(47, 128, 237);
    painter.setPen(QPen(QColor(dotColor.red(), dotColor.green(), dotColor.blue(),
                               Theme::isDarkMode() ? 10 : 9), 1.0));
    for (qreal y = surface.top() + 22; y < surface.bottom(); y += 30) {
        for (qreal x = surface.left() + 22; x < surface.right(); x += 30) {
            painter.drawPoint(QPointF(x, y));
        }
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
