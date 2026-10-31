/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "ModsDrawer.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "SearchField.h"
#include "Theme.h"
#include "ToggleSwitch.h"

#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPoint>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolTip>
#include <QVBoxLayout>

namespace {

QColor categoryColor(const QString &category)
{
    if (category == QStringLiteral("性能")) {
        return Theme::Accent;
    }
    if (category == QStringLiteral("光影")) {
        return Theme::Blue;
    }
    if (category == QStringLiteral("前置") || category == QStringLiteral("核心")) {
        return Theme::Purple;
    }
    if (category == QStringLiteral("视觉") || category == QStringLiteral("界面")) {
        return Theme::Purple;
    }
    return Theme::Amber;
}

class ElidedLabel : public QLabel
{
public:
    ElidedLabel(const QString &text, int pixelSize, QFont::Weight weight,
                const QColor &color, QWidget *parent = nullptr)
        : QLabel(text, parent)
    {
        setFont(Theme::font(pixelSize, weight));
        QPalette p = palette();
        p.setColor(QPalette::WindowText, color);
        setPalette(p);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setMinimumWidth(20);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text(), Qt::ElideRight, width()));
    }
};

class ModBadge : public QWidget
{
public:
    ModBadge(const QString &name, const QColor &accent, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_accent(accent)
    {
        QString displayName = name.trimmed();
        while (!displayName.isEmpty()
               && !displayName.at(0).isLetterOrNumber()) {
            displayName.remove(0, 1);
        }
        m_letter = displayName.isEmpty() ? QStringLiteral("M")
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
        painter.setPen(QPen(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 58), 1.0));
        painter.setBrush(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 24));
        painter.drawRoundedRect(box, 9, 9);
        painter.setPen(QColor(m_accent.darker(115)));
        painter.setFont(Theme::font(15, QFont::DemiBold));
        painter.drawText(box, Qt::AlignCenter, m_letter);
    }

private:
    QString m_letter;
    QColor m_accent;
};

} // namespace

ModsDrawer::ModsDrawer(const VersionInfo &version, QWidget *parent)
    : QWidget(parent)
    , m_version(version)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);

    if (parent) {
        parent->installEventFilter(this);
        setGeometry(parent->rect());
    }

    buildUi();

    m_animation = new QPropertyAnimation(this, "revealProgress", this);
    m_animation->setDuration(330);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_closing) {
            emit closed();
            deleteLater();
        }
    });

    auto *escape = new QShortcut(QKeySequence(QStringLiteral("Esc")), this);
    connect(escape, &QShortcut::activated, this, &ModsDrawer::closeDrawer);

    updateEnabledCount();
    applyFilter(QString());
    setRevealProgress(0.0);
}

void ModsDrawer::buildUi()
{
    m_panel = new QWidget(this);
    m_panel->setAttribute(Qt::WA_NoSystemBackground);
    m_panel->setAutoFillBackground(false);

    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setContentsMargins(25, 22, 25, 20);
    panelLayout->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);

    auto *headerText = new QVBoxLayout();
    headerText->setContentsMargins(0, 0, 0, 0);
    headerText->setSpacing(2);
    headerText->addWidget(Theme::label(QStringLiteral("已安装模组"), 22, QFont::DemiBold,
                                       Theme::TextPrimary, m_panel, true));
    headerText->addWidget(Theme::label(
        QStringLiteral("Minecraft %1  ·  %2 %3")
            .arg(m_version.minecraft, m_version.loader, m_version.loaderVersion),
        11, QFont::Normal, Theme::TextMuted, m_panel));
    header->addLayout(headerText, 1);

    auto *closeButton = new AccentButton(QString(), Icons::Type::Close, m_panel);
    closeButton->setVariant(AccentButton::Variant::Ghost);
    closeButton->setCompact(true);
    closeButton->setFixedSize(34, 34);
    closeButton->setAccessibleName(QStringLiteral("关闭模组详情"));
    connect(closeButton, &QAbstractButton::clicked, this, &ModsDrawer::closeDrawer);
    header->addWidget(closeButton, 0, Qt::AlignTop);
    panelLayout->addLayout(header);
    panelLayout->addSpacing(15);
    panelLayout->addWidget(Theme::divider(m_panel));
    panelLayout->addSpacing(14);

    auto *statsRow = new QHBoxLayout();
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(8);
    QColor loaderBackground = m_version.accent;
    loaderBackground.setAlpha(24);
    QColor loaderBorder = m_version.accent;
    loaderBorder.setAlpha(64);
    statsRow->addWidget(Theme::badge(QStringLiteral("%1 %2").arg(m_version.loader, m_version.loaderVersion),
                                     m_version.accent, loaderBackground, m_panel, loaderBorder));
    m_countLabel = Theme::label(QString(), 10, QFont::Normal, Theme::TextSecondary, m_panel);
    statsRow->addWidget(m_countLabel);
    statsRow->addStretch();
    panelLayout->addLayout(statsRow);
    panelLayout->addSpacing(12);

    m_search = new SearchField(QStringLiteral("搜索已安装的模组…"), m_panel);
    m_search->setFixedHeight(40);
    connect(m_search, &QLineEdit::textChanged, this, &ModsDrawer::applyFilter);
    panelLayout->addWidget(m_search);
    panelLayout->addSpacing(12);

    auto *listHeader = new QHBoxLayout();
    listHeader->setContentsMargins(2, 0, 2, 0);
    listHeader->setSpacing(8);
    listHeader->addWidget(Theme::label(QStringLiteral("模组列表"), 12, QFont::DemiBold,
                                       Theme::TextPrimary, m_panel));
    listHeader->addStretch();
    panelLayout->addLayout(listHeader);
    panelLayout->addSpacing(7);

    m_scroll = new QScrollArea(m_panel);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget();
    content->setAttribute(Qt::WA_NoSystemBackground);
    m_modsLayout = new QVBoxLayout(content);
    m_modsLayout->setContentsMargins(0, 0, 5, 0);
    m_modsLayout->setSpacing(6);

    for (int index = 0; index < m_version.mods.size(); ++index) {
        const ModInfo &mod = m_version.mods.at(index);
        auto *row = new QFrame(content);
        row->setProperty("role", QStringLiteral("modRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setFixedHeight(54);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(9, 8, 9, 8);
        rowLayout->setSpacing(10);

        const QColor accent = categoryColor(mod.category);
        rowLayout->addWidget(new ModBadge(mod.name, accent, row), 0, Qt::AlignVCenter);

        auto *nameLabel = new ElidedLabel(mod.name, 11, QFont::DemiBold,
                                          Theme::TextPrimary, row);
        rowLayout->addWidget(nameLabel, 1);

        if (!mod.version.isEmpty()) {
            rowLayout->addWidget(Theme::label(mod.version, 9, QFont::Normal,
                                              Theme::TextMuted, row));
        }

        auto *toggle = new ToggleSwitch(row);
        toggle->setChecked(mod.enabled);
        toggle->setAccessibleName(QStringLiteral("启用或停用 %1").arg(mod.name));
        connect(toggle, &QAbstractButton::toggled, this,
                [this, index](bool enabled) { setModEnabled(index, enabled); });
        rowLayout->addWidget(toggle, 0, Qt::AlignVCenter);

        m_modsLayout->addWidget(row);
        m_rowWidgets.append(row);
        m_rowSearchKeys.append(QStringLiteral("%1 %2 %3 %4")
                                   .arg(mod.name, mod.version, mod.category, mod.description)
                                   .toLower());
        m_switches.append(toggle);
    }

    m_noResultsLabel = Theme::label(
        m_version.mods.isEmpty()
            ? QStringLiteral("没有在游戏目录或版本隔离目录的 mods 文件夹中找到 JAR 文件。")
            : QStringLiteral("没有找到匹配的模组，试试其他关键词。"),
        11, QFont::Normal, Theme::TextMuted, content);
    m_noResultsLabel->setAlignment(Qt::AlignCenter);
    m_noResultsLabel->setFixedHeight(90);
    m_modsLayout->addWidget(m_noResultsLabel);
    m_modsLayout->addStretch(1);

    m_scroll->setWidget(content);
    panelLayout->addWidget(m_scroll, 1);
}

void ModsDrawer::open()
{
    if (!parentWidget()) {
        return;
    }

    m_closing = false;
    setGeometry(parentWidget()->rect());
    updatePanelGeometry();
    show();
    raise();
    setFocus(Qt::ActiveWindowFocusReason);

    m_animation->stop();
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    m_animation->setDuration(330);
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(1.0);
    m_animation->start();
}

void ModsDrawer::closeDrawer()
{
    if (m_closing) {
        return;
    }
    m_closing = true;
    m_animation->stop();
    m_animation->setEasingCurve(QEasingCurve::InCubic);
    m_animation->setDuration(230);
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(0.0);
    m_animation->start();
}

qreal ModsDrawer::revealProgress() const
{
    return m_progress;
}

void ModsDrawer::setRevealProgress(qreal progress)
{
    m_progress = qBound(0.0, progress, 1.0);
    updatePanelGeometry();
    update();
}

void ModsDrawer::updatePanelGeometry()
{
    if (!m_panel) {
        return;
    }

    const int margin = 14;
    const int panelWidth = qMin(456, qMax(300, width() - 42));
    const int panelHeight = qMax(300, height() - margin * 2);
    const int openX = width() - panelWidth - margin;
    const int x = qRound(width() - (width() - openX) * m_progress);
    m_panel->setGeometry(x, margin, panelWidth, panelHeight);
}

void ModsDrawer::updateEnabledCount()
{
    int enabled = 0;
    for (ToggleSwitch *toggle : m_switches) {
        if (toggle->isChecked()) {
            ++enabled;
        }
    }
    if (m_countLabel) {
        m_countLabel->setText(QStringLiteral("%1 个模组  ·  %2 已启用")
                                  .arg(m_version.mods.size())
                                  .arg(enabled));
    }
}

void ModsDrawer::setModEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_version.mods.size()
        || index >= m_switches.size()) {
        return;
    }

    ModInfo &mod = m_version.mods[index];
    ToggleSwitch *toggle = m_switches.at(index);
    if (!toggle) {
        return;
    }
    if (mod.enabled == enabled) {
        updateEnabledCount();
        return;
    }

    const QString action = enabled ? QStringLiteral("启用") : QStringLiteral("停用");
    const auto fail = [this, toggle, &mod, action](const QString &reason) {
        QSignalBlocker blocker(toggle);
        toggle->setChecked(mod.enabled);
        updateEnabledCount();
        const QString message =
            QStringLiteral("无法%1模组「%2」：%3").arg(action, mod.name, reason);
        QToolTip::showText(toggle->mapToGlobal(QPoint(0, toggle->height() + 4)),
                           message, toggle);
        emit notifyRequested(message);
    };

    if (mod.filePath.isEmpty() || !QFileInfo::exists(mod.filePath)) {
        fail(QStringLiteral("找不到模组文件。"));
        return;
    }

    const QString sourcePath = mod.filePath;
    QString targetPath;
    if (enabled) {
        if (!sourcePath.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            fail(QStringLiteral("文件状态与列表记录不一致。"));
            return;
        }
        targetPath = sourcePath.left(sourcePath.size() - 9);
    } else {
        if (sourcePath.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            fail(QStringLiteral("文件状态与列表记录不一致。"));
            return;
        }
        targetPath = sourcePath + QStringLiteral(".disabled");
    }

    if (QFileInfo::exists(targetPath)) {
        fail(QStringLiteral("目标文件已存在。"));
        return;
    }

    QFile file(sourcePath);
    if (!file.rename(targetPath)) {
        fail(file.errorString());
        return;
    }

    mod.enabled = enabled;
    mod.filePath = targetPath;
    updateEnabledCount();
    emit modsChanged();
    emit notifyRequested(
        QStringLiteral("已%1模组「%2」。").arg(action, mod.name));
}

void ModsDrawer::applyFilter(const QString &query)
{
    const QString normalized = query.trimmed().toLower();
    int visible = 0;
    for (int i = 0; i < m_rowWidgets.size(); ++i) {
        const bool matches = normalized.isEmpty() || m_rowSearchKeys.at(i).contains(normalized);
        m_rowWidgets.at(i)->setVisible(matches);
        if (matches) {
            ++visible;
        }
    }

    if (m_noResultsLabel) {
        if (m_rowWidgets.isEmpty()) {
            m_noResultsLabel->setText(
                QStringLiteral("没有在游戏目录或版本隔离目录的 mods 文件夹中找到 JAR 文件。"));
        } else {
            m_noResultsLabel->setText(QStringLiteral("没有找到匹配的模组，试试其他关键词。"));
        }
        m_noResultsLabel->setVisible(visible == 0);
    }
}

void ModsDrawer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(23, 36, 58, static_cast<int>(90 * m_progress)));
    painter.drawRect(rect());

    if (!m_panel || m_progress <= 0.001) {
        return;
    }

    const QRectF panelRect(m_panel->geometry());

    for (int i = 12; i >= 1; --i) {
        const qreal spread = i * 1.15;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(23, 36, 58, 5));
        painter.drawRoundedRect(panelRect.adjusted(-spread, -spread + 3,
                                                   spread, spread + 3),
                                22 + spread, 22 + spread);
    }

    QLinearGradient background(panelRect.topLeft(), panelRect.bottomRight());
    if (Theme::isDarkMode()) {
        background.setColorAt(0.0, QColor(QStringLiteral("#162130")));
        background.setColorAt(0.55, QColor(QStringLiteral("#111A25")));
        background.setColorAt(1.0, QColor(QStringLiteral("#0D141D")));
    } else {
        background.setColorAt(0.0, QColor(QStringLiteral("#FFFFFF")));
        background.setColorAt(0.55, QColor(QStringLiteral("#FBFDFF")));
        background.setColorAt(1.0, QColor(QStringLiteral("#F2F7FD")));
    }
    painter.setBrush(background);
    painter.setPen(QPen(Theme::Border, 1.0));
    painter.drawRoundedRect(panelRect.adjusted(0.5, 0.5, -0.5, -0.5), 22, 22);

    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(panelRect, 22, 22);
    painter.setClipPath(clip);

    QRadialGradient glow(QPointF(panelRect.right() - 62, panelRect.top() + 36), 260);
    glow.setColorAt(0.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                Theme::Accent.blue(), 20));
    glow.setColorAt(1.0, QColor(Theme::Accent.red(), Theme::Accent.green(),
                                Theme::Accent.blue(), 0));
    painter.fillRect(panelRect, glow);

    QLinearGradient edge(panelRect.topLeft(), panelRect.topRight());
    edge.setColorAt(0.0, QColor(255, 255, 255, 0));
    edge.setColorAt(0.25, QColor(255, 255, 255, 235));
    edge.setColorAt(0.75, QColor(255, 255, 255, 160));
    edge.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setPen(QPen(QBrush(edge), 1.0));
    painter.drawLine(QPointF(panelRect.left() + 24, panelRect.top() + 1),
                     QPointF(panelRect.right() - 24, panelRect.top() + 1));
    painter.restore();
}

void ModsDrawer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && (!m_panel || !m_panel->geometry().contains(event->position().toPoint()))) {
        closeDrawer();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ModsDrawer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePanelGeometry();
}

bool ModsDrawer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        updatePanelGeometry();
    }
    return QWidget::eventFilter(watched, event);
}
