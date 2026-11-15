/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 */

#include "ModernSelectionDialog.h"

#include "AccentButton.h"
#include "BackgroundStore.h"
#include "CardWidget.h"
#include "DialogBackground.h"
#include "ModernInputs.h"
#include "Theme.h"

#include <QAbstractItemView>
#include <QAbstractAnimation>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

ModernSelectionDialog::ModernSelectionDialog(QWidget *parent,
                                             const QString &title,
                                             const QString &description,
                                             const QStringList &items,
                                             int currentIndex)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setModal(true);
    resize(460, qBound(300, 230 + qMin(items.size(), 9) * 36, 600));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *card = new CardWidget(this);
    card->setAccentColor(Theme::Accent);
    card->setCornerRadius(18);
    card->setInteractive(false);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(Theme::label(title, 15, QFont::DemiBold,
                                     Theme::TextPrimary, card), 1);
    auto *closeButton = new AccentButton(QString(), Icons::Type::Close, card);
    closeButton->setVariant(AccentButton::Variant::Ghost);
    closeButton->setCompact(true);
    closeButton->setFixedSize(30, 30);
    connect(closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    auto *descriptionLabel = Theme::label(description, 10, QFont::Normal,
                                          Theme::TextMuted, card);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    m_search = new ModernLineEdit(card);
    m_search->setPlaceholderText(QStringLiteral("搜索…"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_list = new QListWidget(card);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; border: none; color: %1;"
        "outline: 0; padding: 0; }"
        "QListWidget::item { min-height: 30px; padding: 4px 9px; border-radius: 6px; }"
        "QListWidget::item:hover { background: rgba(255, 255, 255, 8); }"
        "QListWidget::item:selected {"
        "background: rgba(110, 231, 183, 24); color: %2; }")
        .arg(Theme::TextSecondary.name(), Theme::TextPrimary.name()));
    for (int index = 0; index < items.size(); ++index) {
        auto *item = new QListWidgetItem(items.at(index), m_list);
        item->setData(Qt::UserRole, index);
    }
    layout->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);
    buttons->addStretch();
    auto *cancelButton = new AccentButton(QStringLiteral("取消"),
                                          Icons::Type::Close, card);
    cancelButton->setVariant(AccentButton::Variant::Ghost);
    cancelButton->setCompact(true);
    connect(cancelButton, &QAbstractButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancelButton);
    auto *confirmButton = new AccentButton(QStringLiteral("确定"),
                                           Icons::Type::Check, card);
    confirmButton->setVariant(AccentButton::Variant::Primary);
    confirmButton->setCompact(true);
    connect(confirmButton, &QAbstractButton::clicked, this, [this]() { accept(); });
    buttons->addWidget(confirmButton);
    layout->addLayout(buttons);
    outer->addWidget(card);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem *item = m_list->item(row);
            item->setHidden(!text.isEmpty()
                            && !item->text().contains(text, Qt::CaseInsensitive));
        }
        if (m_list->currentItem() && m_list->currentItem()->isHidden()) {
            selectFirstVisible();
        }
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { accept(); });
    connect(m_search, &QLineEdit::returnPressed, this, [this]() { accept(); });

    if (currentIndex >= 0 && currentIndex < m_list->count()) {
        m_list->setCurrentRow(currentIndex);
        m_list->scrollToItem(m_list->currentItem(),
                             QAbstractItemView::PositionAtCenter);
    } else if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }

    QTimer::singleShot(0, m_search, [this]() { m_search->setFocus(); });

    connect(&BackgroundStore::instance(), &BackgroundStore::changed,
            this, [this]() { update(); });
}

void ModernSelectionDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    DialogBackground::paint(this, painter);
}

QString ModernSelectionDialog::getItem(QWidget *parent,
                                       const QString &title,
                                       const QString &description,
                                       const QStringList &items,
                                       int currentIndex,
                                       bool *accepted)
{
    if (accepted) {
        *accepted = false;
    }
    if (items.isEmpty()) {
        return QString();
    }

    ModernSelectionDialog dialog(parent, title, description, items, currentIndex);
    const int result = dialog.exec();
    if (result != QDialog::Accepted) {
        return QString();
    }
    if (accepted) {
        *accepted = true;
    }
    return dialog.m_selected;
}

void ModernSelectionDialog::selectFirstVisible()
{
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (!item->isHidden()) {
            m_list->setCurrentRow(row);
            return;
        }
    }
}

void ModernSelectionDialog::accept()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item || item->isHidden()) {
        selectFirstVisible();
        item = m_list->currentItem();
    }
    if (!item) {
        return;
    }
    m_selected = item->text();
    QDialog::accept();
}
