/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "GlobalSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QComboBox;
class QProgressBar;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTextEdit;
class QTimer;
class QButtonGroup;

class GlobalSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GlobalSettingsWidget(QWidget *parent = nullptr);
    ~GlobalSettingsWidget() override;
    void setJavaPath(const QString &path);
    void setGameDirectory(const QString &directory);
    void flush();

signals:
    void notifyRequested(const QString &message);
    void versionIsolationChanged();

private:
    QWidget *buildLaunchCard(QWidget *parent);
    QWidget *buildNetworkCard(QWidget *parent);
    QWidget *buildMemoryCard(QWidget *parent);
    QWidget *buildSkinCard(QWidget *parent);
    QWidget *buildAdvancedCard(QWidget *parent);

    void scheduleSave();
    void saveNow();
    void syncMemoryControls();
    void updateMemoryMonitor();
    void refreshJavaList();
    void updateSkinControls();

    GlobalSettings m_settings;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_memoryTimer = nullptr;

    QLineEdit *m_windowTitle = nullptr;
    QLineEdit *m_customInfo = nullptr;
    QComboBox *m_visibility = nullptr;
    QComboBox *m_priority = nullptr;
    QComboBox *m_windowSize = nullptr;
    QComboBox *m_crashBehavior = nullptr;
    QComboBox *m_versionIsolation = nullptr;

    QComboBox *m_downloadSource = nullptr;
    QComboBox *m_proxyType = nullptr;
    QLineEdit *m_proxyHost = nullptr;
    QSpinBox *m_proxyPort = nullptr;
    QSpinBox *m_downloadConcurrency = nullptr;

    QRadioButton *m_autoMemory = nullptr;
    QRadioButton *m_customMemory = nullptr;
    QSlider *m_memorySlider = nullptr;
    QLabel *m_memoryValue = nullptr;
    QCheckBox *m_optimizeMemory = nullptr;
    QProgressBar *m_usedMemoryBar = nullptr;
    QProgressBar *m_allocatedMemoryBar = nullptr;
    QLabel *m_usedMemoryLabel = nullptr;
    QLabel *m_allocatedMemoryLabel = nullptr;

    QButtonGroup *m_skinGroup = nullptr;
    QLineEdit *m_premiumName = nullptr;
    QLineEdit *m_customSkinPath = nullptr;

    QTextEdit *m_jvmArgs = nullptr;
    QComboBox *m_jvmPreset = nullptr;
    QLineEdit *m_gameArgs = nullptr;
    QLineEdit *m_preLaunchCommand = nullptr;
    QComboBox *m_javaList = nullptr;
    QComboBox *m_memoryManagement = nullptr;
    QCheckBox *m_autoDownloadJava = nullptr;
    QCheckBox *m_disableLaunchWrapper = nullptr;
    QCheckBox *m_disableLwjglUnsafeAgent = nullptr;
    QCheckBox *m_highPerformanceGpu = nullptr;
    bool m_updatingJvmArgs = false;
    QString m_gameDirectory;
};
