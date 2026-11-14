/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AccountDialog.h"

#include "AccountManager.h"
#include "AccentButton.h"
#include "BackgroundStore.h"
#include "CardWidget.h"
#include "Theme.h"
#include "WindowButton.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QOverload>
#include <QPainter>
#include <QPoint>
#include <QPainterPath>
#include <QSettings>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QImageReader>

namespace {

class AccountBadge : public QWidget
{
public:
    explicit AccountBadge(const QString &name, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        QString displayName = name.trimmed();
        while (!displayName.isEmpty() && !displayName.at(0).isLetterOrNumber()) {
            displayName.remove(0, 1);
        }
        m_letter = displayName.isEmpty() ? QStringLiteral("A")
                                         : displayName.left(1).toUpper();
        setFixedSize(34, 34);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(47, 128, 237, 90), 1.0));
        painter.setBrush(QColor(47, 128, 237, 26));
        painter.drawEllipse(box);
        painter.setPen(QColor(QStringLiteral("#1E6FE0")));
        painter.setFont(Theme::font(14, QFont::DemiBold));
        painter.drawText(box, Qt::AlignCenter, m_letter);
    }

private:
    QString m_letter;
};

QString accountRowStyle(bool active)
{
    const QString border = QStringLiteral("rgba(%1, %2, %3, %4)")
                               .arg(Theme::Border.red())
                               .arg(Theme::Border.green())
                               .arg(Theme::Border.blue())
                               .arg(Theme::Border.alpha());
    return active
        ? QStringLiteral("QFrame#accountRow {"
                         "background: rgba(47, 128, 237, 24);"
                         "border: 1px solid rgba(47, 128, 237, 120);"
                         "border-radius: 12px; }")
        : QStringLiteral("QFrame#accountRow {"
                         "background: %1;"
                         "border: 1px solid %2;"
                         "border-radius: 12px; }")
              .arg(Theme::Surface.name(), border);
}

} // namespace

AccountDialog::AccountDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("账户管理"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedSize(680, 690);

    reloadBackground();
    connect(&BackgroundStore::instance(), &BackgroundStore::changed,
            this, &AccountDialog::reloadBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 18, 18, 18);

    auto *panel = new CardWidget(this);
    panel->setInteractive(false);
    panel->setAccentColor(Theme::Accent);
    panel->setCornerRadius(18.0);
    outer->addWidget(panel);

    auto *shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(42);
    shadow->setOffset(0, 12);
    shadow->setColor(QColor(23, 36, 58, 55));
    panel->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(24, 21, 24, 20);
    layout->setSpacing(0);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);
    titleRow->addWidget(Theme::label(QStringLiteral("账户列表"), 20, QFont::DemiBold,
                                     Theme::TextPrimary, panel, true));
    titleRow->addStretch();
    auto *closeButton = new WindowButton(Icons::Type::Close, true, panel);
    closeButton->setFixedSize(32, 28);
    closeButton->setToolTip(QStringLiteral("关闭"));
    closeButton->setAccessibleName(QStringLiteral("关闭账户窗口"));
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    layout->addSpacing(5);
    layout->addWidget(Theme::label(
        QStringLiteral("支持离线账户和第三方外置登录（authlib-injector）。"),
        11, QFont::Normal, Theme::TextMuted, panel));
    layout->addSpacing(14);

    m_scroll = new QScrollArea(panel);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);

    auto *listContent = new QWidget();
    listContent->setAttribute(Qt::WA_NoSystemBackground);
    m_listLayout = new QVBoxLayout(listContent);
    m_listLayout->setContentsMargins(0, 0, 7, 2);
    m_listLayout->setSpacing(8);
    m_scroll->setWidget(listContent);
    layout->addWidget(m_scroll, 1);

    layout->addSpacing(15);
    m_formLabel = Theme::label(QStringLiteral("添加账户"), 10, QFont::DemiBold,
                               Theme::TextSecondary, panel);
    layout->addWidget(m_formLabel);
    layout->addSpacing(6);

    auto *typeRow = new QHBoxLayout();
    typeRow->setContentsMargins(0, 0, 0, 0);
    typeRow->setSpacing(9);
    typeRow->addWidget(Theme::label(QStringLiteral("账号类型"), 10, QFont::Normal,
                                    Theme::TextMuted, panel));
    m_accountType = new QComboBox(panel);
    m_accountType->addItem(QStringLiteral("离线账户"), QStringLiteral("offline"));
    m_accountType->addItem(QStringLiteral("第三方外置登录"),
                           QStringLiteral("external"));
    m_accountType->setMinimumHeight(36);
    m_accountType->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    typeRow->addWidget(m_accountType, 1);
    layout->addLayout(typeRow);
    layout->addSpacing(6);

    auto *usernameRow = new QHBoxLayout();
    usernameRow->setContentsMargins(0, 0, 0, 0);
    usernameRow->setSpacing(9);
    usernameRow->addWidget(Theme::label(QStringLiteral("用户名"), 10, QFont::Normal,
                                        Theme::TextMuted, panel));
    m_usernameEdit = new QLineEdit(panel);
    m_usernameEdit->setPlaceholderText(QStringLiteral("输入用户名"));
    m_usernameEdit->setMaxLength(16);
    m_usernameEdit->setMinimumHeight(38);
    m_usernameEdit->setClearButtonEnabled(true);
    usernameRow->addWidget(m_usernameEdit, 1);
    layout->addLayout(usernameRow);

    m_externalFields = new QWidget(panel);
    auto *externalLayout = new QFormLayout(m_externalFields);
    externalLayout->setContentsMargins(0, 6, 0, 0);
    externalLayout->setHorizontalSpacing(9);
    externalLayout->setVerticalSpacing(6);
    externalLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_serverUrlEdit = new QLineEdit(m_externalFields);
    m_serverUrlEdit->setPlaceholderText(
        QStringLiteral("https://littleskin.cn/api/yggdrasil"));
    m_serverUrlEdit->setMinimumHeight(36);
    externalLayout->addRow(Theme::label(QStringLiteral("认证服务器"), 10,
                                        QFont::Normal, Theme::TextMuted,
                                        m_externalFields),
                           m_serverUrlEdit);

    m_accessTokenEdit = new QLineEdit(m_externalFields);
    m_accessTokenEdit->setPlaceholderText(QStringLiteral("accessToken"));
    m_accessTokenEdit->setEchoMode(QLineEdit::Password);
    m_accessTokenEdit->setMinimumHeight(36);
    externalLayout->addRow(Theme::label(QStringLiteral("Access Token"), 10,
                                        QFont::Normal, Theme::TextMuted,
                                        m_externalFields),
                           m_accessTokenEdit);

    m_uuidEdit = new QLineEdit(m_externalFields);
    m_uuidEdit->setPlaceholderText(QStringLiteral("玩家 UUID，可留空等待登录返回"));
    m_uuidEdit->setMinimumHeight(36);
    externalLayout->addRow(Theme::label(QStringLiteral("UUID"), 10, QFont::Normal,
                                        Theme::TextMuted, m_externalFields),
                           m_uuidEdit);

    m_clientTokenEdit = new QLineEdit(m_externalFields);
    m_clientTokenEdit->setPlaceholderText(QStringLiteral("clientToken"));
    m_clientTokenEdit->setEchoMode(QLineEdit::Password);
    m_clientTokenEdit->setMinimumHeight(36);
    externalLayout->addRow(Theme::label(QStringLiteral("Client Token"), 10,
                                        QFont::Normal, Theme::TextMuted,
                                        m_externalFields),
                           m_clientTokenEdit);
    m_externalFields->hide();
    layout->addWidget(m_externalFields);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 8, 0, 0);
    buttonRow->setSpacing(8);
    buttonRow->addStretch();
    m_submitButton = new AccentButton(QStringLiteral("添加账户"), panel);
    m_submitButton->setVariant(AccentButton::Variant::Primary);
    m_submitButton->setCompact(true);
    buttonRow->addWidget(m_submitButton);

    m_cancelButton = new AccentButton(QStringLiteral("取消"), panel);
    m_cancelButton->setVariant(AccentButton::Variant::Ghost);
    m_cancelButton->setCompact(true);
    m_cancelButton->hide();
    buttonRow->addWidget(m_cancelButton);
    layout->addLayout(buttonRow);

    m_errorLabel = Theme::label(QString(), 10, QFont::Normal, Theme::Danger, panel);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setFixedHeight(30);
    layout->addSpacing(3);
    layout->addWidget(m_errorLabel);

    connect(m_submitButton, &QAbstractButton::clicked, this, &AccountDialog::submitForm);
    connect(m_cancelButton, &QAbstractButton::clicked, this, &AccountDialog::leaveFormEdit);
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &AccountDialog::submitForm);
    connect(m_accountType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateEditorVisibility(); });
    connect(m_usernameEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_errorLabel && !m_errorLabel->text().isEmpty()) {
            setError(QString());
        }
    });

    rebuildList();
}

void AccountDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(rect()), 20.0, 20.0);
    painter.setClipPath(clipPath);
    painter.fillRect(rect(), Theme::BackgroundDeep);

    if (!m_backgroundImage.isNull() && m_backgroundOpacity > 0) {
        const qreal targetAspect = static_cast<qreal>(width()) / height();
        const qreal imageAspect = static_cast<qreal>(m_backgroundImage.width())
            / m_backgroundImage.height();
        QRectF sourceRect(m_backgroundImage.rect());
        if (imageAspect > targetAspect) {
            const qreal croppedWidth = m_backgroundImage.height() * targetAspect;
            sourceRect.setLeft((m_backgroundImage.width() - croppedWidth) / 2.0);
            sourceRect.setWidth(croppedWidth);
        } else {
            const qreal croppedHeight = m_backgroundImage.width() / targetAspect;
            sourceRect.setTop((m_backgroundImage.height() - croppedHeight) / 2.0);
            sourceRect.setHeight(croppedHeight);
        }

        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setOpacity(m_backgroundOpacity / 100.0);
        painter.drawImage(rect(), m_backgroundImage, sourceRect);
    }
}

void AccountDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    if (QWidget *host = parentWidget() ? parentWidget()->window() : nullptr) {
        const QPoint center = host->mapToGlobal(host->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    }
    reloadBackground();
    m_usernameEdit->setFocus(Qt::OtherFocusReason);
}

void AccountDialog::reloadBackground()
{
    m_backgroundImage = BackgroundStore::instance().image();
    m_backgroundOpacity = BackgroundStore::instance().opacity();
    update();
}

void AccountDialog::rebuildList()
{
    if (!m_listLayout || !m_scroll) {
        return;
    }

    while (QLayoutItem *item = m_listLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    AccountManager &manager = AccountManager::instance();
    const QString activeId = manager.activeAccountId();
    QWidget *content = m_scroll->widget();

    if (manager.accounts().isEmpty()) {
        auto *empty = Theme::label(
            QStringLiteral("还没有账户。在下方添加离线账户或第三方外置登录账户。"),
            11, QFont::Normal, Theme::TextMuted, content);
        empty->setAlignment(Qt::AlignCenter);
        empty->setFixedHeight(140);
        m_listLayout->addWidget(empty);
    }

    for (const PlayerAccount &account : manager.accounts()) {
        const bool active = account.id == activeId;
        auto *row = new QFrame(content);
        row->setObjectName(QStringLiteral("accountRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setFixedHeight(62);
        row->setStyleSheet(accountRowStyle(active));

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(10);
        rowLayout->addWidget(new AccountBadge(account.username, row), 0, Qt::AlignVCenter);

        auto *text = new QVBoxLayout();
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(2);
        auto *nameRow = new QHBoxLayout();
        nameRow->setContentsMargins(0, 0, 0, 0);
        nameRow->setSpacing(6);
        nameRow->addWidget(Theme::label(account.username, 12, QFont::DemiBold,
                                        Theme::TextPrimary, row));
        if (account.type == PlayerAccount::Type::External) {
            const QString server = account.serverLabel().isEmpty()
                ? QStringLiteral("外置")
                : account.serverLabel();
            nameRow->addWidget(Theme::badge(
                QStringLiteral("[%1]").arg(server), Theme::Accent,
                QColor(110, 231, 183, 22), row,
                QColor(110, 231, 183, 70)), 0, Qt::AlignVCenter);
        } else {
            nameRow->addWidget(Theme::badge(
                QStringLiteral("[离线]"), Theme::TextMuted,
                QColor(255, 255, 255, 14), row,
                QColor(255, 255, 255, 34)), 0, Qt::AlignVCenter);
        }
        nameRow->addStretch();
        text->addLayout(nameRow);
        QString subtitle = account.typeLabel();
        if (account.type == PlayerAccount::Type::External
            && !account.serverLabel().isEmpty()) {
            subtitle += QStringLiteral(" · ") + account.serverLabel();
        }
        text->addWidget(Theme::label(
            subtitle, 9, QFont::Normal,
            account.type == PlayerAccount::Type::External ? Theme::Accent
                                                          : Theme::TextMuted,
            row));
        rowLayout->addLayout(text, 1);

        if (active) {
            rowLayout->addWidget(
                Theme::badge(QStringLiteral("当前账户"), Theme::Accent,
                             QColor(47, 128, 237, 22), row,
                             QColor(47, 128, 237, 70)),
                0, Qt::AlignVCenter);
        } else {
            auto *activate = new AccentButton(QStringLiteral("设为启动"), row);
            activate->setVariant(AccentButton::Variant::Ghost);
            activate->setCompact(true);
            connect(activate, &QAbstractButton::clicked, this, [this, id = account.id]() {
                if (AccountManager::instance().setActiveAccount(id)) {
                    m_deleteArmedId.clear();
                    setError(QString());
                    rebuildList();
                }
            });
            rowLayout->addWidget(activate, 0, Qt::AlignVCenter);
        }

        auto *edit = new AccentButton(QStringLiteral("编辑"), row);
        edit->setVariant(AccentButton::Variant::Ghost);
        edit->setCompact(true);
        edit->setAccessibleName(QStringLiteral("编辑账户 %1").arg(account.username));
        connect(edit, &QAbstractButton::clicked, this, [this, id = account.id]() {
            beginEdit(id);
        });
        rowLayout->addWidget(edit, 0, Qt::AlignVCenter);

        const bool deleteArmed = m_deleteArmedId == account.id;
        auto *remove = new AccentButton(deleteArmed ? QStringLiteral("确认删除")
                                                    : QStringLiteral("删除"),
                                        row);
        remove->setVariant(AccentButton::Variant::Danger);
        remove->setCompact(true);
        remove->setAccessibleName(QStringLiteral("删除账户 %1").arg(account.username));
        connect(remove, &QAbstractButton::clicked, this, [this, id = account.id]() {
            handleDelete(id);
        });
        rowLayout->addWidget(remove, 0, Qt::AlignVCenter);

        m_listLayout->addWidget(row);
    }

    m_listLayout->addStretch(1);
}

void AccountDialog::beginEdit(const QString &id)
{
    const QVector<PlayerAccount> &accounts = AccountManager::instance().accounts();
    const PlayerAccount *selected = nullptr;
    for (const PlayerAccount &account : accounts) {
        if (account.id == id) {
            selected = &account;
            break;
        }
    }
    if (!selected) {
        return;
    }

    m_editingId = id;
    m_deleteArmedId.clear();
    m_formLabel->setText(QStringLiteral("编辑账户"));
    m_accountType->setCurrentIndex(
        selected->type == PlayerAccount::Type::External ? 1 : 0);
    m_usernameEdit->setText(selected->username);
    m_serverUrlEdit->setText(selected->serverUrl);
    m_accessTokenEdit->setText(selected->accessToken);
    m_uuidEdit->setText(selected->uuid);
    m_clientTokenEdit->setText(selected->clientToken);
    m_submitButton->setText(QStringLiteral("保存修改"));
    m_cancelButton->show();
    updateEditorVisibility();
    setError(QString());
    m_usernameEdit->setFocus(Qt::OtherFocusReason);
    m_usernameEdit->selectAll();
    rebuildList();
}

void AccountDialog::leaveFormEdit()
{
    m_editingId.clear();
    m_formLabel->setText(QStringLiteral("添加账户"));
    m_accountType->setCurrentIndex(0);
    m_usernameEdit->clear();
    m_serverUrlEdit->clear();
    m_accessTokenEdit->clear();
    m_uuidEdit->clear();
    m_clientTokenEdit->clear();
    m_submitButton->setText(QStringLiteral("添加账户"));
    m_cancelButton->hide();
    updateEditorVisibility();
    setError(QString());
}

void AccountDialog::updateEditorVisibility()
{
    if (!m_externalFields || !m_accountType) {
        return;
    }
    m_externalFields->setVisible(
        m_accountType->currentData().toString() == QStringLiteral("external"));
}

void AccountDialog::submitForm()
{
    AccountManager &manager = AccountManager::instance();
    QString error;

    PlayerAccount account;
    account.username = m_usernameEdit->text();
    account.type = m_accountType->currentData().toString()
                           == QStringLiteral("external")
                       ? PlayerAccount::Type::External
                       : PlayerAccount::Type::Offline;
    if (account.type == PlayerAccount::Type::External) {
        account.serverUrl = m_serverUrlEdit->text().trimmed();
        account.accessToken = m_accessTokenEdit->text().trimmed();
        account.uuid = m_uuidEdit->text().trimmed();
        account.clientToken = m_clientTokenEdit->text().trimmed();
    }

    if (!m_editingId.isEmpty()) {
        if (!manager.updateAccount(m_editingId, account, &error)) {
            setError(error);
            return;
        }
        leaveFormEdit();
        rebuildList();
        return;
    }

    if (!manager.addAccount(account, &error)) {
        setError(error);
        return;
    }

    leaveFormEdit();
    m_usernameEdit->setFocus(Qt::OtherFocusReason);
    rebuildList();
}

void AccountDialog::handleDelete(const QString &id)
{
    if (m_deleteArmedId != id) {
        m_deleteArmedId = id;
        rebuildList();
        return;
    }

    QString error;
    if (!AccountManager::instance().removeAccount(id, &error)) {
        setError(error);
        m_deleteArmedId.clear();
        rebuildList();
        return;
    }

    m_deleteArmedId.clear();
    if (m_editingId == id) {
        leaveFormEdit();
    }
    rebuildList();
}

void AccountDialog::setError(const QString &message)
{
    if (m_errorLabel) {
        m_errorLabel->setText(message);
    }
}
