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
#include "VersionSettings.h"

#include <QDialog>
#include <QString>

class AccentButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSlider;
class QSpinBox;

class VersionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    VersionSettingsDialog(const VersionSettings &settings,
                          const GlobalSettings &globalSettings,
                          const QString &versionName,
                          const QString &versionDirectory,
                          QWidget *parent = nullptr);

private:
    QWidget *buildContent();
    QWidget *buildSection(const QString &title, const QString &description,
                          QWidget *parent);
    QWidget *buildSettingRow(const QString &title, const QString &description,
                             QWidget *control, QWidget *parent);
    void updateMemoryControls();
    void updateJvmControls();
    void updateJavaControls();
    void updateWindowControls();
    void updateGameArgsControls();
    void applyJvmPreset(const QString &preset);
    void browseJavaPath();
    void saveSettings();

    VersionSettings m_settings;
    GlobalSettings m_globalSettings;
    QString m_versionName;
    QString m_versionDirectory;

    QCheckBox *m_memoryOverride = nullptr;
    QCheckBox *m_autoMemory = nullptr;
    QSpinBox *m_memorySpin = nullptr;
    QSlider *m_memorySlider = nullptr;
    QLabel *m_memoryHint = nullptr;

    QCheckBox *m_jvmOverride = nullptr;
    QComboBox *m_jvmPreset = nullptr;
    QPlainTextEdit *m_jvmArguments = nullptr;

    QCheckBox *m_javaOverride = nullptr;
    QLineEdit *m_javaPath = nullptr;
    AccentButton *m_javaBrowse = nullptr;
    QLabel *m_javaHint = nullptr;

    QCheckBox *m_windowOverride = nullptr;
    QComboBox *m_windowSize = nullptr;

    QCheckBox *m_gameArgsOverride = nullptr;
    QLineEdit *m_gameArgs = nullptr;
};
