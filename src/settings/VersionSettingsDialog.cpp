/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "VersionSettingsDialog.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QOverload>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QString jvmPresetArguments(const QString &preset)
{
    if (preset == QStringLiteral("g1gc")) {
        return QStringLiteral(
            "-XX:+UseG1GC -XX:+ParallelRefProcEnabled "
            "-XX:MaxGCPauseMillis=200");
    }
    if (preset == QStringLiteral("zgc")) {
        return QStringLiteral("-XX:+UseZGC -XX:+ZGenerational");
    }
    return QString();
}

} // namespace

VersionSettingsDialog::VersionSettingsDialog(const VersionSettings &settings,
                                             const GlobalSettings &globalSettings,
                                             const QString &versionName,
                                             const QString &versionDirectory,
                                             QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_globalSettings(globalSettings)
    , m_versionName(versionName)
    , m_versionDirectory(versionDirectory)
{
    setWindowTitle(QStringLiteral("%1 - 版本独立设置").arg(m_versionName));
    setModal(true);
    setMinimumSize(700, 620);
    resize(780, 760);
    setStyleSheet(QStringLiteral(
        "QDialog { background: #0B1118; }"
        "QFrame#versionGlassCard {"
        "  background: rgba(255,255,255,9);"
        "  border: 1px solid rgba(255,255,255,20);"
        "  border-radius: 16px;"
        "}"
        "QLabel { background: transparent; }"
        "QCheckBox { color: #DCE6EC; spacing: 8px; }"
        "QCheckBox::indicator { width: 17px; height: 17px; }"
        "QComboBox, QLineEdit, QPlainTextEdit, QSpinBox {"
        "  background: rgba(255,255,255,9);"
        "  border: 1px solid rgba(255,255,255,24);"
        "  border-radius: 9px;"
        "  color: #EAF1F7;"
        "  padding: 7px 10px;"
        "}"
        "QComboBox:disabled, QLineEdit:disabled,"
        "QPlainTextEdit:disabled, QSpinBox:disabled {"
        "  color: rgba(220,230,236,90);"
        "  background: rgba(255,255,255,4);"
        "  border-color: rgba(255,255,255,12);"
        "}"
        "QComboBox::drop-down { border: none; width: 25px; }"
        "QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {"
        "  width: 0px; height: 0px; border: none; background: transparent;"
        "}"
        "QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow {"
        "  image: none; width: 0px; height: 0px;"
        "}"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px;"
        "  background: rgba(255,255,255,22); }"
        "QSlider::sub-page:horizontal { border-radius: 2px;"
        "  background: #6EE7B7; }"
        "QSlider::handle:horizontal { width: 15px; height: 15px;"
        "  margin: -6px 0; border-radius: 7px; background: #C8FFEB;"
        "  border: 2px solid #6EE7B7; }"
        "QPushButton:disabled { color: rgba(220,230,236,90); }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 22, 26, 20);
    root->setSpacing(14);

    auto *heading = new QVBoxLayout();
    heading->setSpacing(3);
    heading->addWidget(Theme::label(QStringLiteral("版本独立设置"), 22,
                                    QFont::DemiBold, Theme::TextPrimary, this));
    heading->addWidget(Theme::label(
        QStringLiteral("%1 · 只覆盖当前版本的启动参数，未勾选的项继续使用全局设置。")
            .arg(m_versionName),
        10, QFont::Normal, Theme::TextMuted, this));
    root->addLayout(heading);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(buildContent());
    root->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 2, 0, 0);
    footer->setSpacing(12);
    auto *cancel = new AccentButton(QStringLiteral("取消"),
                                    Icons::Type::Close, this);
    cancel->setVariant(AccentButton::Variant::Secondary);
    cancel->setMinimumHeight(44);
    cancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    auto *save = new AccentButton(QStringLiteral("保存"),
                                  Icons::Type::Check, this);
    save->setVariant(AccentButton::Variant::Primary);
    save->setMinimumHeight(44);
    save->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(save, &QAbstractButton::clicked,
            this, &VersionSettingsDialog::saveSettings);
    footer->addWidget(cancel);
    footer->addWidget(save);
    root->addLayout(footer);

    updateMemoryControls();
    updateJvmControls();
    updateJavaControls();
    updateWindowControls();
    updateGameArgsControls();
}

QWidget *VersionSettingsDialog::buildContent()
{
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(2, 2, 8, 2);
    layout->setSpacing(12);

    // Memory
    auto *memoryCard = buildSection(
        QStringLiteral("内存分配"),
        QStringLiteral("自动分配交给启动器计算，或手动指定此版本的最大内存。"),
        content);
    auto *memoryLayout = static_cast<QVBoxLayout *>(memoryCard->layout());
    m_memoryOverride = new QCheckBox(
        QStringLiteral("为此版本单独覆盖内存分配"), memoryCard);
    m_memoryOverride->setChecked(m_settings.overrideMemory);
    memoryLayout->addWidget(m_memoryOverride);

    m_autoMemory = new QCheckBox(QStringLiteral("自动分配"), memoryCard);
    m_autoMemory->setChecked(m_settings.overrideMemory
                                 ? m_settings.automaticMemory
                                 : m_globalSettings.memory.automatic);
    memoryLayout->addWidget(m_autoMemory);

    auto *memoryControl = new QWidget(memoryCard);
    auto *memoryControlLayout = new QHBoxLayout(memoryControl);
    memoryControlLayout->setContentsMargins(0, 0, 0, 0);
    memoryControlLayout->setSpacing(12);
    m_memorySlider = new QSlider(Qt::Horizontal, memoryControl);
    m_memorySlider->setRange(512, qMax(512, GlobalSettings::physicalMemoryMb()));
    m_memorySlider->setSingleStep(512);
    m_memorySlider->setPageStep(1024);
    m_memorySpin = new QSpinBox(memoryControl);
    m_memorySpin->setRange(512, qMax(512, GlobalSettings::physicalMemoryMb()));
    m_memorySpin->setSingleStep(512);
    m_memorySpin->setSuffix(QStringLiteral(" MB"));
    m_memorySpin->setFixedWidth(125);
    const int initialMemory = m_settings.overrideMemory && m_settings.memoryMb > 0
        ? m_settings.memoryMb
        : m_globalSettings.effectiveMemoryMb();
    m_memorySlider->setValue(initialMemory);
    m_memorySpin->setValue(initialMemory);
    m_memoryHint = Theme::label(QString(), 9, QFont::Normal,
                                Theme::TextMuted, memoryControl);
    memoryControlLayout->addWidget(m_memorySlider, 1);
    memoryControlLayout->addWidget(m_memorySpin);
    memoryControlLayout->addWidget(m_memoryHint);
    memoryLayout->addWidget(memoryControl);
    layout->addWidget(memoryCard);

    connect(m_memoryOverride, &QCheckBox::toggled, this,
            [this]() { updateMemoryControls(); });
    connect(m_autoMemory, &QCheckBox::toggled, this,
            [this]() { updateMemoryControls(); });
    connect(m_memorySlider, &QSlider::valueChanged, m_memorySpin,
            &QSpinBox::setValue);
    connect(m_memorySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_memorySlider, &QSlider::setValue);

    // JVM
    auto *jvmCard = buildSection(
        QStringLiteral("JVM 参数"),
        QStringLiteral("可以选择常用 GC 预设，也可以直接编辑完整参数。"),
        content);
    auto *jvmLayout = static_cast<QVBoxLayout *>(jvmCard->layout());
    m_jvmOverride = new QCheckBox(
        QStringLiteral("为此版本单独设置 JVM 参数"), jvmCard);
    m_jvmOverride->setChecked(m_settings.overrideJvmArguments);
    jvmLayout->addWidget(m_jvmOverride);

    m_jvmPreset = new QComboBox(jvmCard);
    m_jvmPreset->addItem(QStringLiteral("G1GC"), QStringLiteral("g1gc"));
    m_jvmPreset->addItem(QStringLiteral("ZGC"), QStringLiteral("zgc"));
    m_jvmPreset->addItem(QStringLiteral("自定义"), QStringLiteral("custom"));
    const QString initialPreset = m_settings.overrideJvmArguments
        ? m_settings.jvmPreset
        : m_globalSettings.advanced.jvmPreset;
    const int presetIndex = m_jvmPreset->findData(initialPreset);
    m_jvmPreset->setCurrentIndex(presetIndex >= 0 ? presetIndex : 2);
    m_jvmPreset->setMinimumWidth(180);
    jvmLayout->addWidget(buildSettingRow(
        QStringLiteral("参数预设"), QStringLiteral("选择后会自动填入下面的参数框。"),
        m_jvmPreset, jvmCard));

    m_jvmArguments = new QPlainTextEdit(jvmCard);
    m_jvmArguments->setMinimumHeight(110);
    m_jvmArguments->setPlaceholderText(QStringLiteral("-XX:+UseG1GC ..."));
    m_jvmArguments->setPlainText(
        m_settings.overrideJvmArguments ? m_settings.jvmArguments
                                        : m_globalSettings.advanced.jvmArgs);
    jvmLayout->addWidget(m_jvmArguments);
    layout->addWidget(jvmCard);

    connect(m_jvmOverride, &QCheckBox::toggled, this,
            [this]() { updateJvmControls(); });
    connect(m_jvmPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (m_jvmOverride->isChecked()) {
            applyJvmPreset(m_jvmPreset->currentData().toString());
        }
    });

    // Java
    auto *javaCard = buildSection(
        QStringLiteral("Java 路径"),
        QStringLiteral("不指定时使用全局 Java 设置。"),
        content);
    auto *javaLayout = static_cast<QVBoxLayout *>(javaCard->layout());
    m_javaOverride = new QCheckBox(
        QStringLiteral("为此版本单独指定 Java 路径"), javaCard);
    m_javaOverride->setChecked(m_settings.overrideJavaPath);
    javaLayout->addWidget(m_javaOverride);

    auto *javaControl = new QWidget(javaCard);
    auto *javaControlLayout = new QHBoxLayout(javaControl);
    javaControlLayout->setContentsMargins(0, 0, 0, 0);
    javaControlLayout->setSpacing(8);
    m_javaPath = new QLineEdit(javaControl);
    m_javaPath->setPlaceholderText(QStringLiteral("自动使用全局 Java 路径"));
    m_javaPath->setText(m_settings.overrideJavaPath
                            ? m_settings.javaPath
                            : m_globalSettings.advanced.javaPath);
    m_javaBrowse = new AccentButton(QStringLiteral("浏览"),
                                    Icons::Type::Folder, javaControl);
    m_javaBrowse->setVariant(AccentButton::Variant::Secondary);
    m_javaBrowse->setCompact(true);
    connect(m_javaBrowse, &QAbstractButton::clicked,
            this, &VersionSettingsDialog::browseJavaPath);
    javaControlLayout->addWidget(m_javaPath, 1);
    javaControlLayout->addWidget(m_javaBrowse);
    javaLayout->addWidget(javaControl);
    m_javaHint = Theme::label(QString(), 9, QFont::Normal,
                              Theme::TextMuted, javaCard);
    javaLayout->addWidget(m_javaHint);
    layout->addWidget(javaCard);

    connect(m_javaOverride, &QCheckBox::toggled, this,
            [this]() { updateJavaControls(); });
    connect(m_javaPath, &QLineEdit::textChanged, this,
            [this](const QString &) { updateJavaControls(); });

    // Window
    auto *windowCard = buildSection(
        QStringLiteral("游戏窗口"),
        QStringLiteral("仅对当前版本覆盖全局窗口模式。"),
        content);
    auto *windowLayout = static_cast<QVBoxLayout *>(windowCard->layout());
    m_windowOverride = new QCheckBox(
        QStringLiteral("仅在此版本中启用窗口设置"), windowCard);
    m_windowOverride->setChecked(m_settings.overrideWindowSize);
    windowLayout->addWidget(m_windowOverride);
    m_windowSize = new QComboBox(windowCard);
    m_windowSize->addItem(QStringLiteral("默认"), QStringLiteral("default"));
    m_windowSize->addItem(QStringLiteral("全屏"), QStringLiteral("fullscreen"));
    m_windowSize->addItem(QStringLiteral("无边框"), QStringLiteral("borderless"));
    const QString initialWindow = m_settings.overrideWindowSize
        ? m_settings.windowSize
        : m_globalSettings.launch.windowSize;
    int windowIndex = m_windowSize->findData(initialWindow);
    if (windowIndex < 0) {
        windowIndex = 0;
    }
    m_windowSize->setCurrentIndex(windowIndex);
    windowLayout->addWidget(buildSettingRow(
        QStringLiteral("窗口模式"),
        QStringLiteral("默认继承全局；全屏和无边框可作为版本独占配置。"),
        m_windowSize, windowCard));
    layout->addWidget(windowCard);

    connect(m_windowOverride, &QCheckBox::toggled, this,
            [this]() { updateWindowControls(); });

    // Game arguments
    auto *argsCard = buildSection(
        QStringLiteral("游戏参数"),
        QStringLiteral("追加到原版游戏参数之后，例如 --width 1920 --height 1080。"),
        content);
    auto *argsLayout = static_cast<QVBoxLayout *>(argsCard->layout());
    m_gameArgsOverride = new QCheckBox(
        QStringLiteral("为此版本追加自定义游戏参数"), argsCard);
    m_gameArgsOverride->setChecked(m_settings.overrideGameArgs);
    argsLayout->addWidget(m_gameArgsOverride);
    m_gameArgs = new QLineEdit(argsCard);
    m_gameArgs->setPlaceholderText(QStringLiteral("--width 1920 --height 1080"));
    m_gameArgs->setText(m_settings.overrideGameArgs
                            ? m_settings.gameArgs
                            : m_globalSettings.advanced.gameArgs);
    argsLayout->addWidget(m_gameArgs);
    layout->addWidget(argsCard);

    connect(m_gameArgsOverride, &QCheckBox::toggled, this,
            [this]() { updateGameArgsControls(); });

    layout->addStretch(1);
    return content;
}

QWidget *VersionSettingsDialog::buildSection(const QString &title,
                                             const QString &description,
                                             QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("versionGlassCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(9);
    layout->addWidget(Theme::label(title, 15, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(Theme::label(description, 9, QFont::Normal,
                                   Theme::TextMuted, card));
    layout->addSpacing(2);
    return card;
}

QWidget *VersionSettingsDialog::buildSettingRow(const QString &title,
                                                const QString &description,
                                                QWidget *control,
                                                QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *text = new QVBoxLayout();
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);
    text->addWidget(Theme::label(title, 11, QFont::DemiBold,
                                 Theme::TextPrimary, row));
    text->addWidget(Theme::label(description, 9, QFont::Normal,
                                 Theme::TextMuted, row));
    layout->addLayout(text, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

void VersionSettingsDialog::updateMemoryControls()
{
    const bool override = m_memoryOverride->isChecked();
    m_autoMemory->setEnabled(override);
    const bool manual = override && !m_autoMemory->isChecked();
    m_memorySlider->setEnabled(manual);
    m_memorySpin->setEnabled(manual);
    if (!override) {
        m_memoryHint->setText(QStringLiteral("使用全局设置"));
    } else if (m_autoMemory->isChecked()) {
        m_memoryHint->setText(QStringLiteral("自动分配"));
    } else {
        m_memoryHint->setText(QStringLiteral("当前版本独占"));
    }
}

void VersionSettingsDialog::updateJvmControls()
{
    const bool enabled = m_jvmOverride->isChecked();
    m_jvmPreset->setEnabled(enabled);
    m_jvmArguments->setEnabled(enabled);
}

void VersionSettingsDialog::updateJavaControls()
{
    const bool enabled = m_javaOverride->isChecked();
    m_javaPath->setEnabled(enabled);
    m_javaBrowse->setEnabled(enabled);
    if (!enabled) {
        m_javaHint->setText(QStringLiteral("使用全局 Java 路径"));
    } else if (m_javaPath->text().trimmed().isEmpty()) {
        m_javaHint->setText(QStringLiteral("请选择有效的 Java 可执行文件"));
    } else {
        m_javaHint->setText(QStringLiteral("当前版本独占"));
    }
}

void VersionSettingsDialog::updateWindowControls()
{
    m_windowSize->setEnabled(m_windowOverride->isChecked());
}

void VersionSettingsDialog::updateGameArgsControls()
{
    m_gameArgs->setEnabled(m_gameArgsOverride->isChecked());
}

void VersionSettingsDialog::applyJvmPreset(const QString &preset)
{
    if (preset == QStringLiteral("custom")) {
        return;
    }
    const QString arguments = jvmPresetArguments(preset);
    if (!arguments.isEmpty()) {
        m_jvmArguments->setPlainText(arguments);
    }
}

void VersionSettingsDialog::browseJavaPath()
{
    const QString startDirectory = m_javaPath->text().trimmed().isEmpty()
        ? QString()
        : QFileInfo(m_javaPath->text().trimmed()).absolutePath();
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 Java 可执行文件"), startDirectory,
        QStringLiteral("Java 可执行文件 (java.exe javaw.exe java);;所有文件 (*)"));
    if (file.isEmpty()) {
        return;
    }
    m_javaPath->setText(QFileInfo(file).absoluteFilePath());
    updateJavaControls();
}

void VersionSettingsDialog::saveSettings()
{
    if (m_javaOverride->isChecked()
        && m_javaPath->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"),
                             QStringLiteral("已勾选单独指定 Java 路径，但路径为空。"));
        return;
    }

    VersionSettings updated = m_settings;
    updated.overrideMemory = m_memoryOverride->isChecked();
    updated.automaticMemory = updated.overrideMemory
        && m_autoMemory->isChecked();
    updated.memoryMb = m_memorySpin->value();

    updated.overrideJvmArguments = m_jvmOverride->isChecked();
    updated.jvmArguments = m_jvmArguments->toPlainText();
    updated.jvmPreset = m_jvmPreset->currentData().toString();

    updated.overrideJavaPath = m_javaOverride->isChecked();
    updated.javaPath = m_javaPath->text().trimmed();

    updated.overrideWindowSize = m_windowOverride->isChecked();
    updated.windowSize = m_windowSize->currentData().toString();

    updated.overrideGameArgs = m_gameArgsOverride->isChecked();
    updated.gameArgs = m_gameArgs->text().trimmed();

    QString error;
    if (!updated.save(m_versionDirectory, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return;
    }
    accept();
}
