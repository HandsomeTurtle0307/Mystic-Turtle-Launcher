/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DonationPage.h"

#include "CardWidget.h"
#include "Theme.h"

#include <QAbstractAnimation>
#include <QFrame>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QRadialGradient>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QtMath>

DonationPage::DonationPage(QWidget *parent)
    : QWidget(parent)
    , m_glowAnimation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    m_animationsEnabled = Theme::animationsEnabled();
    m_glowAnimation->setStartValue(0.0);
    m_glowAnimation->setEndValue(1.0);
    m_glowAnimation->setDuration(6200);
    m_glowAnimation->setLoopCount(-1);
    m_glowAnimation->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_glowAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { setGlowProgress(value.toReal()); });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(34, 26, 34, 24);
    root->setSpacing(0);
    root->addWidget(Theme::label(QStringLiteral("支持 MysticTurtle"), 27,
                                 QFont::DemiBold, Theme::TextPrimary, this, true));
    root->addWidget(Theme::label(QStringLiteral("感谢每一份理解与支持"), 11,
                                 QFont::Normal, Theme::TextMuted, this));
    root->addSpacing(20);

    auto *cardRow = new QHBoxLayout();
    cardRow->setContentsMargins(0, 0, 0, 0);
    cardRow->addStretch();

    auto *card = new CardWidget(this);
    card->setAccentColor(QColor(QStringLiteral("#56D998")));
    card->setInteractive(true);
    card->setMinimumWidth(0);
    card->setMaximumWidth(1040);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 8);
    cardLayout->setSpacing(0);

    auto *panel = new QFrame(card);
    panel->setObjectName(QStringLiteral("donationPanel"));
    panel->setStyleSheet(QStringLiteral(
        "QFrame#donationPanel { background: rgba(8, 16, 24, 232);"
        "border: 1px solid rgba(135, 205, 190, 50); border-radius: 16px; }"));
    auto *content = new QHBoxLayout(panel);
    content->setContentsMargins(28, 24, 28, 24);
    content->setSpacing(24);

    auto *message = new QVBoxLayout();
    message->setContentsMargins(0, 0, 0, 0);
    message->setSpacing(10);

    auto *sectionTitle = Theme::label(QStringLiteral("关于赞赏和更新"), 19,
                                      QFont::DemiBold, Theme::TextPrimary, panel);
    message->addWidget(sectionTitle);
    message->addWidget(Theme::divider(panel));

    auto *intro = new QLabel(panel);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral(
        "QLabel { color: #D7E2E5; font-size: 13px; line-height: 155%; }"));
    intro->setText(QStringLiteral(
        "感谢你使用 MysticTurtle！如果你觉得这个小软件还不错，欢迎扫码请作者吃点好吃的，"
        "或者赞助一点点的 Token！<br>"
        "（毕竟这软件不少代码是 AI 写的，我现在三天就能用完一个月的模型额度，呜~）"));
    message->addWidget(intro);
    message->addSpacing(2);

    auto *subheading = Theme::label(QStringLiteral("一些你需要知道的："), 13,
                                    QFont::DemiBold,
                                    QColor(QStringLiteral("#8CE8B7")), panel);
    message->addWidget(subheading);

    auto *notice = new QLabel(panel);
    notice->setTextFormat(Qt::RichText);
    notice->setWordWrap(true);
    notice->setStyleSheet(QStringLiteral(
        "QLabel { color: #C0CDD2; font-size: 12px; line-height: 155%; }"));
    notice->setText(QStringLiteral(
        "作者是一名标准的学生，目前身心状况需要长期休养，且学业繁忙（是真的忙），"
        "因此 <b><font color=\"#FFD078\">无法保证频繁且连续的更新</font></b>。<br><br>"
        "如果你打赏是为了“预购”未来的版本或者“催更”，那请三思"
        "（我建议这种目的就先别扫啦）。<br><br>"
        "如果以上这些你都能接受，并且单纯只是想支持一下这个项目，"
        "欢迎扫描旁边的二维码。感谢大家的包容与支持！🙏🙏🙏"));
    message->addWidget(notice);
    message->addStretch(1);
    content->addLayout(message, 1);

    auto *qrColumn = new QVBoxLayout();
    qrColumn->setContentsMargins(0, 0, 0, 0);
    qrColumn->setSpacing(10);
    auto *qrFrame = new QFrame(panel);
    qrFrame->setObjectName(QStringLiteral("donationQrFrame"));
    qrFrame->setFixedSize(300, 300);
    qrFrame->setStyleSheet(QStringLiteral(
        "QFrame#donationQrFrame { background: #FFFFFF; border: 3px solid #65DFA7;"
        "border-radius: 16px; }"));
    auto *qrLayout = new QVBoxLayout(qrFrame);
    qrLayout->setContentsMargins(10, 10, 10, 10);
    auto *qr = new QLabel(qrFrame);
    qr->setAlignment(Qt::AlignCenter);
    qr->setFixedSize(274, 274);
    qr->setAccessibleName(QStringLiteral("微信捐赠二维码"));
    const QImage donationPoster(QStringLiteral(":/assets/donation-qr.png"));
    if (!donationPoster.isNull()) {
        const QRect qrCrop(565, 305, 470, 470);
        const QImage codeImage = donationPoster.copy(qrCrop);
        if (!codeImage.isNull()) {
            qr->setPixmap(QPixmap::fromImage(codeImage).scaled(
                qr->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
        } else {
            qr->setText(QStringLiteral("二维码裁切失败"));
        }
    } else {
        qr->setText(QStringLiteral("二维码加载失败"));
        qr->setStyleSheet(QStringLiteral("color: %1; background: white;")
                               .arg(Theme::Danger.name()));
    }
    qrLayout->addWidget(qr);
    qrColumn->addWidget(qrFrame, 0, Qt::AlignCenter);
    auto *scanHint = Theme::label(QStringLiteral("微信扫码 · 自愿赞赏"), 11,
                                  QFont::DemiBold,
                                  QColor(QStringLiteral("#8CE8B7")), panel);
    scanHint->setAlignment(Qt::AlignCenter);
    qrColumn->addWidget(scanHint);
    content->addLayout(qrColumn, 0);
    cardLayout->addWidget(panel);

    cardRow->addWidget(card);
    cardRow->addStretch();
    root->addLayout(cardRow);
    root->addStretch(1);
    updateAnimationState();
}

void DonationPage::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    updateAnimationState();
}

qreal DonationPage::glowProgress() const
{
    return m_glowProgress;
}

void DonationPage::setGlowProgress(qreal progress)
{
    if (qFuzzyCompare(m_glowProgress + 1.0, progress + 1.0)) {
        return;
    }
    m_glowProgress = progress;
    update();
}

void DonationPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    constexpr qreal pi = 3.14159265358979323846;
    const qreal phase = m_glowProgress * 2.0 * pi;
    const qreal firstPulse = 0.5 + 0.5 * qSin(phase);
    const qreal secondPulse = 0.5 + 0.5 * qSin(phase + pi);
    const auto drawGlow = [this, &painter](const QPointF &center,
                                           const QColor &color,
                                           qreal pulse) {
        const qreal radius = qMax(width(), height()) * (0.38 + pulse * 0.12);
        QRadialGradient gradient(center, radius);
        QColor core = color;
        core.setAlphaF(m_animationsEnabled ? 0.14 + pulse * 0.10 : 0.17);
        gradient.setColorAt(0.0, core);
        QColor edge = color;
        edge.setAlpha(0);
        gradient.setColorAt(1.0, edge);
        painter.fillRect(rect(), gradient);
    };

    const QPointF blueCenter(width() * (0.17 + firstPulse * 0.10),
                             height() * (0.24 + secondPulse * 0.12));
    const QPointF greenCenter(width() * (0.82 - secondPulse * 0.10),
                              height() * (0.78 - firstPulse * 0.12));
    drawGlow(blueCenter, Theme::Blue, firstPulse);
    drawGlow(greenCenter, Theme::Accent, secondPulse);

    painter.setPen(Qt::NoPen);
    for (int index = 0; index < 14; ++index) {
        const qreal orbit = phase + index * (2.0 * pi / 14.0);
        const qreal x = width() * (0.5 + 0.47 * qCos(orbit));
        const qreal y = height() * (0.5 + 0.43 * qSin(orbit));
        const qreal twinkle = 0.5 + 0.5 * qSin(orbit * 2.0 + index);
        const qreal radius = 1.4 + twinkle * 2.2;
        QColor particle = index % 2 == 0 ? Theme::Blue : Theme::Accent;
        particle.setAlpha(m_animationsEnabled
                              ? qRound(45 + twinkle * 110)
                              : 72);
        painter.setBrush(particle);
        painter.drawEllipse(QPointF(x, y), radius, radius);
    }
}

void DonationPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateAnimationState();
}

void DonationPage::hideEvent(QHideEvent *event)
{
    m_glowAnimation->stop();
    QWidget::hideEvent(event);
}

void DonationPage::updateAnimationState()
{
    if (m_animationsEnabled && isVisible()) {
        if (m_glowAnimation->state() != QAbstractAnimation::Running) {
            m_glowAnimation->start();
        }
    } else {
        m_glowAnimation->stop();
        setGlowProgress(0.0);
    }
}
