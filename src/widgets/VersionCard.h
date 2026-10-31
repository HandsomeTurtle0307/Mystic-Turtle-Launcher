/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "CardWidget.h"
#include "SampleData.h"

class QLabel;
class QMouseEvent;
class QResizeEvent;

class VersionCard : public CardWidget
{
    Q_OBJECT

public:
    explicit VersionCard(const VersionInfo &version, int index, QWidget *parent = nullptr);
    void setSelected(bool selected);
    bool isSelected() const;

signals:
    void detailsRequested(int index);
    void deleteRequested(int index);
    void selected(int index);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int m_index;
    bool m_selected = false;
    QColor m_accent;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QString m_fullVersionText;
    void updateVersionElide();
};
