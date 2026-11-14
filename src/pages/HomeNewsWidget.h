/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QWidget>

class AccentButton;
class QLabel;
class QVBoxLayout;
class LoadingIndicator;
class QNetworkAccessManager;
class QNetworkReply;

class HomeNewsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HomeNewsWidget(QWidget *parent = nullptr);

    void refresh();
    void setAnimationsEnabled(bool enabled);

private:
    struct HotItem
    {
        QString title;
        QString description;
        QString url;
        QString source;
    };

    void requestHotList();
    void handleReply(QNetworkReply *reply);
    QVector<HotItem> parseHotList(const QByteArray &data,
                                  const QString &source) const;
    void finishLoading();
    void updatePage();
    void updateCount();
    void openUrl(const QUrl &url);
    void openBingSearch(const QString &query);
    void showDisclaimer();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QStringList m_endpoints;
    int m_endpointIndex = 0;
    int m_generation = 0;
    int m_page = 0;
    bool m_loading = false;
    QVector<HotItem> m_items;

    QWidget *m_itemsContainer = nullptr;
    QVBoxLayout *m_itemsLayout = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QStringList m_lastErrors;
    QLabel *m_countLabel = nullptr;
    LoadingIndicator *m_loadingIndicator = nullptr;
    AccentButton *m_refreshButton = nullptr;
    AccentButton *m_previousButton = nullptr;
    AccentButton *m_nextButton = nullptr;
    AccentButton *m_disclaimerButton = nullptr;
    QLabel *m_pageLabel = nullptr;
    bool m_animationsEnabled = true;
};
