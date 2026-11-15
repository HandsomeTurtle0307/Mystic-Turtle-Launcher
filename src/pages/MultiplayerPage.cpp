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
#include "ModernInputs.h"

#include <QButtonGroup>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
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
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>
#include <exception>

namespace {

const QColor kCreateAccent = Theme::Accent;
const QColor kJoinAccent = Theme::Blue;

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

} // namespace

MultiplayerPage::MultiplayerPage(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setStyleSheet(QStringLiteral(
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
    titleBlock->addWidget(Theme::label(QStringLiteral("P2P 联机"), 21,
                                       QFont::DemiBold,
                                       Theme::TextPrimary, this, true));
    titleBlock->addWidget(Theme::label(
        QStringLiteral("通过 EasyTier 组建虚拟局域网；创建虚拟网卡需要管理员权限。"),
        9, QFont::Normal, Theme::TextMuted, this));
    m_easyTierPathLabel = Theme::label(QString(), 9, QFont::Normal,
                                       Theme::TextMuted, this);
    m_easyTierPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_easyTierPathLabel->setWordWrap(true);
    titleBlock->addWidget(m_easyTierPathLabel);
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

    auto *tabs = new QHBoxLayout();
    tabs->setContentsMargins(0, 0, 0, 0);
    tabs->setSpacing(6);
    const auto makeTab = [this](const QString &text, Icons::Type icon,
                                const QColor &accent) {
        auto *button = new AccentButton(text, icon, this);
        button->setVariant(AccentButton::Variant::Secondary);
        button->setAccentColor(accent);
        button->setCompact(true);
        button->setCheckable(true);
        button->setAccessibleName(text);
        return button;
    };
    auto *createTab = makeTab(QStringLiteral("创建房间"), Icons::Type::Play,
                              Theme::Accent);
    auto *joinTab = makeTab(QStringLiteral("加入房间"), Icons::Type::External,
                            Theme::Blue);
    auto *guideTab = makeTab(QStringLiteral("使用说明"), Icons::Type::Info,
                             Theme::Amber);
    auto *logTab = makeTab(QStringLiteral("运行日志"), Icons::Type::Package,
                           Theme::Purple);
    tabs->addWidget(createTab);
    tabs->addWidget(joinTab);
    tabs->addWidget(guideTab);
    tabs->addWidget(logTab);
    tabs->addStretch();
    root->addLayout(tabs);

    m_modeStack = new QStackedWidget(this);
    m_modeStack->setAttribute(Qt::WA_NoSystemBackground);

    const auto wrapPage = [this](QWidget *content) {
        auto *page = new QWidget(m_modeStack);
        page->setAttribute(Qt::WA_NoSystemBackground);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(content);
        layout->addStretch(1);
        return page;
    };
    m_modeStack->addWidget(wrapPage(buildCreateCard()));
    m_modeStack->addWidget(wrapPage(buildJoinCard()));
    m_modeStack->addWidget(buildGuidePage());
    m_modeStack->addWidget(buildConsole());
    root->addWidget(m_modeStack, 1);

    auto *tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);
    tabGroup->addButton(createTab, 0);
    tabGroup->addButton(joinTab, 1);
    tabGroup->addButton(guideTab, 2);
    tabGroup->addButton(logTab, 3);
    createTab->setChecked(true);
    connect(tabGroup, &QButtonGroup::idClicked, this, [this](int index) {
        if (m_modeStack) {
            m_modeStack->setCurrentIndex(index);
        }
    });

    updateEasyTierPathLabel();
    appendLog(QStringLiteral("P2P 页面已就绪，等待启动 EasyTier。"));
    appendLog(QStringLiteral("提示：EasyTier 需要管理员权限；首次运行请允许 EasyTier 和 Java 通过防火墙。"));
}

MultiplayerPage::~MultiplayerPage()
{
    if (m_easyTier) {
        m_easyTier->stop();
    }
}

void MultiplayerPage::shutdown()
{
    if (m_easyTier) {
        m_easyTier->stop();
    }
}

QLineEdit *MultiplayerPage::makeInput(const QString &placeholder,
                                      const QString &text,
                                      QWidget *parent)
{
    auto *input = new ModernLineEdit(text, parent);
    input->setObjectName(QStringLiteral("p2pInput"));
    input->setPlaceholderText(placeholder);
    input->setFixedHeight(36);
    input->setFont(Theme::font(12));
    input->setClearButtonEnabled(false);
    return input;
}

QWidget *MultiplayerPage::buildCreateCard()
{
    auto *card = new CardWidget(this);
    card->setAccentColor(kCreateAccent);
    card->setCornerRadius(16);
    card->setInteractive(false);

    auto *outer = new QVBoxLayout(card);
    outer->setContentsMargins(20, 18, 20, 18);
    outer->setSpacing(12);

    outer->addWidget(Theme::label(QStringLiteral("创建房间"), 16,
                                  QFont::DemiBold, Theme::TextPrimary, card));
    outer->addWidget(Theme::label(
        QStringLiteral("设置房间信息，开启一个虚拟局域网。"),
        10, QFont::Normal, Theme::TextMuted, card));

    auto *content = new QHBoxLayout();
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(28);

    auto *left = new QVBoxLayout();
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(8);
    left->addWidget(Theme::label(QStringLiteral("房间名称"), 11,
                                 QFont::DemiBold, Theme::TextSecondary, card));
    m_roomName = makeInput(QStringLiteral("输入房间名称"),
                           QStringLiteral("MysticTurtle 房间"), card);
    left->addWidget(m_roomName);

    left->addWidget(Theme::label(QStringLiteral("房间密码"), 11,
                                 QFont::DemiBold, Theme::TextSecondary, card));
    auto *passwordRow = new QHBoxLayout();
    passwordRow->setContentsMargins(0, 0, 0, 0);
    passwordRow->setSpacing(6);
    m_roomPassword = makeInput(QStringLiteral("6 位房间码"),
                               generateRoomCode(), card);
    m_refreshButton = new AccentButton(QString(), Icons::Type::Refresh, card);
    m_refreshButton->setVariant(AccentButton::Variant::Ghost);
    m_refreshButton->setToolTip(QStringLiteral("重新生成房间密码"));
    m_refreshButton->setFixedSize(36, 36);
    connect(m_refreshButton, &QAbstractButton::clicked,
            this, &MultiplayerPage::refreshRoomCode);
    passwordRow->addWidget(m_roomPassword, 1);
    passwordRow->addWidget(m_refreshButton);
    left->addLayout(passwordRow);

    left->addWidget(Theme::label(QStringLiteral("虚拟 IP 网段"), 11,
                                 QFont::DemiBold, Theme::TextSecondary, card));
    m_virtualIp = makeInput(QStringLiteral("例如 10.126.126.1"),
                            QStringLiteral("10.126.126.1"), card);
    m_virtualIp->setToolTip(
        QStringLiteral("推荐保持 10.126.126.1；如与本地网络冲突，可改为 "
                       "10.126.127.1 或 10.126.128.1。"));
    left->addWidget(m_virtualIp);
    auto *ipHint = Theme::label(
        QStringLiteral("房间内网段地址，不是公网 IP；冲突时改用 10.126.127.1。"),
        10, QFont::Normal, Theme::TextMuted, card);
    ipHint->setWordWrap(true);
    left->addWidget(ipHint);

    auto *right = new QVBoxLayout();
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(8);
    right->addWidget(Theme::label(QStringLiteral("房间创建后"), 11,
                                  QFont::DemiBold, Theme::TextSecondary, card));
    auto *createTip = Theme::label(
        QStringLiteral("把虚拟 IP 和房间密码发给朋友。进入游戏开放局域网后，"
                       "再告诉朋友游戏端口。"),
        11, QFont::Normal, Theme::TextMuted, card);
    createTip->setWordWrap(true);
    right->addWidget(createTip);
    auto *createTip2 = Theme::label(
        QStringLiteral("创建成功后，这里会显示复制虚拟 IP 和联机状态。"),
        10, QFont::Normal, Theme::TextMuted, card);
    createTip2->setWordWrap(true);
    right->addWidget(createTip2);
    right->addStretch(1);

    m_createButton = new AccentButton(QStringLiteral("一键开启联机"),
                                      Icons::Type::Play, card);
    m_createButton->setVariant(AccentButton::Variant::Primary);
    m_createButton->setAccentColor(kCreateAccent);
    m_createButton->setFont(Theme::font(14, QFont::DemiBold));
    m_createButton->setFixedHeight(50);
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
    right->addWidget(m_createButton);

    m_copyIpButton = new AccentButton(QStringLiteral("复制虚拟 IP"),
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
    right->addWidget(m_copyIpButton);

    m_createHint = Theme::label(
        QStringLiteral("开放局域网后，复制上方 IP 并告诉朋友端口。"),
        10, QFont::Normal, Theme::TextMuted, card);
    m_createHint->setWordWrap(true);
    m_createHint->hide();
    right->addWidget(m_createHint);

    m_createStatus = Theme::label(QStringLiteral("等待操作"), 11,
                                  QFont::Normal, Theme::TextMuted, card);
    m_createStatus->setWordWrap(true);

    left->addSpacing(6);
    left->addWidget(m_createButton);
    left->addWidget(m_copyIpButton);
    left->addWidget(m_createHint);
    left->addWidget(m_createStatus);
    left->addStretch(1);

    content->addLayout(left, 1);
    content->addLayout(right, 1);
    outer->addLayout(content);
    return card;
}

QWidget *MultiplayerPage::buildJoinCard()
{
    auto *card = new CardWidget(this);
    card->setAccentColor(kJoinAccent);
    card->setCornerRadius(16);
    card->setInteractive(false);

    auto *outer = new QVBoxLayout(card);
    outer->setContentsMargins(20, 18, 20, 18);
    outer->setSpacing(12);

    outer->addWidget(Theme::label(QStringLiteral("加入房间"), 16,
                                  QFont::DemiBold, Theme::TextPrimary, card));
    outer->addWidget(Theme::label(
        QStringLiteral("使用房间码加入，再复制游戏直连地址。"),
        10, QFont::Normal, Theme::TextMuted, card));

    auto *content = new QHBoxLayout();
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(28);

    auto *left = new QVBoxLayout();
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(8);
    left->addWidget(Theme::label(QStringLiteral("EasyTier 网络"), 11,
                                 QFont::DemiBold, Theme::TextSecondary, card));
    m_joinCode = makeInput(QStringLiteral("房间名#密码"), QString(), card);
    left->addWidget(m_joinCode);
    auto *formatHint = Theme::label(
        QStringLiteral("输入房主分享的“房间名#密码”。"),
        10, QFont::Normal, Theme::TextMuted, card);
    formatHint->setWordWrap(true);
    left->addWidget(formatHint);
    left->addSpacing(4);

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
            setJoinStatus(QStringLiteral("请先输入房间码。"));
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
    left->addWidget(m_joinButton);
    m_joinStatus = Theme::label(QStringLiteral("等待操作"), 11,
                                QFont::Normal, Theme::TextMuted, card);
    m_joinStatus->setWordWrap(true);
    left->addWidget(m_joinStatus);
    left->addStretch(1);

    auto *divider = new QFrame(card);
    divider->setFrameShape(QFrame::VLine);
    divider->setStyleSheet(QStringLiteral("QFrame { color: rgba(255, 255, 255, 18); }"));
    divider->setFixedWidth(1);
    content->addLayout(left, 1);
    content->addWidget(divider);

    auto *right = new QVBoxLayout();
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(8);
    right->addWidget(Theme::label(QStringLiteral("游戏直连"), 11,
                                  QFont::DemiBold, Theme::TextSecondary, card));
    auto *directHint = Theme::label(
        QStringLiteral("加入网络后，填写房主虚拟 IP 和游戏端口。"),
        10, QFont::Normal, Theme::TextMuted, card);
    directHint->setWordWrap(true);
    right->addWidget(directHint);

    auto *directRow = new QHBoxLayout();
    directRow->setContentsMargins(0, 0, 0, 0);
    directRow->setSpacing(6);
    m_hostIp = makeInput(QStringLiteral("房主虚拟 IP"), QString(), card);
    m_gamePort = makeInput(QStringLiteral("端口"), QString(), card);
    m_gamePort->setFixedWidth(82);
    directRow->addWidget(m_hostIp, 1);
    directRow->addWidget(m_gamePort);
    right->addLayout(directRow);

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
    right->addWidget(m_connectHostButton);

    auto *directNote = Theme::label(
        QStringLiteral("复制后，在游戏内“多人游戏 → 直接连接”中粘贴加入。"),
        10, QFont::Normal, Theme::TextMuted, card);
    directNote->setWordWrap(true);
    right->addWidget(directNote);
    right->addStretch(1);

    content->addLayout(right, 1);
    outer->addLayout(content, 1);
    return card;
}

QWidget *MultiplayerPage::buildGuidePage()
{
    auto *page = new QWidget(this);
    page->setAttribute(Qt::WA_NoSystemBackground);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *card = new CardWidget(page);
    card->setAccentColor(Theme::Amber);
    card->setCornerRadius(16);
    card->setInteractive(false);
    auto *outer = new QVBoxLayout(card);
    outer->setContentsMargins(22, 18, 22, 18);
    outer->setSpacing(12);

    outer->addWidget(Theme::label(QStringLiteral("使用说明"), 16,
                                  QFont::DemiBold, Theme::TextPrimary, card));
    outer->addWidget(Theme::label(
        QStringLiteral("按顺序完成四步，就可以和朋友进入同一个世界。"),
        10, QFont::Normal, Theme::TextMuted, card));

    const struct {
        const char *number;
        const char *title;
        const char *description;
    } steps[] = {
        {"1", "双方加入同一房间", "输入相同的房间名和密码，等待状态变为已连接对端节点。"},
        {"2", "房主开放局域网", "房主进入 Minecraft，在游戏菜单中点击“对局域网开放”。"},
        {"3", "复制 IP 和端口", "房主复制虚拟 IP 和游戏生成的端口，发给朋友。"},
        {"4", "朋友直接连接", "朋友在“多人游戏 → 直接连接”中输入 IP:端口并加入。"}
    };

    for (const auto &step : steps) {
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 2, 0, 2);
        row->setSpacing(12);
        auto *number = Theme::label(QString::fromLatin1(step.number), 11,
                                    QFont::DemiBold, Theme::Amber, card);
        number->setAlignment(Qt::AlignCenter);
        number->setFixedSize(26, 26);
        number->setStyleSheet(QStringLiteral(
            "background: rgba(255, 198, 109, 16);"
            "border: 1px solid rgba(255, 198, 109, 38);"
            "border-radius: 13px; color: %1;")
            .arg(Theme::Amber.name()));
        row->addWidget(number, 0, Qt::AlignTop);
        auto *text = new QVBoxLayout();
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(1);
        text->addWidget(Theme::label(QString::fromUtf8(step.title), 11,
                                     QFont::DemiBold, Theme::TextPrimary, card));
        auto *description = Theme::label(QString::fromUtf8(step.description), 10,
                                         QFont::Normal, Theme::TextMuted, card);
        description->setWordWrap(true);
        text->addWidget(description);
        row->addLayout(text, 1);
        outer->addLayout(row);
    }

    outer->addSpacing(4);
    outer->addWidget(Theme::divider(card));
    outer->addSpacing(4);

    outer->addWidget(Theme::label(QStringLiteral("虚拟 IP 怎么填"), 11,
                                  QFont::DemiBold, Theme::TextSecondary, card));
    auto *ipHelp = Theme::label(
        QStringLiteral("虚拟 IP 是 EasyTier 房间内的局域网地址，不是公网 IP。"
                       "推荐保持 10.126.126.1；如果本地网络也使用这个网段，"
                       "请改成 10.126.127.1。IP 冲突会导致朋友找不到房间或连接超时。"),
        10, QFont::Normal, Theme::TextMuted, card);
    ipHelp->setWordWrap(true);
    outer->addWidget(ipHelp);

    outer->addWidget(Theme::label(QStringLiteral("游戏直连怎么连"), 11,
                                  QFont::DemiBold, Theme::TextSecondary, card));
    auto *gameHelp = Theme::label(
        QStringLiteral("加入方需要先进入同一个 EasyTier 房间，再填写房主的虚拟 IP "
                       "和游戏端口。复制 IP:端口后，到 Minecraft 的“直接连接”中粘贴。"),
        10, QFont::Normal, Theme::TextMuted, card);
    gameHelp->setWordWrap(true);
    outer->addWidget(gameHelp);

    outer->addWidget(Theme::label(QStringLiteral("权限与防火墙"), 11,
                                  QFont::DemiBold, Theme::TextSecondary, card));
    auto *permissionHelp = Theme::label(
        QStringLiteral("EasyTier 创建虚拟网卡需要管理员权限。首次运行时请允许 "
                       "EasyTier 和 Java 通过 Windows 防火墙，否则可能显示已连接但游戏无法进入。"),
        10, QFont::Normal, Theme::TextMuted, card);
    permissionHelp->setWordWrap(true);
    outer->addWidget(permissionHelp);

    outer->addStretch(1);
    pageLayout->addWidget(card, 1);
    return page;
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
    header->addWidget(Theme::label(QStringLiteral("EASYTIER"), 8,
                                   QFont::Normal, Theme::TextMuted, card));
    header->addStretch();
    auto *checkEasyTier = new AccentButton(QStringLiteral("检查 EasyTier 状态"),
                                            Icons::Type::Info, card);
    checkEasyTier->setVariant(AccentButton::Variant::Ghost);
    checkEasyTier->setCompact(true);
    checkEasyTier->setToolTip(QStringLiteral("检查 EasyTier 是否已安装并可运行"));
    connect(checkEasyTier, &QAbstractButton::clicked, this, [this, checkEasyTier]() {
        const QString directory = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("tools/easytier"));
        const QString binary = QDir(directory).filePath(
            QStringLiteral("easytier-core.exe"));
        if (QFileInfo::exists(binary)) {
            m_easytierBinaryPath = binary;
            m_easyTier->setBinaryPath(binary);
            updateEasyTierPathLabel();
            setCreateStatus(QStringLiteral("EasyTier 已安装，P2P 联机可用。"));
            setJoinStatus(QStringLiteral("EasyTier 已安装，P2P 联机可用。"));
            QMessageBox::information(
                this, QStringLiteral("EasyTier 状态"),
                QStringLiteral("EasyTier 已安装，P2P 联机功能可用。\n\n路径：%1\n进程状态：%2")
                    .arg(QDir::toNativeSeparators(binary),
                         m_easyTier->isRunning() ? QStringLiteral("运行中")
                                                  : QStringLiteral("未运行")));
            return;
        }

        if (m_ensureInProgress) {
            appendLog(QStringLiteral("EasyTier 正在自动安装中，请稍候。"));
            QMessageBox::information(
                this, QStringLiteral("EasyTier 正在安装"),
                QStringLiteral("EasyTier 正在自动下载或解压，请稍候。"));
            return;
        }

        const auto choice = QMessageBox::question(
            this, QStringLiteral("未检测到 EasyTier"),
            QStringLiteral("当前电脑还没有安装 EasyTier。\n\n"
                           "P2P 联机需要 EasyTier 提供虚拟局域网能力。\n"
                           "是否现在自动下载并安装 EasyTier？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (choice != QMessageBox::Yes) {
            return;
        }

        checkEasyTier->setEnabled(false);
        appendLog(QStringLiteral("开始自动安装 EasyTier..."));
        setCreateStatus(QStringLiteral("正在自动安装 EasyTier，请稍候..."));
        setJoinStatus(QStringLiteral("正在自动安装 EasyTier，请稍候..."));
        ensureEasyTier([this, checkEasyTier](bool success) {
            checkEasyTier->setEnabled(true);
            if (success) {
                setCreateStatus(QStringLiteral("EasyTier 安装完成，P2P 联机可用。"));
                setJoinStatus(QStringLiteral("EasyTier 安装完成，P2P 联机可用。"));
                QMessageBox::information(
                    this, QStringLiteral("EasyTier 安装完成"),
                    QStringLiteral("EasyTier 已自动安装完成，现在可以创建或加入联机房间了。"));
                return;
            }

            setConnectionState(QStringLiteral("P2P 不可用"));
            setCreateStatus(QStringLiteral("EasyTier 安装失败，P2P 联机无法使用。请查看日志。"));
            setJoinStatus(QStringLiteral("EasyTier 安装失败，P2P 联机无法使用。请查看日志。"));
            QMessageBox::warning(
                this, QStringLiteral("EasyTier 安装失败"),
                QStringLiteral("EasyTier 自动下载或解压失败，P2P 联机功能暂时无法使用。\n"
                               "请检查网络后重试，或查看底部运行日志。"));
        });
    });
    header->addWidget(checkEasyTier);
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
    auto *exportLog = new AccentButton(QStringLiteral("导出日志"),
                                        Icons::Type::Download, card);
    exportLog->setVariant(AccentButton::Variant::Ghost);
    exportLog->setCompact(true);
    exportLog->setToolTip(QStringLiteral("把 EasyTier 运行日志保存到本地文件"));
    connect(exportLog, &QAbstractButton::clicked, this, [this]() {
        if (!m_log) {
            return;
        }
        QString directory =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (directory.isEmpty()) {
            directory = QDir::homePath();
        }
        const QString suggested = QDir(directory).filePath(
            QStringLiteral("MysticTurtle-P2P-%1.log")
                .arg(QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyyMMdd-HHmmss"))));
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出 EasyTier 日志"), suggested,
            QStringLiteral("日志文件 (*.log);;文本文件 (*.txt);;所有文件 (*)"));
        if (path.isEmpty()) {
            return;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            appendLog(QStringLiteral("导出日志失败：%1").arg(file.errorString()));
            return;
        }
        const QByteArray data = m_log->toPlainText().toUtf8();
        if (file.write(data) != data.size() || !file.commit()) {
            appendLog(QStringLiteral("导出日志失败：无法完整写入文件。"));
            return;
        }
        appendLog(QStringLiteral("日志已导出：%1")
                      .arg(QDir::toNativeSeparators(path)));
    });
    header->addWidget(exportLog);
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
    m_log->setMinimumHeight(280);
    m_log->setFont(QFont(QStringLiteral("Consolas"), 10));
    m_log->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_log, 1);
    return card;
}

void MultiplayerPage::refreshRoomCode()
{
    if (m_roomPassword) {
        m_roomPassword->setText(generateRoomCode());
    }
}

void MultiplayerPage::updateEasyTierPathLabel()
{
    if (!m_easyTierPathLabel) {
        return;
    }
    const QString directory = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("tools/easytier"));
    const QString binary = QDir(directory).filePath(
        QStringLiteral("easytier-core.exe"));
    const QString nativePath = QDir::toNativeSeparators(directory);
    QString displayPath = nativePath;
    if (displayPath.size() > 52) {
        displayPath = QStringLiteral("…\\tools\\easytier");
    }
    m_easyTierPathLabel->setText(
        QStringLiteral("EasyTier 安装位置：%1%2")
            .arg(displayPath,
                 QFileInfo::exists(binary)
                     ? QStringLiteral("（已安装）")
                     : QStringLiteral("（未安装，首次使用自动下载）")));
    m_easyTierPathLabel->setToolTip(
        QStringLiteral("EasyTier 安装目录：%1").arg(nativePath));
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
    emit connectionStateChanged(text);

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
    try {
        m_easyTier->setBinaryPath(m_easytierBinaryPath);
        m_easyTier->startCreate(roomName, password, virtualIp);
    } catch (const std::exception &exception) {
        const QString message = QStringLiteral("启动 EasyTier 失败：%1")
                                    .arg(QString::fromLocal8Bit(exception.what()));
        appendLog(message);
        resetConnectionUi(QStringLiteral("连接失败"), message,
                          QStringLiteral("等待操作"));
    } catch (...) {
        const QString message = QStringLiteral("启动 EasyTier 失败：未知错误。");
        appendLog(message);
        resetConnectionUi(QStringLiteral("连接失败"), message,
                          QStringLiteral("等待操作"));
    }
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
    try {
        m_easyTier->setBinaryPath(m_easytierBinaryPath);
        m_easyTier->startJoin(networkName, password);
    } catch (const std::exception &exception) {
        const QString message = QStringLiteral("启动 EasyTier 失败：%1")
                                    .arg(QString::fromLocal8Bit(exception.what()));
        appendLog(message);
        resetConnectionUi(QStringLiteral("连接失败"), QStringLiteral("等待操作"),
                          message);
    } catch (...) {
        const QString message = QStringLiteral("启动 EasyTier 失败：未知错误。");
        appendLog(message);
        resetConnectionUi(QStringLiteral("连接失败"), QStringLiteral("等待操作"),
                          message);
    }
}

void MultiplayerPage::ensureEasyTier(std::function<void(bool)> callback)
{
    const QString directory = QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("tools/easytier"));
    const QString binary = QDir(directory).filePath(QStringLiteral("easytier-core.exe"));

    if (QFileInfo::exists(binary)) {
        m_easytierBinaryPath = binary;
        m_easyTier->setBinaryPath(binary);
        updateEasyTierPathLabel();
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
    request.setTransferTimeout(20000);
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

        m_easyTierDirectory = directory;
        m_easyTierZipPath = QDir(directory).filePath(
            QStringLiteral("easytier-download.zip"));
        m_easyTierDownloadUrls.clear();
        const QStringList mirrorPrefixes = {
            QStringLiteral("https://ghfast.top/"),
            QStringLiteral("https://gh-proxy.com/"),
            QStringLiteral("https://ghproxy.net/")
        };
        for (const QString &prefix : mirrorPrefixes) {
            m_easyTierDownloadUrls.append(prefix + downloadUrl);
        }
        // 官方地址作为最后兜底。
        m_easyTierDownloadUrls.append(downloadUrl);
        m_easyTierDownloadIndex = 0;
        startEasyTierDownload();
    });
}

void MultiplayerPage::startEasyTierDownload()
{
    if (m_easyTierDownloadIndex >= m_easyTierDownloadUrls.size()) {
        appendLog(QStringLiteral(
            "EasyTier 下载失败：已尝试国内镜像和 GitHub 官方源，请检查网络后重试。"));
        finishEnsure(false);
        return;
    }

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    QFile::remove(m_easyTierZipPath);

    const int sourceIndex = m_easyTierDownloadIndex;
    const QString url = m_easyTierDownloadUrls.at(sourceIndex);
    appendLog(QStringLiteral("EasyTier 下载源 %1/%2：%3")
                  .arg(sourceIndex + 1)
                  .arg(m_easyTierDownloadUrls.size())
                  .arg(sourceIndex == m_easyTierDownloadUrls.size() - 1
                           ? QStringLiteral("GitHub 官方")
                           : QStringLiteral("国内镜像")));

    m_downloadFile = new QFile(m_easyTierZipPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        appendLog(QStringLiteral("无法写入 EasyTier 下载文件：%1")
                      .arg(m_downloadFile->errorString()));
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        finishEnsure(false);
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("MysticTurtle/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(90000);
    m_downloadReply = m_network->get(request);

    auto lastPercent = std::make_shared<int>(-1);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this, lastPercent](qint64 received, qint64 total) {
        const int percent = total > 0 ? int(received * 100 / total) : -1;
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
            [this, sourceIndex]() {
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

        bool failed = !reply || reply->error() != QNetworkReply::NoError;
        const QString errorString = reply ? reply->errorString()
                                          : QStringLiteral("未知错误");
        if (reply) {
            reply->deleteLater();
        }
        if (!failed) {
            const QFileInfo zipInfo(m_easyTierZipPath);
            if (!zipInfo.exists() || zipInfo.size() < 100 * 1024) {
                failed = true;
            } else {
                QFile zip(m_easyTierZipPath);
                if (!zip.open(QIODevice::ReadOnly) || zip.read(2) != "PK") {
                    failed = true;
                }
            }
        }

        if (failed) {
            appendLog(QStringLiteral("下载源失败（%1），尝试下一个来源...")
                          .arg(errorString));
            QFile::remove(m_easyTierZipPath);
            ++m_easyTierDownloadIndex;
            QTimer::singleShot(250, this, &MultiplayerPage::startEasyTierDownload);
            return;
        }

        appendLog(QStringLiteral("EasyTier 下载完成，开始解压..."));
        finishEasyTierArchive();
    });
}

void MultiplayerPage::finishEasyTierArchive()
{
    bool extracted = false;
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (!tar.isEmpty()) {
        QProcess process;
        process.setProgram(tar);
        process.setArguments({QStringLiteral("-xf"), m_easyTierZipPath,
                              QStringLiteral("-C"), m_easyTierDirectory});
        process.start();
        extracted = process.waitForFinished(120000)
            && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0;
        if (!extracted) {
            appendLog(QStringLiteral("tar 解压失败，尝试 PowerShell..."));
        }
    }
    if (!extracted) {
        const QString escapedZip = QString(m_easyTierZipPath).replace(
            QLatin1Char('\''), QStringLiteral("''"));
        const QString escapedDir = QString(m_easyTierDirectory).replace(
            QLatin1Char('\''), QStringLiteral("''"));
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
    QDirIterator iterator(m_easyTierDirectory,
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
    if (binaryInfo.absolutePath() != QDir(m_easyTierDirectory).absolutePath()) {
        QDir sourceDir(binaryInfo.absolutePath());
        const QFileInfoList files = sourceDir.entryInfoList(QDir::Files);
        for (const QFileInfo &file : files) {
            const QString destination = QDir(m_easyTierDirectory).filePath(file.fileName());
            if (file.absoluteFilePath() != destination) {
                QFile::remove(destination);
                QFile::copy(file.absoluteFilePath(), destination);
            }
        }
        binary = QDir(m_easyTierDirectory).filePath(QStringLiteral("easytier-core.exe"));
    }

    m_easytierBinaryPath = binary;
    m_easyTier->setBinaryPath(binary);
    updateEasyTierPathLabel();
    QFile::remove(m_easyTierZipPath);
    appendLog(QStringLiteral("EasyTier 已就绪：%1")
                  .arg(QDir::toNativeSeparators(binary)));
    finishEnsure(true);
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
