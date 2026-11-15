/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 */

#include "DialogBackground.h"

#include "BackgroundStore.h"
#include "Theme.h"

#include <QImage>
#include <QPainter>
#include <QWidget>

namespace DialogBackground {

void paint(QWidget *widget, QPainter &painter)
{
    if (!widget) {
        return;
    }
    const QRect target = widget->rect();
    painter.fillRect(target, Theme::isDarkMode() ? Theme::BackgroundDeep
                                                 : QColor(QStringLiteral("#EEF5FC")));

    const QImage image = BackgroundStore::instance().image();
    const int opacity = BackgroundStore::instance().opacity();
    if (image.isNull() || opacity <= 0 || target.width() <= 0 || target.height() <= 0) {
        return;
    }

    const qreal targetAspect = static_cast<qreal>(target.width()) / target.height();
    const qreal imageAspect = static_cast<qreal>(image.width()) / image.height();
    QRectF sourceRect(image.rect());
    if (imageAspect > targetAspect) {
        const qreal croppedWidth = image.height() * targetAspect;
        sourceRect.setLeft((image.width() - croppedWidth) / 2.0);
        sourceRect.setWidth(croppedWidth);
    } else {
        const qreal croppedHeight = image.width() / targetAspect;
        sourceRect.setTop((image.height() - croppedHeight) / 2.0);
        sourceRect.setHeight(croppedHeight);
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(opacity / 100.0);
    painter.drawImage(target, image, sourceRect);
    painter.setOpacity(1.0);
}

} // namespace DialogBackground
