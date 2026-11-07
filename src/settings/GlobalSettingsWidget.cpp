/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "GlobalSettingsWidget.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "MinecraftPaths.h"
#include "Theme.h"
#include "minecraft/GameLauncher.h"
#include "minecraft/LocalVersionScanner.h"
#include "network/MojangMirror.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <algorithm>
#include <utility>

namespace {

QLabel *iconTile(Icons::Type type, const QColor &accent, QWidget *parent)
{
    auto *tile = new QLabel(parent);
    tile->setFixedSize(36, 36);
    tile->setAlignment(Qt::AlignCenter);
    tile->setPixmap(Icons::icon(type, accent, 17).pixmap(17, 17));
    tile->setAttribute(Qt::WA_TransparentForMouseEvents);
    tile->setStyleSheet(QStringLiteral(
                            "background: rgba(%1, %2, %3, 24);"
                            "border: 1px solid rgba(%1, %2, %3, 46);"
                            "border-radius: 10px;")
                            .arg(accent.red())
                            .arg(accent.green())
                            .arg(accent.blue()));
    return tile;
}

QWidget *settingRow(Icons::Type icon, const QString &title,
                    const QString &description, QWidget *control, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(iconTile(icon, Theme::Accent, row), 0, Qt::AlignTop);

    auto *text = new QVBoxLayout();
    text->setContentsMargins(0, 1, 0, 0);
    text->setSpacing(1);
    text->addWidget(Theme::label(title, 12, QFont::DemiBold, Theme::TextPrimary, row));
    if (!description.isEmpty()) {
        auto *descriptionLabel = Theme::label(description, 10, QFont::Normal,
                                              Theme::TextMuted, row);
        descriptionLabel->setWordWrap(true);
        text->addWidget(descriptionLabel);
    }
    layout->addLayout(text, 1);
    if (control) {
        layout->addWidget(control, 0, Qt::AlignVCenter);
    }
    return row;
}

QComboBox *makeCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setFixedHeight(38);
    combo->setMinimumWidth(210);
    combo->setStyleSheet(QStringLiteral(
        "QComboBox { background: rgba(255,255,255,7); border: 1px solid %1;"
        "border-radius: 8px; padding: 5px 10px; color: %2; }"
        "QComboBox:hover { border-color: %3; }"
        "QComboBox QAbstractItemView { background: %4; color: %2;"
        "border: 1px solid %1; selection-background-color: %5; }")
        .arg(Theme::Border.name(), Theme::TextPrimary.name(),
             QColor(255, 255, 255, 60).name(QColor::HexArgb),
             Theme::SurfaceRaised.name(),
             QColor(Theme::Accent.red(), Theme::Accent.green(),
                    Theme::Accent.blue(), 56).name(QColor::HexArgb)));
    return combo;
}

void selectComboData(QComboBox *combo, const QString &value)
{
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

QLabel *sectionHint(const QString &text, QWidget *parent)
{
    auto *label = Theme::label(text, 10, QFont::Normal, Theme::TextMuted, parent);
    label->setWordWrap(true);
    return label;
}

QString formatGb(int memoryMb)
{
    return QStringLiteral("%1 GB").arg(memoryMb / 1024.0, 0, 'f', 1);
}

QString jvmPresetArguments(const QString &preset)
{
    if (preset == QStringLiteral("pcl")) {
        return GlobalSettings::defaultJvmArguments();
    }
    if (preset == QStringLiteral("g1-light")) {
        return QStringLiteral(
            "-XX:+UseG1GC -XX:MaxGCPauseMillis=200 -XX:+ParallelRefProcEnabled "
            "-XX:+DisableExplicitGC -XX:+AlwaysPreTouch");
    }
    if (preset == QStringLiteral("low-memory")) {
        return QStringLiteral(
            "-XX:+UseG1GC -XX:MaxGCPauseMillis=100 -XX:+UseCompressedOops "
            "-XX:+DisableExplicitGC -XX:+ParallelRefProcEnabled");
    }
    if (preset == QStringLiteral("zgc")) {
        return QStringLiteral("-XX:+UseZGC -XX:+UnlockExperimentalVMOptions");
    }
    return QString();
}

void applyProgressStyle(QProgressBar *bar, const QColor &color)
{
    bar->setTextVisible(false);
    bar->setFixedHeight(10);
    bar->setStyleSheet(QStringLiteral(
        "QProgressBar { border: none; border-radius: 5px; background: #1B2530; }"
        "QProgressBar::chunk { border-radius: 5px; background: %1; }")
        .arg(color.name(QColor::HexArgb)));
}

bool moveIsolatedDirectory(const QString &sourcePath,
                           const QString &targetPath,
                           const QString &versionId,
                           QString *errorMessage)
{
    if (!QDir(sourcePath).exists()) {
        return true;
    }

    QDir source(sourcePath);
    if (!QDir().mkpath(targetPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建目标目录：%1").arg(targetPath);
        }
        return false;
    }

    QStringList directories;
    QDirIterator directoryIterator(
        sourcePath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks
                        | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories);
    while (directoryIterator.hasNext()) {
        directoryIterator.next();
        directories.append(directoryIterator.filePath());
    }
    std::sort(directories.begin(), directories.end(),
              [](const QString &left, const QString &right) {
        return left.count(QDir::separator()) > right.count(QDir::separator());
    });

    QDirIterator files(
        sourcePath, QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString filePath = files.next();
        const QString relativePath = source.relativeFilePath(filePath);
        QString destination = QDir(targetPath).filePath(relativePath);
        if (QFileInfo::exists(destination)) {
            const QFileInfo original(destination);
            const QString parent = original.absolutePath();
            const QString suffix = original.suffix();
            const QString base = original.completeBaseName();
            int collision = 0;
            do {
                const QString prefixed = QStringLiteral("%1-%2%3")
                    .arg(versionId, base,
                         collision == 0 ? QString()
                                        : QStringLiteral("-%1").arg(collision));
                destination = QDir(parent).filePath(
                    suffix.isEmpty() ? prefixed
                                     : prefixed + QLatin1Char('.') + suffix);
                ++collision;
            } while (QFileInfo::exists(destination));
        }
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法创建目标目录：%1")
                                    .arg(QFileInfo(destination).absolutePath());
            }
            return false;
        }
        if (!QFile::rename(filePath, destination)
            && (!QFile::copy(filePath, destination) || !QFile::remove(filePath))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("无法迁移文件：%1").arg(filePath);
            }
            return false;
        }
    }

    for (const QString &directory : std::as_const(directories)) {
        QDir().rmdir(directory);
    }
    QDir().rmdir(sourcePath);
    return true;
}

} // namespace

GlobalSettingsWidget::GlobalSettingsWidget(QWidget *parent)
    : QWidget(parent)
    , m_settings(GlobalSettings::load())
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(350);
    connect(m_saveTimer, &QTimer::timeout, this, &GlobalSettingsWidget::saveNow);

    root->addWidget(buildNetworkCard(this));
    root->addWidget(buildLaunchCard(this));
    root->addWidget(buildMemoryCard(this));
    root->addWidget(buildSkinCard(this));
    root->addWidget(buildAdvancedCard(this));

    m_memoryTimer = new QTimer(this);
    m_memoryTimer->setInterval(1000);
    connect(m_memoryTimer, &QTimer::timeout, this, &GlobalSettingsWidget::updateMemoryMonitor);
    m_memoryTimer->start();
    updateMemoryMonitor();
    refreshJavaList();
}

GlobalSettingsWidget::~GlobalSettingsWidget()
{
    if (m_saveTimer && m_saveTimer->isActive()) {
        saveNow();
    }
}

void GlobalSettingsWidget::flush()
{
    if (m_saveTimer && m_saveTimer->isActive()) {
        m_saveTimer->stop();
        saveNow();
    }
}

void GlobalSettingsWidget::setGameDirectory(const QString &directory)
{
    if (!directory.trimmed().isEmpty()) {
        m_gameDirectory = QDir(directory).absolutePath();
    }
}

QWidget *GlobalSettingsWidget::buildNetworkCard(QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(Theme::Blue);
    card->setInteractive(true);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 19, 22, 19);
    layout->setSpacing(0);
    layout->addWidget(Theme::label(QStringLiteral("网络与下载"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(sectionHint(QStringLiteral("下载源、代理与并发策略。"), card));
    layout->addSpacing(12);
    layout->addWidget(Theme::divider(card));
    layout->addSpacing(12);

    m_downloadSource = makeCombo(card);
    m_downloadSource->addItem(QStringLiteral("BMCLAPI 镜像（国内推荐）"),
                              QStringLiteral("bmclapi"));
    m_downloadSource->addItem(QStringLiteral("Mojang 官方源"),
                              QStringLiteral("official"));
    selectComboData(m_downloadSource, m_settings.network.downloadSource);
    connect(m_downloadSource, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.network.downloadSource = m_downloadSource->currentData().toString();
        const bool official =
            m_settings.network.downloadSource == QStringLiteral("official");
        MojangMirror::setDownloadSource(
            official ? MojangMirror::DownloadSource::Official
                     : MojangMirror::DownloadSource::Bmclapi);
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Download, QStringLiteral("下载源"),
        QStringLiteral("影响版本清单、客户端、依赖库和资源文件。"),
        m_downloadSource, card));
    layout->addSpacing(12);

    auto *proxyControl = new QWidget(card);
    auto *proxyLayout = new QHBoxLayout(proxyControl);
    proxyLayout->setContentsMargins(0, 0, 0, 0);
    proxyLayout->setSpacing(7);
    m_proxyType = makeCombo(proxyControl);
    m_proxyType->setFixedWidth(118);
    m_proxyType->addItem(QStringLiteral("不使用代理"), QStringLiteral("none"));
    m_proxyType->addItem(QStringLiteral("HTTP"), QStringLiteral("http"));
    m_proxyType->addItem(QStringLiteral("SOCKS5"), QStringLiteral("socks5"));
    selectComboData(m_proxyType, m_settings.network.proxyType);
    m_proxyHost = new QLineEdit(proxyControl);
    m_proxyHost->setPlaceholderText(QStringLiteral("127.0.0.1"));
    m_proxyHost->setText(m_settings.network.proxyHost);
    m_proxyHost->setFixedHeight(38);
    m_proxyHost->setMinimumWidth(160);
    m_proxyPort = new QSpinBox(proxyControl);
    m_proxyPort->setRange(1, 65535);
    m_proxyPort->setValue(m_settings.network.proxyPort);
    m_proxyPort->setFixedHeight(38);
    m_proxyPort->setFixedWidth(92);
    proxyLayout->addWidget(m_proxyType);
    proxyLayout->addWidget(m_proxyHost, 1);
    proxyLayout->addWidget(m_proxyPort);

    const auto updateProxy = [this]() {
        m_settings.network.proxyType = m_proxyType->currentData().toString();
        m_settings.network.proxyHost = m_proxyHost->text().trimmed();
        m_settings.network.proxyPort = m_proxyPort->value();
        m_settings.applyNetworkProxy();
        scheduleSave();
    };
    connect(m_proxyType, &QComboBox::currentIndexChanged, this,
            [updateProxy](int) { updateProxy(); });
    connect(m_proxyHost, &QLineEdit::textChanged, this,
            [updateProxy](const QString &) { updateProxy(); });
    connect(m_proxyPort, &QSpinBox::valueChanged, this,
            [updateProxy](int) { updateProxy(); });
    layout->addWidget(settingRow(
        Icons::Type::Shield, QStringLiteral("HTTP / SOCKS5 代理"),
        QStringLiteral("留空主机名或选择「不使用代理」即可关闭。"),
        proxyControl, card));
    layout->addSpacing(12);

    m_downloadConcurrency = new QSpinBox(card);
    m_downloadConcurrency->setRange(1, 8);
    m_downloadConcurrency->setValue(m_settings.network.concurrent);
    m_downloadConcurrency->setFixedHeight(38);
    m_downloadConcurrency->setFixedWidth(92);
    m_downloadConcurrency->setSuffix(QStringLiteral(" 个"));
    connect(m_downloadConcurrency, &QSpinBox::valueChanged, this, [this](int value) {
        m_settings.network.concurrent = value;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Grid, QStringLiteral("下载并发数"),
        QStringLiteral("同时下载的文件数量，范围 1–8；过高可能触发服务器限速。"),
        m_downloadConcurrency, card));
    layout->addStretch();

    m_settings.applyNetworkProxy();
    MojangMirror::setDownloadSource(
        m_settings.network.downloadSource == QStringLiteral("official")
            ? MojangMirror::DownloadSource::Official
            : MojangMirror::DownloadSource::Bmclapi);
    return card;
}

QWidget *GlobalSettingsWidget::buildLaunchCard(QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(Theme::Accent);
    card->setInteractive(true);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 19, 22, 19);
    layout->setSpacing(0);
    layout->addWidget(Theme::label(QStringLiteral("启动选项"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(sectionHint(QStringLiteral("窗口标题与启动后的行为。"), card));
    layout->addSpacing(12);
    m_versionIsolation = makeCombo(card);
    m_versionIsolation->addItem(QStringLiteral("所有版本隔离"), QStringLiteral("isolate_all"));
    m_versionIsolation->addItem(QStringLiteral("仅带加载器的版本隔离"),
                                QStringLiteral("isolate_loaders"));
    m_versionIsolation->addItem(QStringLiteral("不隔离"), QStringLiteral("none"));
    selectComboData(m_versionIsolation, m_settings.versionIsolation);
    connect(m_versionIsolation, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString requested = m_versionIsolation->currentData().toString();
        const QString previous = m_settings.versionIsolation;
        if (requested == previous) {
            return;
        }
        const auto isolationRank = [](const QString &mode) {
            if (mode == QStringLiteral("isolate_all")) {
                return 2;
            }
            if (mode == QStringLiteral("isolate_loaders")) {
                return 1;
            }
            return 0;
        };
        if (isolationRank(requested) < isolationRank(previous)) {
            const QString warning = requested == QStringLiteral("none")
                ? QStringLiteral("警告：关闭版本隔离后，游戏将重新读取根目录下的存档和 Mod。"
                                 "由于之前开启隔离时生成的存档位于 versions/版本名 目录下，"
                                 "关闭后您可能看不到这些存档。是否确认关闭？")
                : QStringLiteral("警告：缩小版本隔离范围后，部分版本将改为读取根目录下的"
                                 "存档和 Mod。之前保存在这些版本目录中的数据可能不会显示。"
                                 "是否继续？");
            const auto answer = QMessageBox::warning(
                this, QStringLiteral("确认调整版本隔离"),
                warning,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                const QSignalBlocker blocker(m_versionIsolation);
                selectComboData(m_versionIsolation, previous);
                return;
            }
            const auto migrate = QMessageBox::question(
                this, QStringLiteral("迁移隔离目录数据"),
                QStringLiteral("是否需要将即将取消隔离的版本中的存档和 Mod 移动回根目录？"
                               "遇到同名文件时会保留双方文件并为迁入文件添加版本名前缀。"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (migrate == QMessageBox::Yes) {
                if (m_gameDirectory.trimmed().isEmpty()) {
                    QMessageBox::warning(
                        this, QStringLiteral("无法迁移隔离目录"),
                        QStringLiteral("请先设置有效的 Minecraft 游戏目录。"));
                    const QSignalBlocker blocker(m_versionIsolation);
                    selectComboData(m_versionIsolation, previous);
                    return;
                }
                const GlobalSettings nextSettings = [this, &requested]() {
                    GlobalSettings next = m_settings;
                    next.versionIsolation = requested;
                    return next;
                }();
                const LocalVersionScanner::Result scan =
                    LocalVersionScanner::scan(m_gameDirectory);
                for (const VersionInfo &version : scan.versions) {
                    if (!m_settings.isolatesVersion(version.loader)
                        || nextSettings.isolatesVersion(version.loader)) {
                        continue;
                    }
                    for (const QString &dataDirectory :
                         {QStringLiteral("saves"), QStringLiteral("mods")}) {
                        QString error;
                        if (!moveIsolatedDirectory(
                                QDir(version.gameDataDirectory).filePath(dataDirectory),
                                QDir(m_gameDirectory).filePath(dataDirectory),
                                version.versionId, &error)) {
                            QMessageBox::warning(
                                this, QStringLiteral("迁移未完成"),
                                QStringLiteral("%1\n\n版本隔离设置保持不变；"
                                               "已成功迁移的文件不会自动回滚。")
                                    .arg(error));
                            const QSignalBlocker blocker(m_versionIsolation);
                            selectComboData(m_versionIsolation, previous);
                            return;
                        }
                    }
                }
                emit notifyRequested(QStringLiteral("已将隔离目录中的存档和 Mod 迁移到根目录。"));
            }
        }
        m_settings.versionIsolation = requested;
        if (m_saveTimer && m_saveTimer->isActive()) {
            m_saveTimer->stop();
        }
        if (!m_settings.save()) {
            m_settings.versionIsolation = previous;
            const QSignalBlocker blocker(m_versionIsolation);
            selectComboData(m_versionIsolation, previous);
            emit notifyRequested(
                QStringLiteral("版本隔离设置保存失败，请检查配置目录权限。"));
            return;
        }
        emit versionIsolationChanged();
    });
    layout->addWidget(settingRow(
        Icons::Type::Grid, QStringLiteral("版本隔离"),
        QStringLiteral("隔离版存档、Mod、资源包、光影、配置、日志和游戏选项。"),
        m_versionIsolation, card));
    layout->addSpacing(12);
    layout->addWidget(Theme::divider(card));
    layout->addSpacing(12);

    m_windowTitle = new QLineEdit(card);
    m_windowTitle->setPlaceholderText(QStringLiteral("默认"));
    m_windowTitle->setText(m_settings.launch.windowTitle);
    m_windowTitle->setFixedHeight(38);
    connect(m_windowTitle, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.launch.windowTitle = text;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Sliders, QStringLiteral("游戏窗口标题"),
        QStringLiteral("留空使用 Minecraft 默认标题。"), m_windowTitle, card));
    layout->addSpacing(12);

    m_customInfo = new QLineEdit(card);
    m_customInfo->setPlaceholderText(QStringLiteral("例如：PCL"));
    m_customInfo->setText(m_settings.launch.customInfo);
    m_customInfo->setFixedHeight(38);
    connect(m_customInfo, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.launch.customInfo = text;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Info, QStringLiteral("自定义信息"),
        QStringLiteral("追加到 Minecraft 窗口标题，例如整合包名称。"),
        m_customInfo, card));
    layout->addSpacing(12);

    m_visibility = makeCombo(card);
    m_visibility->addItem(QStringLiteral("游戏启动后仍保持不变"), QStringLiteral("keep"));
    m_visibility->addItem(QStringLiteral("最小化"), QStringLiteral("minimize"));
    m_visibility->addItem(QStringLiteral("隐藏"), QStringLiteral("hide"));
    selectComboData(m_visibility, m_settings.launch.launcherVisibility);
    connect(m_visibility, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.launch.launcherVisibility = m_visibility->currentData().toString();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Grid, QStringLiteral("启动器可见性"),
        QStringLiteral("游戏窗口出现后如何处理启动器窗口。"), m_visibility, card));
    layout->addSpacing(12);

    m_priority = makeCombo(card);
    m_priority->addItem(QStringLiteral("高（优先占用 CPU）"), QStringLiteral("high"));
    m_priority->addItem(QStringLiteral("高于普通"), QStringLiteral("above_normal"));
    m_priority->addItem(QStringLiteral("普通"), QStringLiteral("normal"));
    m_priority->addItem(QStringLiteral("低于普通"), QStringLiteral("below_normal"));
    m_priority->addItem(QStringLiteral("低（后台运行）"), QStringLiteral("low"));
    selectComboData(m_priority, m_settings.launch.processPriority);
    connect(m_priority, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.launch.processPriority = m_priority->currentData().toString();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Cpu, QStringLiteral("进程优先级"),
        QStringLiteral("高优先级可能让游戏更流畅，但会占用更多其他程序的 CPU 时间。"),
        m_priority, card));
    layout->addSpacing(12);

    m_windowSize = makeCombo(card);
    m_windowSize->addItem(QStringLiteral("默认（使用游戏设置）"), QStringLiteral("default"));
    m_windowSize->addItem(QStringLiteral("全屏"), QStringLiteral("fullscreen"));
    m_windowSize->addItem(QStringLiteral("宽屏（1280 × 720）"), QStringLiteral("widescreen"));
    m_windowSize->addItem(QStringLiteral("窗口（1280 × 720）"), QStringLiteral("windowed"));
    m_windowSize->addItem(QStringLiteral("无边框窗口化"), QStringLiteral("borderless"));
    selectComboData(m_windowSize, m_settings.launch.windowSize);
    connect(m_windowSize, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.launch.windowSize = m_windowSize->currentData().toString();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Maximize, QStringLiteral("窗口大小"),
        QStringLiteral("启动时覆盖游戏窗口尺寸；无边框窗口化会在游戏窗口出现后应用。"),
        m_windowSize, card));
    layout->addSpacing(12);

    m_crashBehavior = makeCombo(card);
    m_crashBehavior->addItem(QStringLiteral("弹窗提示"), QStringLiteral("dialog"));
    m_crashBehavior->addItem(QStringLiteral("静默处理"), QStringLiteral("silent"));
    m_crashBehavior->addItem(QStringLiteral("保持启动器打开"), QStringLiteral("keep_open"));
    selectComboData(m_crashBehavior, m_settings.launch.crashBehavior);
    connect(m_crashBehavior, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.launch.crashBehavior = m_crashBehavior->currentData().toString();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Info, QStringLiteral("游戏崩溃后的行为"),
        QStringLiteral("游戏进程在窗口出现前退出时，启动器如何提示。"),
        m_crashBehavior, card));
    layout->addStretch();
    return card;
}

QWidget *GlobalSettingsWidget::buildMemoryCard(QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(Theme::Blue);
    card->setInteractive(true);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 19, 22, 19);
    layout->setSpacing(0);
    layout->addWidget(Theme::label(QStringLiteral("内存分配"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(sectionHint(QStringLiteral("Java 堆内存与系统内存使用情况。"), card));
    layout->addSpacing(12);
    layout->addWidget(Theme::divider(card));
    layout->addSpacing(12);

    auto *modeControl = new QWidget(card);
    auto *modeLayout = new QHBoxLayout(modeControl);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(12);
    m_autoMemory = new QRadioButton(QStringLiteral("自动配置"), modeControl);
    m_customMemory = new QRadioButton(QStringLiteral("自定义"), modeControl);
    m_autoMemory->setChecked(m_settings.memory.automatic);
    m_customMemory->setChecked(!m_settings.memory.automatic);
    auto *group = new QButtonGroup(modeControl);
    group->addButton(m_autoMemory);
    group->addButton(m_customMemory);
    modeLayout->addWidget(m_autoMemory);
    modeLayout->addWidget(m_customMemory);
    modeLayout->addStretch();
    layout->addWidget(settingRow(
        Icons::Type::Memory, QStringLiteral("内存模式"),
        QStringLiteral("自动模式按物理内存推荐；自定义模式手动设置 Java 最大堆。"),
        modeControl, card));
    layout->addSpacing(12);

    auto *sliderControl = new QWidget(card);
    auto *sliderLayout = new QVBoxLayout(sliderControl);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(2);
    m_memoryValue = Theme::label(QString(), 11, QFont::DemiBold,
                                 Theme::Accent, sliderControl);
    m_memoryValue->setAlignment(Qt::AlignRight);
    sliderLayout->addWidget(m_memoryValue);
    const int maxMemoryMb = qMax(1, GlobalSettings::physicalMemoryMb());
    m_memorySlider = new QSlider(Qt::Horizontal, sliderControl);
    m_memorySlider->setRange(0, maxMemoryMb);
    m_memorySlider->setSingleStep(256);
    m_memorySlider->setPageStep(1024);
    m_memorySlider->setValue(
        qBound(0, m_settings.memory.customMb, maxMemoryMb));
    m_memorySlider->setFixedWidth(220);
    m_memorySlider->setEnabled(!m_settings.memory.automatic);
    connect(m_memorySlider, &QSlider::valueChanged, this, [this](int value) {
        m_settings.memory.customMb = value;
        m_settings.memory.automatic = false;
        m_customMemory->setChecked(true);
        m_memoryValue->setText(QStringLiteral("%1 GB").arg(value / 1024.0, 0, 'f', 1));
        syncMemoryControls();
        scheduleSave();
    });
    sliderLayout->addWidget(m_memorySlider);
    layout->addWidget(settingRow(
        Icons::Type::Sliders, QStringLiteral("自定义内存"),
        QStringLiteral("拖动分配 0–%1 GB（当前系统内存上限）；关闭自动配置后可调整。")
            .arg(maxMemoryMb / 1024.0, 0, 'f', 1),
        sliderControl, card));
    layout->addSpacing(12);

    m_optimizeMemory = new QCheckBox(QStringLiteral("启动前执行内存优化"), card);
    m_optimizeMemory->setChecked(m_settings.memory.optimizeBeforeLaunch);
    connect(m_optimizeMemory, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.memory.optimizeBeforeLaunch = checked;
        scheduleSave();
    });
    layout->addWidget(m_optimizeMemory);
    layout->addSpacing(14);

    auto *monitor = new QWidget(card);
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(5);
    m_usedMemoryLabel = Theme::label(QStringLiteral("已使用内存：—"), 10,
                                     QFont::Normal, Theme::TextSecondary, monitor);
    monitorLayout->addWidget(m_usedMemoryLabel);
    m_usedMemoryBar = new QProgressBar(monitor);
    m_usedMemoryBar->setRange(0, 100);
    applyProgressStyle(m_usedMemoryBar, Theme::Blue);
    monitorLayout->addWidget(m_usedMemoryBar);
    m_allocatedMemoryLabel = Theme::label(QStringLiteral("游戏分配：—"), 10,
                                          QFont::Normal, Theme::TextSecondary, monitor);
    monitorLayout->addWidget(m_allocatedMemoryLabel);
    m_allocatedMemoryBar = new QProgressBar(monitor);
    m_allocatedMemoryBar->setRange(0, 100);
    applyProgressStyle(m_allocatedMemoryBar, Theme::Accent);
    monitorLayout->addWidget(m_allocatedMemoryBar);
    layout->addWidget(monitor);
    layout->addStretch();

    connect(m_autoMemory, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_settings.memory.automatic = true;
            syncMemoryControls();
            scheduleSave();
        }
    });
    connect(m_customMemory, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_settings.memory.automatic = false;
            m_memorySlider->setEnabled(true);
            m_settings.memory.customMb = m_memorySlider->value();
            syncMemoryControls();
            scheduleSave();
        }
    });
    syncMemoryControls();
    return card;
}

QWidget *GlobalSettingsWidget::buildSkinCard(QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(QColor(QStringLiteral("#F0A85A")));
    card->setInteractive(true);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 19, 22, 19);
    layout->setSpacing(0);
    layout->addWidget(Theme::label(QStringLiteral("离线皮肤"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(sectionHint(QStringLiteral("管理离线账户使用的角色皮肤。"), card));
    layout->addSpacing(12);

    auto *warning = Theme::label(
        QStringLiteral("该功能尚未接入游戏启动流程，当前设置不会影响实际皮肤。"),
        10, QFont::DemiBold, QColor(QStringLiteral("#F5C27A")), card);
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral(
        "background: rgba(240, 168, 90, 22); border: 1px solid rgba(240, 168, 90, 80);"
        "border-radius: 8px; padding: 8px 10px;"));
    layout->addWidget(warning);
    layout->addSpacing(12);

    auto *skinControl = new QWidget(card);
    skinControl->setEnabled(false);
    auto *skinLayout = new QHBoxLayout(skinControl);
    skinLayout->setContentsMargins(0, 0, 0, 0);
    skinLayout->setSpacing(10);
    m_skinGroup = new QButtonGroup(skinControl);
    const QStringList modes = {
        QStringLiteral("random"), QStringLiteral("steve"), QStringLiteral("alex"),
        QStringLiteral("premium"), QStringLiteral("custom")
    };
    const QStringList labels = {
        QStringLiteral("随机"), QStringLiteral("Steve"), QStringLiteral("Alex"),
        QStringLiteral("正版皮肤"), QStringLiteral("自定义")
    };
    for (int index = 0; index < labels.size(); ++index) {
        auto *radio = new QRadioButton(labels.at(index), skinControl);
        radio->setProperty("skinMode", modes.at(index));
        m_skinGroup->addButton(radio, index);
        skinLayout->addWidget(radio);
        if (modes.at(index) == m_settings.skin.mode) {
            radio->setChecked(true);
        }
    }
    skinLayout->addStretch();
    connect(m_skinGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (!checked || !m_skinGroup->button(id)) {
            return;
        }
        m_settings.skin.mode =
            m_skinGroup->button(id)->property("skinMode").toString();
        updateSkinControls();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Palette, QStringLiteral("皮肤选择"),
        QStringLiteral("离线皮肤仅保存在本地，不会修改正版账号。"),
        skinControl, card));
    layout->addSpacing(12);

    m_premiumName = new QLineEdit(card);
    m_premiumName->setEnabled(false);
    m_premiumName->setPlaceholderText(QStringLiteral("输入正版玩家名"));
    m_premiumName->setText(m_settings.skin.premiumName);
    m_premiumName->setFixedHeight(38);
    connect(m_premiumName, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.skin.premiumName = text;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::External, QStringLiteral("正版玩家名"),
        QStringLiteral("选择「正版皮肤」后启用，用于从正版皮肤 API 拉取角色外观。"),
        m_premiumName, card));
    layout->addSpacing(12);

    auto *customControl = new QWidget(card);
    customControl->setEnabled(false);
    auto *customLayout = new QHBoxLayout(customControl);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(7);
    m_customSkinPath = new QLineEdit(customControl);
    m_customSkinPath->setReadOnly(true);
    m_customSkinPath->setPlaceholderText(QStringLiteral("选择皮肤 PNG 文件"));
    m_customSkinPath->setText(m_settings.skin.customPath);
    m_customSkinPath->setFixedHeight(38);
    auto *browse = new AccentButton(QStringLiteral("浏览"), Icons::Type::Folder, customControl);
    browse->setCompact(true);
    connect(browse, &QAbstractButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择皮肤图片"), QString(),
            QStringLiteral("皮肤图片 (*.png);;所有文件 (*)"));
        if (!file.isEmpty()) {
            m_settings.skin.customPath = QDir::toNativeSeparators(file);
            m_settings.skin.mode = QStringLiteral("custom");
            m_customSkinPath->setText(m_settings.skin.customPath);
            updateSkinControls();
            scheduleSave();
        }
    });
    customLayout->addWidget(m_customSkinPath, 1);
    customLayout->addWidget(browse);
    layout->addWidget(settingRow(
        Icons::Type::Palette, QStringLiteral("自定义皮肤"),
        QStringLiteral("选择 64×64 或 64×32 的皮肤 PNG。"),
        customControl, card));
    layout->addStretch();
    updateSkinControls();
    return card;
}

QWidget *GlobalSettingsWidget::buildAdvancedCard(QWidget *parent)
{
    auto *card = new CardWidget(parent);
    card->setAccentColor(Theme::Purple);
    card->setInteractive(true);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 19, 22, 19);
    layout->setSpacing(0);
    layout->addWidget(Theme::label(QStringLiteral("高级选项"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, card));
    layout->addWidget(sectionHint(QStringLiteral("JVM、游戏参数、Java 与兼容性开关。"), card));
    layout->addSpacing(12);
    layout->addWidget(Theme::divider(card));
    layout->addSpacing(12);

    auto *jvmHeader = new QHBoxLayout();
    jvmHeader->setContentsMargins(0, 0, 0, 0);
    jvmHeader->setSpacing(10);
    jvmHeader->addWidget(Theme::label(QStringLiteral("Java 虚拟机参数"),
                                      11, QFont::DemiBold, Theme::TextSecondary, card));
    jvmHeader->addStretch();
    m_jvmPreset = makeCombo(card);
    m_jvmPreset->setMinimumWidth(150);
    m_jvmPreset->addItem(QStringLiteral("自定义"), QStringLiteral("custom"));
    m_jvmPreset->addItem(QStringLiteral("PCL 常用优化"), QStringLiteral("pcl"));
    m_jvmPreset->addItem(QStringLiteral("G1GC 轻量"), QStringLiteral("g1-light"));
    m_jvmPreset->addItem(QStringLiteral("低内存优化"), QStringLiteral("low-memory"));
    m_jvmPreset->addItem(QStringLiteral("ZGC"), QStringLiteral("zgc"));
    selectComboData(m_jvmPreset, m_settings.advanced.jvmPreset);
    connect(m_jvmPreset, &QComboBox::currentIndexChanged, this, [this](int) {
        const QString preset = m_jvmPreset->currentData().toString();
        m_settings.advanced.jvmPreset = preset;
        const QString presetArgs = jvmPresetArguments(preset);
        if (!presetArgs.isNull()) {
            m_updatingJvmArgs = true;
            m_settings.advanced.jvmArgs = presetArgs;
            m_jvmArgs->setPlainText(presetArgs);
            m_updatingJvmArgs = false;
        }
        QString gcMode;
        if (preset == QStringLiteral("zgc")) {
            gcMode = QStringLiteral("zgc");
        } else if (preset == QStringLiteral("pcl")
                   || preset == QStringLiteral("g1-light")
                   || preset == QStringLiteral("low-memory")) {
            gcMode = QStringLiteral("g1gc");
        }
        if (!gcMode.isEmpty()) {
            m_settings.advanced.memoryManagement = gcMode;
            if (m_memoryManagement) {
                selectComboData(m_memoryManagement, gcMode);
            }
        }
        scheduleSave();
    });
    jvmHeader->addWidget(m_jvmPreset);
    layout->addLayout(jvmHeader);

    m_jvmArgs = new QTextEdit(card);
    m_jvmArgs->setPlainText(m_settings.advanced.jvmArgs);
    m_jvmArgs->setMinimumHeight(88);
    m_jvmArgs->setStyleSheet(QStringLiteral(
        "QTextEdit { background: rgba(255,255,255,6); border: 1px solid %1;"
        "border-radius: 8px; padding: 7px; color: %2; }")
        .arg(Theme::Border.name(), Theme::TextPrimary.name()));
    connect(m_jvmArgs, &QTextEdit::textChanged, this, [this]() {
        if (m_updatingJvmArgs) {
            return;
        }
        m_settings.advanced.jvmArgs = m_jvmArgs->toPlainText();
        if (m_jvmPreset && m_jvmPreset->currentData().toString() != QStringLiteral("custom")) {
            const QSignalBlocker blocker(m_jvmPreset);
            selectComboData(m_jvmPreset, QStringLiteral("custom"));
            m_settings.advanced.jvmPreset = QStringLiteral("custom");
        }
        scheduleSave();
    });
    layout->addWidget(m_jvmArgs);
    layout->addSpacing(12);

    m_gameArgs = new QLineEdit(card);
    m_gameArgs->setPlaceholderText(QStringLiteral("附加游戏参数，例如 --width 1280 --height 720"));
    m_gameArgs->setText(m_settings.advanced.gameArgs);
    m_gameArgs->setFixedHeight(38);
    m_gameArgs->setMinimumWidth(420);
    connect(m_gameArgs, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.advanced.gameArgs = text;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Play, QStringLiteral("游戏参数"),
        QStringLiteral("追加到 Minecraft 启动参数末尾。"), m_gameArgs, card));
    layout->addSpacing(12);

    m_preLaunchCommand = new QLineEdit(card);
    m_preLaunchCommand->setPlaceholderText(QStringLiteral("启动前执行的命令，可留空"));
    m_preLaunchCommand->setText(m_settings.advanced.preLaunchCommand);
    m_preLaunchCommand->setFixedHeight(38);
    m_preLaunchCommand->setMinimumWidth(420);
    connect(m_preLaunchCommand, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_settings.advanced.preLaunchCommand = text;
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Sliders, QStringLiteral("启动前执行命令"),
        QStringLiteral("在启动 Java 前以当前游戏目录为工作目录执行。"),
        m_preLaunchCommand, card));
    layout->addSpacing(12);

    auto *javaControl = new QWidget(card);
    auto *javaLayout = new QHBoxLayout(javaControl);
    javaLayout->setContentsMargins(0, 0, 0, 0);
    javaLayout->setSpacing(7);
    m_javaList = makeCombo(javaControl);
    m_javaList->setMinimumWidth(300);
    connect(m_javaList, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.advanced.javaPath = m_javaList->currentData().toString();
        scheduleSave();
    });
    auto *refresh = new AccentButton(QString(), Icons::Type::Refresh, javaControl);
    refresh->setVariant(AccentButton::Variant::Ghost);
    refresh->setCompact(true);
    refresh->setFixedSize(38, 38);
    refresh->setAccessibleName(QStringLiteral("刷新 Java 列表"));
    connect(refresh, &QAbstractButton::clicked, this, [this]() {
        refreshJavaList();
        emit notifyRequested(QStringLiteral("Java 列表已刷新。"));
    });
    javaLayout->addWidget(m_javaList, 1);
    javaLayout->addWidget(refresh);
    layout->addWidget(settingRow(
        Icons::Type::Cpu, QStringLiteral("Java 列表"),
        QStringLiteral("选择启动游戏使用的 Java；自动选择会匹配版本要求。"),
        javaControl, card));
    layout->addSpacing(10);

    m_autoDownloadJava = new QCheckBox(
        QStringLiteral("允许启动器自动下载 Java 运行时"), card);
    m_autoDownloadJava->setChecked(m_settings.advanced.autoDownloadJava);
    connect(m_autoDownloadJava, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.advanced.autoDownloadJava = checked;
        scheduleSave();
    });
    layout->addWidget(m_autoDownloadJava);
    layout->addSpacing(12);

    m_memoryManagement = makeCombo(card);
    m_memoryManagement->addItem(QStringLiteral("调优 G1GC"), QStringLiteral("g1gc"));
    m_memoryManagement->addItem(QStringLiteral("默认（JVM 决定）"), QStringLiteral("default"));
    m_memoryManagement->addItem(QStringLiteral("ZGC"), QStringLiteral("zgc"));
    m_memoryManagement->addItem(QStringLiteral("Shenandoah"), QStringLiteral("shenandoah"));
    selectComboData(m_memoryManagement, m_settings.advanced.memoryManagement);
    connect(m_memoryManagement, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.advanced.memoryManagement =
            m_memoryManagement->currentData().toString();
        scheduleSave();
    });
    layout->addWidget(settingRow(
        Icons::Type::Memory, QStringLiteral("内存管理"),
        QStringLiteral("垃圾回收器策略，影响大型整合包的内存表现。"),
        m_memoryManagement, card));
    layout->addSpacing(12);

    auto *switches = new QVBoxLayout();
    switches->setContentsMargins(0, 0, 0, 0);
    switches->setSpacing(7);
    m_disableLaunchWrapper = new QCheckBox(
        QStringLiteral("禁用 Java Launch Wrapper"), card);
    m_disableLaunchWrapper->setChecked(m_settings.advanced.disableLaunchWrapper);
    m_disableLwjglUnsafeAgent = new QCheckBox(
        QStringLiteral("禁用 LWJGL Unsafe Agent"), card);
    m_disableLwjglUnsafeAgent->setChecked(m_settings.advanced.disableLwjglUnsafeAgent);
    m_highPerformanceGpu = new QCheckBox(
        QStringLiteral("使用高性能显卡"), card);
    m_highPerformanceGpu->setChecked(m_settings.advanced.preferHighPerformanceGpu);
    switches->addWidget(m_disableLaunchWrapper);
    switches->addWidget(m_disableLwjglUnsafeAgent);
    switches->addWidget(m_highPerformanceGpu);
    connect(m_disableLaunchWrapper, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.advanced.disableLaunchWrapper = checked;
        scheduleSave();
    });
    connect(m_disableLwjglUnsafeAgent, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.advanced.disableLwjglUnsafeAgent = checked;
        scheduleSave();
    });
    connect(m_highPerformanceGpu, &QCheckBox::toggled, this, [this](bool checked) {
        m_settings.advanced.preferHighPerformanceGpu = checked;
        scheduleSave();
    });
    layout->addLayout(switches);
    layout->addSpacing(12);
    layout->addWidget(sectionHint(
        QStringLiteral("版本独立设置可单独覆盖内存分配和 JVM 参数。"), card));
    layout->addStretch();
    return card;
}

void GlobalSettingsWidget::scheduleSave()
{
    if (m_saveTimer) {
        m_saveTimer->start();
    }
}

void GlobalSettingsWidget::saveNow()
{
    if (!m_settings.save()) {
        emit notifyRequested(QStringLiteral("全局设置保存失败，请检查配置目录权限。"));
    }
}

void GlobalSettingsWidget::syncMemoryControls()
{
    const bool automatic = m_settings.memory.automatic;
    if (m_autoMemory) {
        m_autoMemory->setChecked(automatic);
    }
    if (m_customMemory) {
        m_customMemory->setChecked(!automatic);
    }
    if (m_memorySlider) {
        const QSignalBlocker blocker(m_memorySlider);
        const int maxMemoryMb = qMax(1, GlobalSettings::physicalMemoryMb());
        m_memorySlider->setRange(0, maxMemoryMb);
        m_memorySlider->setEnabled(!automatic);
        m_memorySlider->setValue(qBound(0, m_settings.memory.customMb, maxMemoryMb));
    }
    if (m_memoryValue) {
        const int activeMb = automatic ? m_settings.effectiveMemoryMb()
                                       : m_settings.memory.customMb;
        QString text = QStringLiteral("%1 GB").arg(activeMb / 1024.0, 0, 'f', 1);
        if (automatic) {
            text += QStringLiteral("（自动）");
        }
        m_memoryValue->setText(text);
    }
    if (m_allocatedMemoryLabel) {
        m_allocatedMemoryLabel->setText(
            QStringLiteral("游戏分配：%1").arg(formatGb(m_settings.effectiveMemoryMb())));
    }
    if (m_allocatedMemoryBar) {
        const int total = qMax(1, GlobalSettings::physicalMemoryMb());
        m_allocatedMemoryBar->setValue(
            qBound(0, m_settings.effectiveMemoryMb() * 100 / total, 100));
    }
}

void GlobalSettingsWidget::updateMemoryMonitor()
{
    int totalMb = 0;
    int usedMb = 0;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        totalMb = static_cast<int>(status.ullTotalPhys / (1024ULL * 1024ULL));
        usedMb = totalMb - static_cast<int>(status.ullAvailPhys / (1024ULL * 1024ULL));
    }
#endif
    if (m_usedMemoryLabel) {
        if (totalMb > 0) {
            m_usedMemoryLabel->setText(
                QStringLiteral("已使用内存：%1 / %2 GB")
                    .arg(usedMb / 1024.0, 0, 'f', 1)
                    .arg(totalMb / 1024.0, 0, 'f', 1));
            m_usedMemoryBar->setValue(qBound(0, usedMb * 100 / totalMb, 100));
        } else {
            m_usedMemoryLabel->setText(QStringLiteral("已使用内存：不可用"));
            m_usedMemoryBar->setValue(0);
        }
    }
    syncMemoryControls();
}

void GlobalSettingsWidget::updateSkinControls()
{
    if (m_skinGroup) {
        for (QAbstractButton *button : m_skinGroup->buttons()) {
            if (button->property("skinMode").toString() == m_settings.skin.mode) {
                button->setChecked(true);
                break;
            }
        }
    }
    const bool premium = m_settings.skin.mode == QStringLiteral("premium");
    const bool custom = m_settings.skin.mode == QStringLiteral("custom");
    if (m_premiumName) {
        m_premiumName->setEnabled(premium);
    }
    if (m_customSkinPath) {
        m_customSkinPath->setEnabled(custom);
    }
}

void GlobalSettingsWidget::setJavaPath(const QString &path)
{
    if (!m_javaList || path.trimmed().isEmpty()) {
        return;
    }
    int index = m_javaList->findData(path);
    if (index < 0) {
        const int major = GameLauncher::javaMajorVersion(path);
        m_javaList->addItem(QStringLiteral("Java %1 · %2")
                                .arg(major > 0 ? QString::number(major)
                                               : QStringLiteral("?"))
                                .arg(QDir::toNativeSeparators(path)),
                            path);
        index = m_javaList->count() - 1;
    }
    m_javaList->setCurrentIndex(index);
    m_settings.advanced.javaPath = path;
    scheduleSave();
}

void GlobalSettingsWidget::refreshJavaList()
{
    if (!m_javaList) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QSet<QString> seen;
    QVector<QPair<QString, int>> runtimes;
    const auto addCandidate = [&seen, &runtimes](const QString &candidate) {
        if (candidate.trimmed().isEmpty()) {
            return;
        }
        const QString absolute = QFileInfo(candidate).absoluteFilePath();
        const QString normalized = QDir::cleanPath(absolute).toCaseFolded();
        if (seen.contains(normalized) || !QFileInfo::exists(absolute)) {
            return;
        }
        seen.insert(normalized);
        const int major = GameLauncher::javaMajorVersion(absolute);
        if (major > 0) {
            runtimes.append(qMakePair(absolute, major));
        }
    };

    addCandidate(m_settings.advanced.javaPath);
    const QSettings legacy;
    addCandidate(legacy.value(QStringLiteral("game/javaPath")).toString());
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString javaHome = environment.value(QStringLiteral("JAVA_HOME"));
    if (!javaHome.isEmpty()) {
        addCandidate(QDir(javaHome).filePath(QStringLiteral("bin/javaw.exe")));
        addCandidate(QDir(javaHome).filePath(QStringLiteral("bin/java.exe")));
    }
    addCandidate(QStandardPaths::findExecutable(QStringLiteral("javaw")));
    addCandidate(QStandardPaths::findExecutable(QStringLiteral("java")));

    QStringList roots = {
        QDir(MinecraftPaths::savedGameDirectory()).filePath(QStringLiteral("runtime")),
        QDir(MinecraftPaths::defaultGameDirectory()).filePath(QStringLiteral("runtime")),
        QStringLiteral("C:/Program Files/Java"),
        QStringLiteral("C:/Program Files/Eclipse Adoptium"),
        QStringLiteral("C:/Program Files/Microsoft"),
        QStringLiteral("C:/Program Files/Zulu"),
        QStringLiteral("C:/Program Files/BellSoft"),
        QStringLiteral("C:/Program Files/Amazon Corretto"),
        QDir::homePath() + QStringLiteral("/.jdks")
    };
    for (const QString &root : roots) {
        if (!QDir(root).exists()) {
            continue;
        }
        QDirIterator iterator(root,
                              QStringList{QStringLiteral("javaw.exe"),
                                          QStringLiteral("java.exe")},
                              QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            addCandidate(iterator.next());
        }
    }

    std::sort(runtimes.begin(), runtimes.end(),
              [](const QPair<QString, int> &left, const QPair<QString, int> &right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return QString::localeAwareCompare(left.first, right.first) < 0;
    });

    const QString previous = m_javaList->currentData().toString();
    const QSignalBlocker blocker(m_javaList);
    m_javaList->clear();
    m_javaList->addItem(QStringLiteral("自动选择（检测到 %1 个 Java）")
                            .arg(static_cast<int>(runtimes.size())), QString());
    for (const auto &runtime : runtimes) {
        m_javaList->addItem(QStringLiteral("Java %1 · %2")
                                .arg(runtime.second)
                                .arg(QDir::toNativeSeparators(runtime.first)),
                            runtime.first);
    }
    const QString selected = !m_settings.advanced.javaPath.isEmpty()
        ? m_settings.advanced.javaPath
        : previous;
    int index = m_javaList->findData(selected);
    if (index < 0) {
        index = 0;
    }
    m_javaList->setCurrentIndex(index);
    m_settings.advanced.javaPath = m_javaList->itemData(index).toString();
    QApplication::restoreOverrideCursor();
}
