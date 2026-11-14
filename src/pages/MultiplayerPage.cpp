/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "MultiplayerPage.h"

#include "EasyTierProcess.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace {

const QColor kCreateAccent(QStringLiteral("#60A5FA"));
const QColor kJoinAccent(QStringLiteral("#A78BFA"));

QString generateRoomCode()
{
    static const QString alphabet =
        QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString code;
    code.reserve(6);
    for (int i = 0; i < 6; ++i) {
        code.append(alphabet.at(
            QRandomGenerator::global()->bounded(alphabet.size())));
    }
    return code;
}

QString bareVirtualIp(const QString &value)
{
    QString ip = value.trimmed();
    const int slash = ip.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        ip = ip.left(slash);
    }
    return ip.isEmpty() ? QStringLiteral("10.126.126.1") : ip;
}

QLabel *makeHintLabel(const QString &text, QWidget *parent)
{
    auto *label = Theme::label(text, 8, QFont::Normal,
                               Theme::TextMuted, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral(
        "background: rgba(255, 255, 255, 3);"
        "border: 1px solid rgba(255, 255, 255, 13);"
        "border-radius: 8px; padding: 6px 8px; color: %1;")
        .arg(Theme::TextMuted.name()));
    return label;
}

} // namespace

MultiplayerPage::MultiplayerPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setStyleSheet(QStringLiteral(
        "QLineEdit#p2pInput {"
        "  background: rgba(255, 255, 255, 5);"
        "  border: none;"
        "  border-bottom: 1px solid rgba(255, 255, 255, 28);"
        "  border-radius: 0;"
        "  padding: 8px 4px;"
        "  color: #F4F8F6;"
        "  selection-background-color: rgba(96, 165, 250, 120);"
        "}"
        "QLineEdit#p2pInput:hover {"
        "  border-bottom-color: rgba(255, 255, 255, 48);"
        "  background: rgba(255, 255, 255, 8);"
        "}"
        "QLineEdit#p2pInput:focus {"
        "  border-bottom-color: #60A5FA;"
        "  background: rgba(96, 165, 250, 10);"
        "}"
        "QPlainTextEdit#p2pLog {"
        "  background: transparent;"
        "  border: none;"
        "  color: #9BE7B8;"
        "  padding: 2px 4px;"
        "}"));

    m_network = new QNetworkAccessManager(this);
    m_easyTier = new EasyTierProcess(this);
    m_connectTimeout = new QTimer(this);
    m_connectTimeout->setSingleShot(true);
    m_connectTimeout->setInterval(30000);
    connect(m_connectTimeout, &QTimer::timeout, this, [this]() {
        setConnectionState(QStringLiteral("连接超时"));
        const QString message = QStringLiteral(
            "30 秒内还没有建立 P2P 连接。可能双方 NAT 较严格，EasyTier 仍在尝试中继；"
            "请检查 Windows 防火墙是否放行 EasyTier 和 Java，或尝试手机热点。");
        setCreateStatus(message);
        setJoinStatus(message);
        appendLog(QStringLiteral("EasyTier 连接超时，可能打洞失败，当前可能正在尝试中继。"));
    });
    connect(m_easyTier, &EasyTierProcess::logLine,
            this, &MultiplayerPage::appendLog);
    connect(m_easyTier, &EasyTierProcess::stateChanged,
            this, &MultiplayerPage::handleEasyTierState);
    connect(m_easyTier, &EasyTierProcess::errorOccurred,
            this, [this](const QString &message) {
        setConnectionState(QStringLiteral("连接失败"));
        setCreateStatus(message);
        setJoinStatus(message);
        setInputsEnabled(true);
        if (m_createButton) {
            m_createButton->setText(QStringLiteral("一键开启联机"));
            m_createButton->setEnabled(true);
        }
        if (m_joinButton) {
            m_joinButton->setEnabled(true);
        }
    });

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"));

    auto *content = new QWidget(scroll);
    content->setAttribute(Qt::WA_NoSystemBackground);

    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(18, 4, 18, 10);
    root->setSpacing(8);
    scroll->setWidget(content);
    pageLayout->addWidget(scroll);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(16);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(2);
    titleBlock->addWidget(Theme::label(QStringLiteral("P2P 联机"), 23,
                                       QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    titleBlock->addWidget(Theme::label(
        QStringLiteral("通过 EasyTier 组建虚拟局域网；创建虚拟网卡需要管理员权限。"),
        9, QFont::Normal, Theme::TextMuted, this));
    header->addLayout(titleBlock, 1);

    m_connectionState = Theme::label(QStringLiteral("未连接"), 9,
                                     QFont::DemiBold, Theme::TextMuted, this);
    m_connectionState->setAlignment(Qt::AlignCenter);
    m_connectionState->setMinimumWidth(82);
    m_connectionState->setStyleSheet(QStringLiteral(
        "background: rgba(255, 255, 255, 5);"
        "border: 1px solid rgba(255, 255, 255, 16);"
        "border-radius: 12px; padding: 5px 10px; color: %1;")
        .arg(Theme::TextMuted.name()));
    header->addWidget(m_connectionState, 0, Qt::AlignVCenter);
    root->addLayout(header);

    auto *steps = Theme::label(
        QStringLiteral("① 双方输入相同房间名和密码加入联机   ② 房主进游戏开放局域网   "
                       "③ 房主复制 IP 和端口发给朋友   ④ 朋友在游戏内“直接连接”粘贴加入"),
        8, QFont::Normal, Theme::TextMuted, content);
    steps->setWordWrap(true);
    steps->setStyleSheet(QStringLiteral(
        "background: rgba(255, 255, 255, 3);"
        "border: 1px solid rgba(255, 255, 255, 12);"
        "border-radius: 9px; padding: 7px 10px; color: %1;")
        .arg(Theme::TextMuted.name()));
    root->addWidget(steps);

    auto *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(14);
    body->addWidget(buildCreateCard(), 1);
    body->addWidget(buildJoinCard(), 1);
    root->addLayout(body, 1);

    root->addWidget(buildConsole());
    appendLog(QStringLiteral("P2P 页面已就绪，等待启动 EasyTier。"));
    appendLog(QStringLiteral("提示：EasyTier 需要管理员权限；首次运行请允许 EasyTier 和 Java 通过防火墙。"));
}

MultiplayerPage::~MultiplayerPage()
{
    if (m_easyTier) {
        m_easyTier->stop();
    }
}

QLineEdit *MultiplayerPage::makeInput(const QString &placeholder,
                                      const QString &text,
                                      QWidget *parent)
{
    auto *input = new QLineEdit(text, parent);
    input->setObjectName(QStringLiteral("p2pInput"));
    input->setPlaceholderText(placeholder);
    input->setFixedHeight(34);
    input->setFont(Theme::font(11));
    input->setClearButtonEnabled(false);
    return input;
}

QWidget *MultiplayerPage::buildCreateCard()
{
    auto *card = new CardWidget(this);
    card->setAccentColor(kCreateAccent);
    card->setCornerRadius(20);
    card->setInteractive(false);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(6);

    auto *heading = new QHBoxLayout();
    heading->setContentsMargins(0, 0, 0, 0);
    heading->addWidget(Theme::label(QStringLiteral("创建房间"), 15,
                                    QFont::DemiBold, Theme::TextPrimary, card));
    heading->addStretch();
    heading->addWidget(Theme::badge(QStringLiteral("CREATE ROOM"),
                                    kCreateAccent,
                                    QColor(kCreateAccent.red(), kCreateAccent.green(),
                                           kCreateAccent.blue(), 20),
                                    card));
    layout->addLayout(heading);
    layout->addSpacing(2);

    layout->addWidget(Theme::label(QStringLiteral("房间名称"), 9,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    m_roomName = makeInput(QStringLiteral("输入房间名称"),
                           QStringLiteral("MysticTurtle 房间"), card);
    layout->addWidget(m_roomName);

    layout->addWidget(Theme::label(QStringLiteral("房间密码"), 9,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    auto *passwordRow = new QHBoxLayout();
    passwordRow->setContentsMargins(0, 0, 0, 0);
    passwordRow->setSpacing(6);
    m_roomPassword = makeInput(QStringLiteral("6 位房间码"),
                               generateRoomCode(), card);
    m_refreshButton = new AccentButton(QString(), Icons::Type::Refresh, card);
    m_refreshButton->setVariant(AccentButton::Variant::Ghost);
    m_refreshButton->setToolTip(QStringLiteral("重新生成房间密码"));
    m_refreshButton->setFixedSize(38, 38);
    connect(m_refreshButton, &QAbstractButton::clicked,
            this, &MultiplayerPage::refreshRoomCode);
    passwordRow->addWidget(m_roomPassword, 1);
    passwordRow->addWidget(m_refreshButton);
    layout->addLayout(passwordRow);

    layout->addWidget(Theme::label(QStringLiteral("虚拟 IP 网段"), 9,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    m_virtualIp = makeInput(QStringLiteral("例如 10.126.126.1"),
                            QStringLiteral("10.126.126.1"), card);
    m_virtualIp->setToolTip(
        QStringLiteral("推荐保持 10.126.126.1；如与本地网络冲突，可改为 "
                       "10.126.127.1 或 10.126.128.1。"));
    layout->addWidget(m_virtualIp);
    layout->addWidget(makeHintLabel(
        QStringLiteral("虚拟 IP 是 EasyTier 房间内的局域网地址，不是公网 IP，"
                       "同一房间内每台设备必须不同。推荐保持默认 10.126.126.1；"
                       "若你所在网络也使用 10.126.126.x，请改成 10.126.127.1。"
                       "IP 冲突可能导致朋友找不到房间、连接超时或只能单向通信。"),
        card));

    m_copyIpButton = new AccentButton(QStringLiteral("复制我的虚拟 IP"),
                                      Icons::Type::Check, card);
    m_copyIpButton->setVariant(AccentButton::Variant::Ghost);
    m_copyIpButton->setCompact(true);
    m_copyIpButton->setToolTip(QStringLiteral("复制当前虚拟 IP"));
    m_copyIpButton->hide();
    connect(m_copyIpButton, &QAbstractButton::clicked, this, [this]() {
        const QString ip = bareVirtualIp(m_virtualIp->text());
        QGuiApplication::clipboard()->setText(ip);
        setCreateStatus(QStringLiteral("已复制虚拟 IP：%1").arg(ip));
        appendLog(QStringLiteral("已复制虚拟 IP：%1").arg(ip));
    });
    layout->addWidget(m_copyIpButton);

    m_createHint = makeHintLabel(
        QStringLiteral("提示：请进入游戏 -> 对局域网开放 -> 将上方 IP 发送给你的朋友，"
                       "并告诉朋友你在游戏里生成的端口号（如 54321）。"),
        card);
    m_createHint->hide();
    layout->addWidget(m_createHint);

    layout->addSpacing(4);
    m_createButton = new AccentButton(QStringLiteral("一键开启联机"),
                                      Icons::Type::Play, card);
    m_createButton->setVariant(AccentButton::Variant::Primary);
    m_createButton->setAccentColor(kCreateAccent);
    m_createButton->setFont(Theme::font(14, QFont::DemiBold));
    m_createButton->setFixedHeight(52);
    m_createButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_createButton, &QAbstractButton::clicked, this, [this]() {
        if (m_easyTier->isRunning()) {
            m_easyTier->stop();
            resetConnectionUi(QStringLiteral("未连接"),
                              QStringLiteral("已停止"),
                              QStringLiteral("等待操作"));
            appendLog(QStringLiteral("已停止 EasyTier 联机。"));
            return;
        }
        if (m_ensureInProgress) {
            appendLog(QStringLiteral("EasyTier 正在准备中，请稍候。"));
            return;
        }
        const QString roomName = m_roomName->text().trimmed();
        const QString password = m_roomPassword->text().trimmed();
        const QString virtualIp = m_virtualIp->text().trimmed();
        if (roomName.isEmpty() || password.isEmpty()) {
            setCreateStatus(QStringLiteral("房间名称和密码不能为空。"));
            return;
        }
        m_createMode = true;
        m_pendingRoomName = roomName;
        m_pendingRoomPassword = password;
        setCreateStatus(QStringLiteral("正在准备 EasyTier..."));
        setConnectionState(QStringLiteral("正在连接..."));
        m_createButton->setEnabled(false);
        m_joinButton->setEnabled(false);
        ensureEasyTier([this, roomName, password, virtualIp](bool ready) {
            if (!ready) {
                resetConnectionUi(QStringLiteral("P2P 不可用"),
                                  QStringLiteral("EasyTier 安装失败，P2P 联机无法使用。请查看日志，或手动将 EasyTier 放入 tools/easytier。"),
                                  QStringLiteral("等待操作"));
                return;
            }
            m_easyTier->setBinaryPath(m_easytierBinaryPath);
            m_easyTier->startCreate(roomName, password, virtualIp);
        });
    });
    layout->addWidget(m_createButton);

    m_createStatus = Theme::label(QStringLiteral("等待操作"), 9,
                                  QFont::Normal, Theme::TextMuted, card);
    m_createStatus->setWordWrap(true);
    layout->addWidget(m_createStatus);

    return card;
}

QWidget *MultiplayerPage::buildJoinCard()
{
    auto *card = new CardWidget(this);
    card->setAccentColor(kJoinAccent);
    card->setCornerRadius(20);
    card->setInteractive(false);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(6);

    auto *heading = new QHBoxLayout();
    heading->setContentsMargins(0, 0, 0, 0);
    heading->addWidget(Theme::label(QStringLiteral("加入房间"), 15,
                                    QFont::DemiBold, Theme::TextPrimary, card));
    heading->addStretch();
    heading->addWidget(Theme::badge(QStringLiteral("JOIN ROOM"),
                                    kJoinAccent,
                                    QColor(kJoinAccent.red(), kJoinAccent.green(),
                                           kJoinAccent.blue(), 20),
                                    card));
    layout->addLayout(heading);
    layout->addSpacing(2);

    layout->addWidget(Theme::label(QStringLiteral("房间码或虚拟 IP"), 9,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    m_joinCode = makeInput(QStringLiteral("房间名#密码，或对方的虚拟 IP"),
                           QString(), card);
    layout->addWidget(m_joinCode);
    layout->addSpacing(2);
    layout->addWidget(Theme::label(
        QStringLiteral("房间码格式：房间名#密码。只填密码时使用默认房间名。"),
        8, QFont::Normal, Theme::TextMuted, card));
    layout->addSpacing(4);

    m_joinButton = new AccentButton(QStringLiteral("加入联机"),
                                    Icons::Type::External, card);
    m_joinButton->setVariant(AccentButton::Variant::Primary);
    m_joinButton->setAccentColor(kJoinAccent);
    m_joinButton->setFont(Theme::font(14, QFont::DemiBold));
    m_joinButton->setFixedHeight(46);
    m_joinButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_joinButton, &QAbstractButton::clicked, this, [this]() {
        if (m_easyTier->isRunning()) {
            setJoinStatus(QStringLiteral("请先停止当前联机。"));
            return;
        }
        const QString target = m_joinCode->text().trimmed();
        if (target.isEmpty()) {
            setJoinStatus(QStringLiteral("请先输入房间码或虚拟 IP。"));
            return;
        }
        if (m_ensureInProgress) {
            setJoinStatus(QStringLiteral("EasyTier 正在准备中，请稍候。"));
            return;
        }
        m_createMode = false;
        m_pendingJoinTarget = target;
        setJoinStatus(QStringLiteral("正在准备 EasyTier..."));
        setConnectionState(QStringLiteral("正在连接..."));
        m_createButton->setEnabled(false);
        m_joinButton->setEnabled(false);
        ensureEasyTier([this, target](bool ready) {
            if (!ready) {
                resetConnectionUi(QStringLiteral("P2P 不可用"),
                                  QStringLiteral("等待操作"),
                                  QStringLiteral("EasyTier 安装失败，P2P 联机无法使用。请查看日志，或手动将 EasyTier 放入 tools/easytier。"));
                return;
            }
            m_easyTier->setBinaryPath(m_easytierBinaryPath);
            startJoinRoom(target);
        });
    });
    layout->addWidget(m_joinButton);

    m_joinStatus = Theme::label(QStringLiteral("等待操作"), 9,
                                QFont::Normal, Theme::TextMuted, card);
    m_joinStatus->setWordWrap(true);
    layout->addWidget(m_joinStatus);
    layout->addSpacing(2);

    layout->addWidget(Theme::label(QStringLiteral("游戏直连信息"), 9,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    auto *directRow = new QHBoxLayout();
    directRow->setContentsMargins(0, 0, 0, 0);
    directRow->setSpacing(6);
    m_hostIp = makeInput(QStringLiteral("房主虚拟 IP"), QString(), card);
    m_gamePort = makeInput(QStringLiteral("端口"), QString(), card);
    m_gamePort->setFixedWidth(82);
    directRow->addWidget(m_hostIp, 1);
    directRow->addWidget(m_gamePort);
    layout->addLayout(directRow);

    m_connectHostButton = new AccentButton(QStringLiteral("复制 IP:端口"),
                                            Icons::Type::External, card);
    m_connectHostButton->setVariant(AccentButton::Variant::Secondary);
    m_connectHostButton->setFont(Theme::font(12, QFont::DemiBold));
    m_connectHostButton->setFixedHeight(36);
    m_connectHostButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_connectHostButton->setToolTip(QStringLiteral("复制房主虚拟 IP 和游戏端口"));
    connect(m_connectHostButton, &QAbstractButton::clicked, this, [this]() {
        const QString ip = m_hostIp->text().trimmed();
        const QString port = m_gamePort->text().trimmed();
        if (ip.isEmpty() || port.isEmpty()) {
            setJoinStatus(QStringLiteral("请先填写房主虚拟 IP 和游戏端口。"));
            return;
        }
        const QString endpoint = QStringLiteral("%1:%2").arg(ip, port);
        QGuiApplication::clipboard()->setText(endpoint);
        setJoinStatus(QStringLiteral("已复制连接地址：%1").arg(endpoint));
        appendLog(QStringLiteral("已复制游戏直连地址：%1").arg(endpoint));
    });
    layout->addWidget(m_connectHostButton);

    layout->addWidget(makeHintLabel(
        QStringLiteral("提示：复制后，请在游戏内的“多人游戏 -> 直接连接”中粘贴并加入。"),
        card));
    return card;
}

QWidget *MultiplayerPage::buildConsole()
{
    auto *card = new CardWidget(this);
    card->setAccentColor(Theme::Accent);
    card->setCornerRadius(16);
    card->setInteractive(false);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 9, 14, 11);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(Theme::label(QStringLiteral("运行日志"), 11,
                                   QFont::DemiBold, Theme::TextSecondary, card));
    header->addSpacing(8);
    header->addWidget(Theme::badge(QStringLiteral("EASYTIER / STDOUT"),
                                   Theme::Accent,
                                   QColor(Theme::Accent.red(), Theme::Accent.green(),
                                          Theme::Accent.blue(), 18),
                                   card));
    header->addStretch();
    auto *copyInfo = new AccentButton(QStringLiteral("复制联机信息"),
                                      Icons::Type::Info, card);
    copyInfo->setVariant(AccentButton::Variant::Ghost);
    copyInfo->setCompact(true);
    connect(copyInfo, &QAbstractButton::clicked, this, [this]() {
        const QString ip = bareVirtualIp(m_virtualIp ? m_virtualIp->text() : QString());
        const QString password = m_roomPassword
            ? m_roomPassword->text().trimmed() : QString();
        const QString message =
            QStringLiteral("快加入我的联机房间！IP：%1，密码：%2，请进游戏后告诉我端口")
                .arg(ip, password);
        QGuiApplication::clipboard()->setText(message);
        appendLog(QStringLiteral("已复制联机信息到剪贴板。"));
    });
    header->addWidget(copyInfo);
    auto *clear = new AccentButton(QStringLiteral("清空"), Icons::Type::Close, card);
    clear->setVariant(AccentButton::Variant::Ghost);
    clear->setCompact(true);
    connect(clear, &QAbstractButton::clicked, this, [this]() {
        if (m_log) {
            m_log->clear();
        }
    });
    header->addWidget(clear);
    layout->addLayout(header);

    m_log = new QPlainTextEdit(card);
    m_log->setObjectName(QStringLiteral("p2pLog"));
    m_log->setReadOnly(true);
    m_log->setUndoRedoEnabled(false);
    m_log->setMaximumBlockCount(500);
    m_log->setFixedHeight(145);
    m_log->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_log->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_log);
    return card;
}

void MultiplayerPage::refreshRoomCode()
{
    if (m_roomPassword) {
        m_roomPassword->setText(generateRoomCode());
    }
}

void MultiplayerPage::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
}

void MultiplayerPage::setConnectionState(const QString &state)
{
    if (!m_connectionState) {
        return;
    }
    const QString text = state.trimmed().isEmpty()
        ? QStringLiteral("未连接") : state.trimmed();
    m_connectionState->setText(text);

    QColor color = Theme::TextMuted;
    QColor background(255, 255, 255, 6);
    QColor border(255, 255, 255, 18);
    if (text.contains(QStringLiteral("已连接"))) {
        color = Theme::Accent;
        background = QColor(Theme::Accent.red(), Theme::Accent.green(),
                            Theme::Accent.blue(), 22);
        border = QColor(Theme::Accent.red(), Theme::Accent.green(),
                        Theme::Accent.blue(), 80);
    } else if (text.contains(QStringLiteral("正在"))) {
        color = QColor(QStringLiteral("#FACC15"));
        background = QColor(250, 204, 21, 18);
        border = QColor(250, 204, 21, 68);
    } else if (text.contains(QStringLiteral("等待"))) {
        color = QColor(QStringLiteral("#FACC15"));
        background = QColor(250, 204, 21, 16);
        border = QColor(250, 204, 21, 64);
    } else if (text.contains(QStringLiteral("超时"))) {
        color = QColor(QStringLiteral("#FACC15"));
        background = QColor(250, 204, 21, 16);
        border = QColor(250, 204, 21, 64);
    } else if (text.contains(QStringLiteral("失败"))
               || text.contains(QStringLiteral("不可用"))) {
        color = Theme::Danger;
        background = QColor(Theme::Danger.red(), Theme::Danger.green(),
                            Theme::Danger.blue(), 18);
        border = QColor(Theme::Danger.red(), Theme::Danger.green(),
                        Theme::Danger.blue(), 70);
    }
    m_connectionState->setStyleSheet(QStringLiteral(
        "background: rgba(%1, %2, %3, %4);"
        "border: 1px solid rgba(%5, %6, %7, %8);"
        "border-radius: 12px; padding: 5px 10px; color: %9;")
        .arg(background.red()).arg(background.green()).arg(background.blue())
        .arg(background.alpha())
        .arg(border.red()).arg(border.green()).arg(border.blue())
        .arg(border.alpha()).arg(color.name()));
}

void MultiplayerPage::setCreateStatus(const QString &status)
{
    if (m_createStatus) {
        m_createStatus->setText(status);
    }
}

void MultiplayerPage::setJoinStatus(const QString &status)
{
    if (m_joinStatus) {
        m_joinStatus->setText(status);
    }
}

void MultiplayerPage::setRoomCode(const QString &code)
{
    if (m_roomPassword) {
        m_roomPassword->setText(code);
    }
}

void MultiplayerPage::appendLog(const QString &line)
{
    if (!m_log) {
        return;
    }
    const QString timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_log->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, line));
}

void MultiplayerPage::setInputsEnabled(bool enabled)
{
    if (m_roomName) m_roomName->setEnabled(enabled);
    if (m_roomPassword) m_roomPassword->setEnabled(enabled);
    if (m_virtualIp) m_virtualIp->setEnabled(enabled);
    if (m_joinCode) m_joinCode->setEnabled(enabled);
    if (m_refreshButton) m_refreshButton->setEnabled(enabled);
}

void MultiplayerPage::resetConnectionUi(const QString &topState,
                                        const QString &createStatus,
                                        const QString &joinStatus)
{
    setConnectionState(topState);
    setCreateStatus(createStatus);
    setJoinStatus(joinStatus);
    if (m_connectTimeout) m_connectTimeout->stop();
    setInputsEnabled(true);
    if (m_copyIpButton) m_copyIpButton->hide();
    if (m_createHint) m_createHint->hide();
    if (m_createButton) {
        m_createButton->setText(QStringLiteral("一键开启联机"));
        m_createButton->setEnabled(true);
    }
    if (m_joinButton) {
        m_joinButton->setEnabled(true);
    }
}

void MultiplayerPage::handleEasyTierState(const QString &state)
{
    if (state == QStringLiteral("starting")) {
        setConnectionState(QStringLiteral("正在连接..."));
        if (m_connectTimeout) m_connectTimeout->start();
        if (m_createMode) {
            setCreateStatus(QStringLiteral("正在分配虚拟 IP，等待 P2P 打洞..."));
        } else {
            setJoinStatus(QStringLiteral("正在连接..."));
        }
        setInputsEnabled(false);
        if (m_copyIpButton) m_copyIpButton->hide();
        if (m_createHint) m_createHint->hide();
        if (m_createButton) {
            m_createButton->setText(QStringLiteral("停止联机"));
            m_createButton->setEnabled(true);
        }
        if (m_joinButton) {
            m_joinButton->setEnabled(false);
        }
        return;
    }

    if (state == QStringLiteral("waiting")) {
        if (m_connectTimeout) m_connectTimeout->stop();
        setConnectionState(QStringLiteral("等待朋友加入"));
        setInputsEnabled(false);
        if (m_copyIpButton) m_copyIpButton->show();
        if (m_createHint) m_createHint->show();
        if (m_createButton) {
            m_createButton->setText(QStringLiteral("停止联机"));
            m_createButton->setEnabled(true);
        }
        if (m_joinButton) {
            m_joinButton->setEnabled(false);
        }
        if (m_createMode) {
            setCreateStatus(QStringLiteral("网络已就绪，等待朋友加入。可先复制虚拟 IP 发给朋友。"));
        } else {
            setJoinStatus(QStringLiteral("已加入网络，等待对端节点连接..."));
        }
        return;
    }

    if (state == QStringLiteral("connected")) {
        setConnectionState(QStringLiteral("已连接对端节点"));
        if (m_connectTimeout) m_connectTimeout->stop();
        setInputsEnabled(false);
        if (m_createButton) {
            m_createButton->setText(QStringLiteral("停止联机"));
            m_createButton->setEnabled(true);
        }
        if (m_joinButton) {
            m_joinButton->setEnabled(false);
        }
        if (m_createMode) {
            if (m_copyIpButton) m_copyIpButton->show();
            if (m_createHint) m_createHint->show();
            setCreateStatus(QStringLiteral("房间创建成功，房间码：%1#%2")
                                .arg(m_pendingRoomName, m_pendingRoomPassword));
        } else {
            setJoinStatus(QStringLiteral("已连接到 %1 的房间").arg(m_pendingJoinTarget));
        }
        return;
    }

    if (state == QStringLiteral("error")) {
        if (m_connectTimeout) m_connectTimeout->stop();
        resetConnectionUi(QStringLiteral("连接失败"),
                          QStringLiteral("EasyTier 启动失败，请查看运行日志。"),
                          QStringLiteral("EasyTier 启动失败，请查看运行日志。"));
        return;
    }

    if (state == QStringLiteral("disconnected")) {
        if (m_connectTimeout) m_connectTimeout->stop();
        resetConnectionUi(QStringLiteral("未连接"),
                          QStringLiteral("已停止"),
                          QStringLiteral("等待操作"));
    }
}

void MultiplayerPage::startCreateRoom(const QString &roomName,
                                      const QString &password,
                                      const QString &virtualIp)
{
    m_easyTier->setBinaryPath(m_easytierBinaryPath);
    m_easyTier->startCreate(roomName, password, virtualIp);
}

void MultiplayerPage::startJoinRoom(const QString &target)
{
    QString networkName = QStringLiteral("MysticTurtle 房间");
    QString password = target;
    const int separator = target.indexOf(QLatin1Char('#'));
    if (separator > 0) {
        networkName = target.left(separator).trimmed();
        password = target.mid(separator + 1).trimmed();
    }
    appendLog(QStringLiteral("加入网络：%1").arg(networkName));
    m_easyTier->setBinaryPath(m_easytierBinaryPath);
    m_easyTier->startJoin(networkName, password);
}

void MultiplayerPage::ensureEasyTier(std::function<void(bool)> callback)
{
    const QString directory = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("tools/easytier"));
    const QString binary = QDir(directory).filePath(QStringLiteral("easytier-core.exe"));

    if (QFileInfo::exists(binary)) {
        m_easytierBinaryPath = binary;
        m_easyTier->setBinaryPath(binary);
        callback(true);
        return;
    }

    if (m_ensureInProgress) {
        appendLog(QStringLiteral("EasyTier 正在准备中，请稍候。"));
        return;
    }
    if (!QDir().mkpath(directory)) {
        appendLog(QStringLiteral("无法创建 EasyTier 目录：%1").arg(directory));
        callback(false);
        return;
    }

    m_ensureInProgress = true;
    m_ensureCallback = std::move(callback);
    appendLog(QStringLiteral("未检测到 EasyTier，正在获取最新版本信息..."));

    QNetworkRequest request(QUrl(
        QStringLiteral("https://api.github.com/repos/EasyTier/EasyTier/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("MysticTurtle/1.0"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *releaseReply = m_network->get(request);
    connect(releaseReply, &QNetworkReply::finished, this,
            [this, releaseReply, directory]() {
        releaseReply->deleteLater();
        if (releaseReply->error() != QNetworkReply::NoError) {
            appendLog(QStringLiteral("获取 EasyTier 版本失败：%1")
                          .arg(releaseReply->errorString()));
            finishEnsure(false);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(releaseReply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            appendLog(QStringLiteral("EasyTier Release JSON 无效：%1")
                          .arg(parseError.errorString()));
            finishEnsure(false);
            return;
        }

        const QJsonArray assets =
            document.object().value(QStringLiteral("assets")).toArray();
        QString downloadUrl;
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QString lower = name.toLower();
            if (lower.contains(QStringLiteral("windows-x86_64"))
                && lower.endsWith(QStringLiteral(".zip"))) {
                downloadUrl = asset.value(QStringLiteral("browser_download_url"))
                                  .toString();
                break;
            }
        }
        if (downloadUrl.isEmpty()) {
            appendLog(QStringLiteral("没有找到 EasyTier Windows x64 压缩包。"));
            finishEnsure(false);
            return;
        }

        const QString zipPath = QDir(directory).filePath(
            QStringLiteral("easytier-download.zip"));
        m_downloadFile = new QFile(zipPath, this);
        if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            appendLog(QStringLiteral("无法写入 EasyTier 下载文件：%1")
                          .arg(m_downloadFile->errorString()));
            m_downloadFile->deleteLater();
            m_downloadFile = nullptr;
            finishEnsure(false);
            return;
        }

        appendLog(QStringLiteral("开始下载 EasyTier..."));
        QNetworkRequest downloadRequest{QUrl(downloadUrl)};
        downloadRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                  QStringLiteral("MysticTurtle/1.0"));
        downloadRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                    QNetworkRequest::NoLessSafeRedirectPolicy);
        m_downloadReply = m_network->get(downloadRequest);

        auto lastPercent = std::make_shared<int>(-1);
        connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
                [this, lastPercent](qint64 received, qint64 total) {
            const int percent = total > 0
                ? int(received * 100 / total) : -1;
            if (percent >= 0 && (percent >= *lastPercent + 10 || percent == 100)) {
                *lastPercent = percent;
                appendLog(QStringLiteral("EasyTier 下载进度：%1%").arg(percent));
            }
        });
        connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
            if (m_downloadFile && m_downloadReply) {
                m_downloadFile->write(m_downloadReply->readAll());
            }
        });
        connect(m_downloadReply, &QNetworkReply::finished, this,
                [this, directory, zipPath]() {
            QNetworkReply *reply = m_downloadReply;
            m_downloadReply = nullptr;
            if (m_downloadFile) {
                if (reply) {
                    m_downloadFile->write(reply->readAll());
                }
                m_downloadFile->flush();
                m_downloadFile->close();
                m_downloadFile->deleteLater();
                m_downloadFile = nullptr;
            }
            if (reply) {
                reply->deleteLater();
            }
            if (!reply || reply->error() != QNetworkReply::NoError) {
                appendLog(QStringLiteral("EasyTier 下载失败：%1")
                              .arg(reply ? reply->errorString()
                                         : QStringLiteral("未知错误")));
                finishEnsure(false);
                return;
            }

            bool extracted = false;
            const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
            if (!tar.isEmpty()) {
                QProcess process;
                process.setProgram(tar);
                process.setArguments({QStringLiteral("-xf"), zipPath,
                                      QStringLiteral("-C"), directory});
                process.start();
                extracted = process.waitForFinished(120000)
                    && process.exitStatus() == QProcess::NormalExit
                    && process.exitCode() == 0;
                if (!extracted) {
                    appendLog(QStringLiteral("tar 解压失败，尝试 PowerShell..."));
                }
            }
            if (!extracted) {
                const QString escapedZip = QString(zipPath).replace(QLatin1Char('\''),
                                                                     QStringLiteral("''"));
                const QString escapedDir = QString(directory).replace(QLatin1Char('\''),
                                                                      QStringLiteral("''"));
                QProcess process;
                process.setProgram(QStandardPaths::findExecutable(
                    QStringLiteral("powershell.exe")));
                process.setArguments({
                    QStringLiteral("-NoProfile"),
                    QStringLiteral("-Command"),
                    QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                        .arg(escapedZip, escapedDir)
                });
                process.start();
                extracted = process.waitForFinished(120000)
                    && process.exitStatus() == QProcess::NormalExit
                    && process.exitCode() == 0;
            }

            if (!extracted) {
                appendLog(QStringLiteral("EasyTier 压缩包解压失败。"));
                finishEnsure(false);
                return;
            }

            QString binary;
            QDirIterator iterator(directory,
                                  QStringList{QStringLiteral("easytier-core.exe")},
                                  QDir::Files, QDirIterator::Subdirectories);
            if (iterator.hasNext()) {
                binary = iterator.next();
            }
            if (binary.isEmpty()) {
                appendLog(QStringLiteral("解压后没有找到 easytier-core.exe。"));
                finishEnsure(false);
                return;
            }

            const QFileInfo binaryInfo(binary);
            if (binaryInfo.absolutePath() != QDir(directory).absolutePath()) {
                QDir sourceDir(binaryInfo.absolutePath());
                const QFileInfoList files = sourceDir.entryInfoList(QDir::Files);
                for (const QFileInfo &file : files) {
                    const QString destination = QDir(directory).filePath(file.fileName());
                    if (file.absoluteFilePath() != destination) {
                        QFile::remove(destination);
                        QFile::copy(file.absoluteFilePath(), destination);
                    }
                }
                binary = QDir(directory).filePath(QStringLiteral("easytier-core.exe"));
            }

            m_easytierBinaryPath = binary;
            m_easyTier->setBinaryPath(binary);
            QFile::remove(zipPath);
            appendLog(QStringLiteral("EasyTier 已就绪：%1")
                          .arg(QDir::toNativeSeparators(binary)));
            finishEnsure(true);
        });
    });
}

void MultiplayerPage::finishEnsure(bool success)
{
    m_ensureInProgress = false;
    auto callback = std::move(m_ensureCallback);
    m_ensureCallback = nullptr;
    if (callback) {
        callback(success);
    }
}
