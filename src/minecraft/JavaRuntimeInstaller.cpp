/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "JavaRuntimeInstaller.h"

#include "DownloadTask.h"
#include "GameLauncher.h"
#include "Theme.h"

#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>
#include <QWidget>

namespace {

QString adoptiumArchitecture()
{
    const QString architecture = QSysInfo::currentCpuArchitecture().toLower();
    if (architecture.contains(QStringLiteral("arm64"))
        || architecture.contains(QStringLiteral("aarch64"))) {
        return QStringLiteral("aarch64");
    }
    if (architecture.contains(QStringLiteral("x86_64"))
        || architecture.contains(QStringLiteral("amd64"))) {
        return QStringLiteral("x64");
    }
    return QStringLiteral("x86");
}

bool extractArchive(const QString &archive, const QString &destination,
                    QString *errorMessage)
{
    QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        tar = QStringLiteral("C:/Windows/System32/tar.exe");
    }
    if (!QFileInfo::exists(tar)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到 Windows tar，无法解压 Java。");
        }
        return false;
    }
    if (!QDir().mkpath(destination)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建 Java 运行时目录。");
        }
        return false;
    }

    QProcess process;
    process.start(tar, QStringList{
                             QStringLiteral("-xf"), archive,
                             QStringLiteral("-C"), destination});
    if (!process.waitForStarted(5000)) {
        const QString standardError =
            QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        qWarning().noquote() << "[Java installer] Unable to start archive extractor:"
                             << process.errorString() << standardError;
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法启动 Java 解压工具：%1%2")
                                .arg(process.errorString(),
                                     standardError.isEmpty()
                                         ? QString()
                                         : QStringLiteral("\n%1").arg(standardError));
        }
        return false;
    }
    if (!process.waitForFinished(180000)) {
        process.kill();
        process.waitForFinished(1000);
        const QString standardError =
            QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        qWarning().noquote() << "[Java installer] Archive extraction timed out:"
                             << process.errorString() << standardError;
        if (errorMessage) {
            *errorMessage = QStringLiteral("解压 Java 超时：%1%2")
                                .arg(process.errorString(),
                                     standardError.isEmpty()
                                         ? QString()
                                         : QStringLiteral("\n%1").arg(standardError));
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString standardError =
            QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        qWarning().noquote() << "[Java installer] Archive extraction failed:"
                             << process.errorString() << standardError;
        if (errorMessage) {
            *errorMessage = QStringLiteral("解压 Java 失败：%1")
                                .arg(standardError.isEmpty()
                                         ? process.errorString()
                                         : standardError);
        }
        return false;
    }
    return true;
}

QString findJavaUnder(const QString &root)
{
    QString fallback;
    QDirIterator iterator(root,
                          QStringList{QStringLiteral("javaw.exe"),
                                      QStringLiteral("java.exe")},
                          QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).fileName().compare(QStringLiteral("javaw.exe"),
                                               Qt::CaseInsensitive) == 0) {
            return path;
        }
        if (fallback.isEmpty()) {
            fallback = path;
        }
    }
    return fallback;
}

} // namespace

bool JavaRuntimeInstaller::download(const QString &gameDirectory, int majorVersion,
                                    QWidget *parent, QString *javaPath,
                                    QString *errorMessage)
{
    if (javaPath) {
        javaPath->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    if (majorVersion <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无效的 Java 版本。");
        }
        return false;
    }

    const QDir gameDir(QDir(gameDirectory).absolutePath());
    const QString runtimeDir = gameDir.filePath(QStringLiteral("runtime"));
    if (!QDir().mkpath(runtimeDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建 runtime 目录。");
        }
        return false;
    }

    const int effectiveMajor = qMax(8, majorVersion);
    const QString archivePath = QDir(runtimeDir)
                                    .filePath(QStringLiteral("java-%1.zip").arg(effectiveMajor));
    QFile::remove(archivePath);

    const QString url =
        QStringLiteral("https://api.adoptium.net/v3/binary/latest/%1/ga/windows/%2/jdk/hotspot/normal/eclipse?project=jdk")
            .arg(effectiveMajor)
            .arg(adoptiumArchitecture());

    QProgressDialog progress(
        QStringLiteral("正在下载 Java %1，请稍候…").arg(effectiveMajor),
        QStringLiteral("取消"), 0, 100, parent);
    progress.setWindowTitle(QStringLiteral("下载 Java"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    DownloadTask task(QUrl(url), archivePath, QByteArray());
    QEventLoop loop;
    bool succeeded = false;
    bool cancelled = false;
    QString failure;

    QObject::connect(&task, &DownloadTask::progressChanged, &progress,
                     [&progress](int percent) {
                         if (QProgressBar *bar = progress.findChild<QProgressBar *>()) {
                             Theme::setProgressValue(bar, percent);
                         }
                     });
    QObject::connect(&task, &DownloadTask::succeeded, &loop,
                     [&succeeded, &loop](const QString &) {
                         succeeded = true;
                         loop.quit();
                     });
    QObject::connect(&task, &DownloadTask::failed, &loop,
                     [&failure, &loop](const QString &reason) {
                         failure = reason;
                         loop.quit();
                     });
    QObject::connect(&progress, &QProgressDialog::canceled, &task,
                     [&cancelled, &task]() {
                         cancelled = true;
                         task.cancel();
                     });

    task.start();
    loop.exec();
    progress.close();

    if (!succeeded) {
        QFile::remove(archivePath);
        if (errorMessage) {
            *errorMessage = cancelled
                ? QStringLiteral("已取消 Java 下载。")
                : QStringLiteral("Java 下载失败：%1").arg(failure);
        }
        return false;
    }

    const QString extractDir = QDir(runtimeDir)
                                   .filePath(QStringLiteral("temurin-%1").arg(effectiveMajor));
    QDir(extractDir).removeRecursively();
    if (!extractArchive(archivePath, extractDir, errorMessage)) {
        QFile::remove(archivePath);
        return false;
    }
    QFile::remove(archivePath);

    const QString detected = findJavaUnder(extractDir);
    if (detected.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Java 压缩包中没有找到 java.exe。");
        }
        return false;
    }

    const int detectedMajor = GameLauncher::javaMajorVersion(detected);
    if (detectedMajor <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Java 已解压，但无法识别其版本：%1").arg(detected);
        }
        return false;
    }
    if (effectiveMajor <= 8 ? detectedMajor != effectiveMajor
                            : detectedMajor < effectiveMajor) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("下载的 Java 版本不匹配：需要 %1，实际为 %2。")
                                .arg(effectiveMajor)
                                .arg(detectedMajor);
        }
        return false;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("game/javaPath"), detected);
    settings.sync();
    if (javaPath) {
        *javaPath = detected;
    }
    return true;
}
