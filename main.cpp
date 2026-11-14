/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "mainwindow.h"
#include "Theme.h"
#include "network/MojangMirror.h"
#include "settings/GlobalSettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMessageBox>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

QString singleInstanceName()
{
    return QStringLiteral("MysticTurtle-Launcher-single-instance");
}

bool notifyExistingInstance()
{
    QLocalSocket socket;
    socket.connectToServer(singleInstanceName());
    if (!socket.waitForConnected(350)) {
        return false;
    }
    socket.write("activate");
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

#ifdef Q_OS_WIN
bool isRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(
            &ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
            &administratorsGroup)) {
        CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }
    return isAdmin != FALSE;
}

QString quotedArgument(const QString &argument)
{
    QString quoted = argument;
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(quoted);
}

bool relaunchAsAdmin()
{
    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty()) {
        arguments.removeFirst();
    }
    QStringList quoted;
    quoted.reserve(arguments.size());
    for (const QString &argument : arguments) {
        quoted.append(quotedArgument(argument));
    }

    const QString executable = QCoreApplication::applicationFilePath();
    const QString parameters = quoted.join(QLatin1Char(' '));
    const QString workingDirectory = QDir::currentPath();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(executable.utf16());
    info.lpParameters = parameters.isEmpty()
        ? nullptr
        : reinterpret_cast<LPCWSTR>(parameters.utf16());
    info.lpDirectory = reinterpret_cast<LPCWSTR>(workingDirectory.utf16());
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            return false;
        }
        return false;
    }
    if (info.hProcess) {
        CloseHandle(info.hProcess);
    }
    return true;
}
#endif

void activateWindow(QWidget &window)
{
    if (window.isMinimized()) {
        window.showNormal();
    } else {
        window.show();
    }
    window.raise();
    window.activateWindow();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationVersion(QStringLiteral("3.0.0"));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));

    QCoreApplication::setOrganizationName(QStringLiteral("MysticTurtle"));
    QCoreApplication::setApplicationName(QStringLiteral("MysticTurtle"));

    // Only one launcher instance may own the EasyTier process and its port.
    if (notifyExistingInstance()) {
        return 0;
    }

#ifdef Q_OS_WIN
    if (!isRunningAsAdmin()) {
        const auto choice = QMessageBox::question(
            nullptr, QStringLiteral("需要管理员权限"),
            QStringLiteral("开启 P2P 联机需要管理员权限以创建虚拟网卡。\n"
                           "点击“确定”将重启启动器并请求权限。"),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
        if (choice == QMessageBox::Ok) {
            if (relaunchAsAdmin()) {
                return 0;
            }
            QMessageBox::warning(
                nullptr, QStringLiteral("未获得管理员权限"),
                QStringLiteral("启动器将继续以普通权限运行，但 P2P 联机可能无法创建虚拟网卡。"));
        }
    }
#endif

    QLocalServer::removeServer(singleInstanceName());
    QLocalServer localServer;
    if (!localServer.listen(singleInstanceName())) {
        QMessageBox::warning(nullptr, QStringLiteral("启动失败"),
                             QStringLiteral("无法创建单实例监听：%1")
                                 .arg(localServer.errorString()));
        return 0;
    }

    const GlobalSettings globalSettings = GlobalSettings::load();
    MojangMirror::setDownloadSource(
        globalSettings.network.downloadSource == QStringLiteral("official")
            ? MojangMirror::DownloadSource::Official
            : MojangMirror::DownloadSource::Bmclapi);
    globalSettings.applyNetworkProxy();

    Theme::apply(app);

    MainWindow window;
    QObject::connect(&localServer, &QLocalServer::newConnection, &window, [&]() {
        while (QLocalSocket *socket = localServer.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected,
                             socket, &QObject::deleteLater);
            socket->waitForReadyRead(250);
            const QByteArray message = socket->readAll();
            if (message.contains("activate")) {
                activateWindow(window);
            }
            socket->disconnectFromServer();
            socket->deleteLater();
        }
    });

    window.show();

    return QApplication::exec();
}
