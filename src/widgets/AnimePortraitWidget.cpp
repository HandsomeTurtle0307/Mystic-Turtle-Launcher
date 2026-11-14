/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "AnimePortraitWidget.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QTransform>

#include <cmath>

namespace {

QImage loadPortrait(const QString &name)
{
    QImage image(QStringLiteral(":/assets/%1").arg(name));
    if (image.isNull()) {
        image = QImage(QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("assets/%1").arg(name)));
    }
    return image;
}

qreal smoothStep(qreal edge0, qreal edge1, qreal value)
{
    if (qFuzzyCompare(edge0, edge1)) {
        return value < edge0 ? 0.0 : 1.0;
    }
    const qreal t = qBound(0.0, (value - edge0) / (edge1 - edge0), 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace

AnimePortraitWidget::AnimePortraitWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    // Purely decorative: never intercept a click from the launcher UI.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setMouseTracking(true);

    loadImage();

    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &AnimePortraitWidget::tick);
    m_clock.start();
}

void AnimePortraitWidget::loadImage()
{
    m_fullImage = loadPortrait(m_resourceName);
    if (m_fullImage.isNull()
        && m_resourceName != QStringLiteral("portrait-maid.png")) {
        m_fullImage = loadPortrait(QStringLiteral("portrait-maid.png"));
    }
    buildHairLayer();
    loadEyeInfo();
}

void AnimePortraitWidget::buildHairLayer()
{
    m_hairImage = QImage();
    if (m_fullImage.isNull()) {
        return;
    }

    const QImage source = m_fullImage.convertToFormat(QImage::Format_ARGB32);
    QImage hair(source.size(), QImage::Format_ARGB32);
    hair.fill(Qt::transparent);

    const int width = source.width();
    const int height = source.height();
    for (int y = 0; y < height; ++y) {
        const qreal ny = y / qMax(1.0, height - 1.0);
        const qreal yFade = smoothStep(0.02, 0.13, ny)
            * smoothStep(0.78, 0.58, ny);
        if (yFade <= 0.001) {
            continue;
        }
        const QRgb *srcLine = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        QRgb *dstLine = reinterpret_cast<QRgb *>(hair.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const qreal nx = x / qMax(1.0, width - 1.0);
            const qreal left = smoothStep(0.44, 0.20, nx);
            const qreal right = smoothStep(0.56, 0.80, nx);
            const qreal weight = qMax(left, right) * yFade * 0.9;
            if (weight <= 0.001) {
                continue;
            }
            const QRgb pixel = srcLine[x];
            const int alpha = qRound(qAlpha(pixel) * weight);
            if (alpha <= 0) {
                continue;
            }
            dstLine[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), alpha);
        }
    }
    m_hairImage = hair;
}

void AnimePortraitWidget::loadEyeInfo()
{
    m_eyeInfo = EyeInfo();
    QFile file(QStringLiteral(":/assets/portrait-eyes.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        file.setFileName(QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("assets/portrait-eyes.json")));
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }
    const QJsonValue value = document.object().value(m_resourceName);
    if (!value.isObject()) {
        return;
    }
    const QJsonObject object = value.toObject();
    m_eyeInfo.enabled = object.value(QStringLiteral("eyes")).toBool(false);
    const QJsonArray left = object.value(QStringLiteral("left")).toArray();
    const QJsonArray right = object.value(QStringLiteral("right")).toArray();
    const QJsonArray radius = object.value(QStringLiteral("radius")).toArray();
    if (!m_eyeInfo.enabled || left.size() < 2 || right.size() < 2) {
        m_eyeInfo.enabled = false;
        return;
    }
    m_eyeInfo.left = QPointF(left.at(0).toDouble(), left.at(1).toDouble());
    m_eyeInfo.right = QPointF(right.at(0).toDouble(), right.at(1).toDouble());
    if (radius.size() >= 2) {
        m_eyeInfo.radius = QPointF(radius.at(0).toDouble(),
                                   radius.at(1).toDouble());
    }
}

void AnimePortraitWidget::setPortrait(const QString &resourceName)
{
    if (resourceName.isEmpty() || m_resourceName == resourceName) {
        return;
    }
    m_resourceName = resourceName;
    loadImage();
    update();
}

void AnimePortraitWidget::setMotionParameters(int breathPercent,
                                              int swayPercent,
                                              int eyePercent)
{
    m_breathAmplitude = qBound(0, breathPercent, 100) / 100.0;
    m_swayAmplitude = qBound(0, swayPercent, 100) / 100.0;
    m_eyeAmplitude = qBound(0, eyePercent, 100) / 100.0;
    if (!m_animationsEnabled) {
        update();
    }
}

void AnimePortraitWidget::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    if (active) {
        m_clock.restart();
        updateCursorTarget();
        if (m_animationsEnabled) {
            m_timer->start();
        }
    } else {
        m_timer->stop();
    }
    update();
}

bool AnimePortraitWidget::isActive() const
{
    return m_active;
}

void AnimePortraitWidget::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (!m_active) {
        m_timer->stop();
    } else if (enabled) {
        m_clock.restart();
        m_timer->start();
    } else {
        m_timer->stop();
        m_time = 0.0;
        m_look = QPointF();
        update();
    }
}

void AnimePortraitWidget::updateCursorTarget()
{
    if (!isVisible()) {
        m_targetLook = QPointF();
        return;
    }
    const QPoint cursor = mapFromGlobal(QCursor::pos());
    const QPointF center(width() / 2.0, height() / 2.0);
    m_targetLook.setX(qBound(-1.0, (cursor.x() - center.x())
                                        / qMax(1.0, width() / 2.0), 1.0));
    m_targetLook.setY(qBound(-1.0, (cursor.y() - center.y())
                                        / qMax(1.0, height() / 2.0), 1.0));
}

void AnimePortraitWidget::tick()
{
    const qreal dt = qBound(0.001, m_clock.restart() / 1000.0, 0.05);
    m_time += dt;
    updateCursorTarget();

    const qreal follow = 1.0 - std::pow(0.0025, dt);
    m_look += (m_targetLook - m_look) * follow;
    update();
}

void AnimePortraitWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF center(width() / 2.0, height() / 2.0);
    m_targetLook.setX(qBound(-1.0, (event->position().x() - center.x())
                                        / qMax(1.0, width() / 2.0), 1.0));
    m_targetLook.setY(qBound(-1.0, (event->position().y() - center.y())
                                        / qMax(1.0, height() / 2.0), 1.0));
    QWidget::mouseMoveEvent(event);
}

void AnimePortraitWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (m_fullImage.isNull()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal sourceWidth = m_fullImage.width();
    const qreal sourceHeight = m_fullImage.height();
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0) {
        return;
    }

    const qreal margin = 10.0;
    const qreal availableWidth = qMax(1.0, width() - margin * 2.0);
    const qreal availableHeight = qMax(1.0, height() - margin * 2.0);
    const qreal scale = qMin(availableWidth / sourceWidth,
                             availableHeight / sourceHeight);
    const qreal drawWidth = sourceWidth * scale;
    const qreal drawHeight = sourceHeight * scale;
    const QRectF target((width() - drawWidth) / 2.0,
                        height() - drawHeight - margin,
                        drawWidth, drawHeight);

    const qreal breathWave = std::sin(m_time * 1.6);
    const qreal swayWave = std::sin(m_time * 0.82);
    const qreal scaleY = 1.0 + 0.015 * m_breathAmplitude * breathWave;
    const qreal scaleX = 1.0 - 0.005 * m_breathAmplitude * breathWave;
    const qreal tilt = 0.4 * m_swayAmplitude * swayWave + m_look.x() * 0.25;
    const qreal shear = 0.004 * m_swayAmplitude * swayWave;
    const qreal followX = m_look.x() * 5.0;
    const qreal followY = m_look.y() * 3.0;

    QTransform base;
    const QPointF pivot(target.center().x(), target.bottom() - 12.0);
    base.translate(pivot.x() + followX, pivot.y() + followY);
    base.rotate(tilt);
    base.shear(shear, 0.0);
    base.scale(scaleX, scaleY);
    base.translate(-pivot.x(), -pivot.y());

    painter.save();
    painter.setTransform(base);
    painter.drawImage(target, m_fullImage);

    if (!m_hairImage.isNull() && m_swayAmplitude > 0.001) {
        // The hair patch is soft-masked (no hard clip edges). It swings with a
        // phase lag, so it trails the body instead of moving as one rigid card.
        const qreal hairWave = std::sin(m_time * 1.6 - 0.8);
        const qreal hairSide = std::sin(m_time * 2.25 - 0.4);
        const qreal hairX = m_swayAmplitude * 4.5 * hairSide;
        const qreal hairY = m_swayAmplitude * 1.8 * hairWave;
        QTransform hairTransform = base;
        hairTransform.translate(hairX, hairY);
        hairTransform.shear(m_swayAmplitude * 0.002 * hairWave, 0.0);
        painter.setTransform(hairTransform);
        painter.drawImage(target, m_hairImage);
    }
    painter.restore();

    if (m_eyeInfo.enabled && m_eyeAmplitude > 0.001) {
        const QPointF left(target.left() + m_eyeInfo.left.x() * target.width(),
                           target.top() + m_eyeInfo.left.y() * target.height());
        const QPointF right(target.left() + m_eyeInfo.right.x() * target.width(),
                            target.top() + m_eyeInfo.right.y() * target.height());
        const qreal rx = qMax(2.0, m_eyeInfo.radius.x() * target.width());
        const qreal ry = qMax(1.5, m_eyeInfo.radius.y() * target.height());

        QPainterPath eyeClip;
        eyeClip.addEllipse(left, rx, ry);
        eyeClip.addEllipse(right, rx, ry);

        const QPointF offset(m_look.x() * 5.0 * m_eyeAmplitude,
                             m_look.y() * 3.0 * m_eyeAmplitude);
        painter.save();
        painter.setTransform(base);
        painter.setClipPath(eyeClip);
        painter.setPen(Qt::NoPen);
        const auto paintGlint = [&painter, ry](const QPointF &center) {
            QRadialGradient glint(center, qMax(3.5, ry * 2.4));
            glint.setColorAt(0.0, QColor(255, 255, 255, 120));
            glint.setColorAt(0.42, QColor(255, 255, 255, 48));
            glint.setColorAt(1.0, QColor(255, 255, 255, 0));
            painter.setBrush(glint);
            painter.drawEllipse(center, qMax(2.3, ry * 1.0),
                                qMax(1.7, ry * 0.62));
        };
        paintGlint(left + offset);
        paintGlint(right + offset);
        painter.restore();
    }
}
