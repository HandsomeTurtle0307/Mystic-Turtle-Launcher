/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QColor>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QWidget>

class AccentButton;
class QLabel;
class QShowEvent;
class LoadingIndicator;
class QNetworkAccessManager;
class QNetworkReply;

class HomeStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeStatusWidget(QWidget *parent = nullptr);

    void refresh();
    void setAnimationsEnabled(bool enabled);

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct StatusRow
    {
        QLabel *dot = nullptr;
        QLabel *state = nullptr;
        LoadingIndicator *loading = nullptr;
    };

    void startCheck(int row, const QUrl &url, bool versionManifest);
    void setRowState(int row, const QString &text, const QColor &color,
                     bool loading);
    void updateErrorSummary();
    bool parseVersionManifest(const QByteArray &data);

    QNetworkAccessManager *m_network = nullptr;
    int m_generation = 0;
    bool m_animationsEnabled = true;
    bool m_started = false;
    QVector<StatusRow> m_rows;
    QStringList m_rowErrors;
    QString m_versionError;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_releaseValue = nullptr;
    QLabel *m_snapshotValue = nullptr;
    QLabel *m_versionStatus = nullptr;
    QLabel *m_wikiLabel = nullptr;
    AccentButton *m_refreshButton = nullptr;
};
