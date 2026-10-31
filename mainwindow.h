/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QPointer>
#include <QRect>
#include <QWidget>

class AnimatedStackedWidget;
class AboutPage;
class DownloadGamePage;
class DownloadModsPage;
class HeaderBar;
class HomePage;
class ModsDrawer;
class QGraphicsOpacityEffect;
class QLabel;
class QProgressDialog;
class QPropertyAnimation;
class QTimer;
class SettingsPage;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void setupUi();
    void navigateTo(int index);
    void toggleMaximize();
    void updateWindowChrome();
    void applyAcrylic(bool enabled);
    void openVersionDetails(int versionIndex);
    void launchSelectedVersion();
    void updateLaunchProgress(int percent, const QString &status);
    void closeLaunchProgress();
    void startGameWindowWait(qint64 processId);
    void checkGameWindow();
    void showToast(const QString &message);
    void positionToast();

    HeaderBar *m_header = nullptr;
    AnimatedStackedWidget *m_pages = nullptr;
    HomePage *m_homePage = nullptr;
    DownloadGamePage *m_downloadGamePage = nullptr;
    DownloadModsPage *m_downloadModsPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    AboutPage *m_aboutPage = nullptr;

    QPointer<ModsDrawer> m_drawer;
    QPointer<QProgressDialog> m_launchProgress;
    QTimer *m_gameWindowTimer = nullptr;
    qint64 m_launchPid = 0;
    int m_gameWindowWaitTicks = 0;
    QString m_pendingLaunchToast;
    bool m_acrylicEnabled = false;
    bool m_animationsEnabled = true;
    bool m_nativeEffectsApplied = false;

    QLabel *m_toast = nullptr;
    QGraphicsOpacityEffect *m_toastEffect = nullptr;
    QTimer *m_toastTimer = nullptr;
    QPointer<QPropertyAnimation> m_toastAnimation;
};
