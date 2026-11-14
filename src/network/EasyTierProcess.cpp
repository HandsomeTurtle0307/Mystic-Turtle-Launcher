/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "EasyTierProcess.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QOverload>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr char kPublicPeer[] = "tcp://public.easytier.cn:11010";

QString normalizedVirtualIp(const QString &value)
{
    QString ip = value.trimmed();
    if (ip.isEmpty()) {
        return QStringLiteral("10.126.126.1/24");
    }
    if (!ip.contains(QLatin1Char('/'))) {
        ip += QStringLiteral("/24");
    }
    return ip;
}

} // namespace

EasyTierProcess::EasyTierProcess(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &EasyTierProcess::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &EasyTierProcess::readOutput);
    connect(&m_process, &QProcess::started, this, [this]() {
        setState(QStringLiteral("starting"));
        attachJobObject();
        emit logLine(QStringLiteral("EasyTier 进程已启动，正在连接共享节点..."));
        if (m_peerTimer) {
            m_peerTimer->start();
        }
        QTimer::singleShot(1200, this, &EasyTierProcess::queryPeers);
    });
    connect(&m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        if (m_stopping) {
            return;
        }
        const QString message = QStringLiteral("EasyTier 进程错误：%1")
                                    .arg(m_process.errorString());
        emit logLine(message);
        if (error != QProcess::Crashed) {
            emit errorOccurred(message);
        }
    });
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        if (m_stopping) {
            return;
        }
        const QString message = QStringLiteral("EasyTier 进程已退出，代码 %1（%2）")
                                    .arg(exitCode)
                                    .arg(status == QProcess::CrashExit
                                             ? QStringLiteral("崩溃")
                                             : QStringLiteral("正常退出"));
        emit logLine(message);
        stopPeerMonitor();
        setState(QStringLiteral("error"));
        emit errorOccurred(message);
    });

    m_peerTimer = new QTimer(this);
    m_peerTimer->setInterval(2200);
    connect(m_peerTimer, &QTimer::timeout,
            this, &EasyTierProcess::queryPeers);

    // aboutToQuit also fires when the main window closes. It is a final safety
    // net in addition to the page destructor.
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &EasyTierProcess::stop);
    }
}

EasyTierProcess::~EasyTierProcess()
{
    stop();
#ifdef Q_OS_WIN
    if (m_jobHandle) {
        CloseHandle(static_cast<HANDLE>(m_jobHandle));
        m_jobHandle = nullptr;
    }
#endif
}

void EasyTierProcess::attachJobObject()
{
#ifdef Q_OS_WIN
    if (!m_jobHandle) {
        m_jobHandle = CreateJobObjectW(nullptr, nullptr);
        if (m_jobHandle) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(static_cast<HANDLE>(m_jobHandle),
                                    JobObjectExtendedLimitInformation,
                                    &info, sizeof(info));
        }
    }
    if (!m_jobHandle) {
        return;
    }
    const DWORD pid = static_cast<DWORD>(m_process.processId());
    HANDLE processHandle = OpenProcess(
        PROCESS_SET_QUOTA | PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!processHandle) {
        return;
    }
    if (!AssignProcessToJobObject(static_cast<HANDLE>(m_jobHandle),
                                  processHandle)) {
        emit logLine(QStringLiteral(
            "无法将 EasyTier 加入清理任务组；关闭启动器时仍会尝试正常结束进程。"));
    }
    CloseHandle(processHandle);
#endif
}

void EasyTierProcess::setBinaryPath(const QString &path)
{
    m_binaryPath = path;
}

QString EasyTierProcess::binaryPath() const
{
    return m_binaryPath;
}

bool EasyTierProcess::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void EasyTierProcess::startCreate(const QString &roomName,
                                  const QString &password,
                                  const QString &virtualIp)
{
    m_localVirtualIp = normalizedVirtualIp(virtualIp).section(QLatin1Char('/'), 0, 0);
    QStringList arguments;
    arguments << QStringLiteral("--network-name") << roomName
              << QStringLiteral("--network-secret") << password
              << QStringLiteral("-i") << normalizedVirtualIp(virtualIp)
              << QStringLiteral("-p") << QString::fromLatin1(kPublicPeer);
    startWithArguments(arguments);
}

void EasyTierProcess::startJoin(const QString &networkName,
                                const QString &password)
{
    m_localVirtualIp.clear();
    QStringList arguments;
    arguments << QStringLiteral("--network-name") << networkName
              << QStringLiteral("--network-secret") << password
              << QStringLiteral("-d")
              << QStringLiteral("-p") << QString::fromLatin1(kPublicPeer);
    startWithArguments(arguments);
}

void EasyTierProcess::startWithArguments(const QStringList &arguments)
{
    stop();
    if (m_binaryPath.trimmed().isEmpty()
        || !QFileInfo::exists(m_binaryPath)) {
        const QString message = QStringLiteral("EasyTier 可执行文件不存在：%1")
                                    .arg(m_binaryPath);
        emit logLine(message);
        emit errorOccurred(message);
        setState(QStringLiteral("error"));
        return;
    }

    m_stopping = false;
    m_connectedEmitted = false;
    m_cliAvailable = QFileInfo(QFileInfo(m_binaryPath).absolutePath()
                                   + QStringLiteral("/easytier-cli.exe"))
                         .exists();
    if (!m_cliAvailable) {
        emit logLine(QStringLiteral(
            "未找到 easytier-cli.exe，无法查询真实对端节点；将暂时使用日志状态判断。"));
    }
    m_process.setWorkingDirectory(QFileInfo(m_binaryPath).absolutePath());
    m_process.setProgram(m_binaryPath);
    m_process.setArguments(arguments);
    emit logLine(QStringLiteral("启动 EasyTier：%1 %2")
                     .arg(QFileInfo(m_binaryPath).fileName(),
                          arguments.join(QLatin1Char(' '))));
    m_process.start();
}

void EasyTierProcess::stop()
{
    m_stopping = true;
    stopPeerMonitor();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1500);
    }
    m_stopping = false;
    m_connectedEmitted = false;
    if (m_state != QStringLiteral("disconnected")) {
        setState(QStringLiteral("disconnected"));
    }
}

void EasyTierProcess::readOutput()
{
    const QByteArray data = m_process.readAllStandardOutput()
        + m_process.readAllStandardError();
    if (data.isEmpty()) {
        return;
    }
    const QString text = QString::fromLocal8Bit(data);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        emit logLine(line);
        handleLine(line);
    }
}

void EasyTierProcess::handleLine(const QString &line)
{
    const QString lower = line.toLower();
    if (lower.contains(QStringLiteral("error"))
        || lower.contains(QStringLiteral("failed"))
        || lower.contains(QStringLiteral("panic"))) {
        emit logLine(QStringLiteral("EasyTier 输出包含错误信息，请检查上方日志。"));
    }

    const bool connected =
        lower.contains(QStringLiteral("p2p connection established"))
        || lower.contains(QStringLiteral("connection established"))
        || (lower.contains(QStringLiteral("connected"))
            && !lower.contains(QStringLiteral("disconnected"))
            && !lower.contains(QStringLiteral("connect failed")));
    if (connected && !m_connectedEmitted) {
        m_connectedEmitted = true;
        // The log only tells us the local node reached the network. Wait for
        // easytier-cli to report an actual peer before showing the green state.
        if (m_cliAvailable) {
            setState(QStringLiteral("waiting"));
        } else {
            setState(QStringLiteral("connected"));
        }
    }
}

void EasyTierProcess::setState(const QString &state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}

void EasyTierProcess::queryPeers()
{
    if (m_process.state() == QProcess::NotRunning || m_cliProcess) {
        return;
    }
    const QFileInfo cliInfo(QFileInfo(m_binaryPath).absolutePath()
                                + QStringLiteral("/easytier-cli.exe"));
    if (!cliInfo.exists()) {
        return;
    }
    m_cliProcess = new QProcess(this);
    m_cliProcess->setWorkingDirectory(cliInfo.absolutePath());
    m_cliProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_cliProcess->setProgram(cliInfo.absoluteFilePath());
    m_cliProcess->setArguments({QStringLiteral("peer")});
    connect(m_cliProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        QProcess *process = m_cliProcess;
        m_cliProcess = nullptr;
        if (!process) {
            return;
        }
        const QString output = QString::fromLocal8Bit(process->readAll());
        process->deleteLater();

        QRegularExpression ipRegex(
            QStringLiteral("\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b"));
        QStringList peerIps;
        QRegularExpressionMatchIterator matches =
            ipRegex.globalMatch(output);
        while (matches.hasNext()) {
            const QString ip = matches.next().captured(0);
            if (ip == QStringLiteral("0.0.0.0")
                || ip == QStringLiteral("127.0.0.1")
                || ip == m_localVirtualIp) {
                continue;
            }
            if (!peerIps.contains(ip)) {
                peerIps.append(ip);
            }
        }

        if (!peerIps.isEmpty()) {
            if (m_state != QStringLiteral("connected")) {
                emit logLine(QStringLiteral("检测到对端节点：%1")
                                 .arg(peerIps.join(QStringLiteral(", "))));
                setState(QStringLiteral("connected"));
            }
        } else if (m_connectedEmitted
                   && (m_state == QStringLiteral("starting")
                       || m_state == QStringLiteral("waiting"))) {
            setState(QStringLiteral("waiting"));
        }
    });
    connect(m_cliProcess, &QProcess::errorOccurred, this,
            [this, cliInfo](QProcess::ProcessError) {
        if (!m_cliErrorLogged) {
            m_cliErrorLogged = true;
            emit logLine(QStringLiteral("easytier-cli peer 执行失败：%1")
                             .arg(cliInfo.absoluteFilePath()));
        }
        if (m_cliProcess) {
            QProcess *process = m_cliProcess;
            m_cliProcess = nullptr;
            process->deleteLater();
        }
    });
    m_cliProcess->start();
}

void EasyTierProcess::stopPeerMonitor()
{
    if (m_peerTimer) {
        m_peerTimer->stop();
    }
    if (m_cliProcess) {
        m_cliProcess->kill();
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }
}
