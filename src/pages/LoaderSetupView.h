/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "LoaderDownloader.h"

#include <QVector>
#include <QWidget>

#include <functional>

class AccentButton;
class LoaderRow;
class QJsonDocument;
class QLabel;
class QLineEdit;
class LoadingIndicator;
class QNetworkAccessManager;

class LoaderSetupView : public QWidget
{
    Q_OBJECT

public:
    explicit LoaderSetupView(const QString &gameVersion, const QString &gameDirectory,
                             QWidget *parent = nullptr);

    QString gameVersion() const;
    QString gameDirectory() const;
    QString versionName() const;
    void setGameDirectory(const QString &directory);
    LoaderSelection selection() const;

signals:
    void backRequested();
    void startRequested();
    void notifyRequested(const QString &message);

private:
    struct LoaderOption {
        QString version;
        QString url;
        QString sha1;
        QString filename;
        QString label;
    };

    struct FabricApiOption {
        QString version;
        QString url;
        QString sha1;
        QString filename;
    };

    void buildUi();
    void refreshCatalogs();
     void fetchCatalogs();
    void requestJson(const QString &url,
                     const std::function<void(const QJsonDocument &, const QString &)> &handler);
    void updateRowStates();
    void maybeShowCatalogErrors();
    void updateStartButton();
    void chooseLoader(const QString &loader);
    void chooseFabricApi();
    const QVector<LoaderOption> &optionsFor(const QString &loader) const;

    QString m_gameVersion;
    QString m_gameDirectory;
    QNetworkAccessManager *m_network = nullptr;

    QVector<LoaderOption> m_forgeOptions;
    QVector<LoaderOption> m_neoForgeOptions;
    QVector<LoaderOption> m_fabricOptions;
    QVector<LoaderOption> m_optiFineOptions;
    QVector<FabricApiOption> m_fabricApiOptions;

    bool m_forgeLoaded = false;
    bool m_neoForgeLoaded = false;
    bool m_fabricLoaded = false;
    bool m_optiFineLoaded = false;
    bool m_fabricApiLoaded = false;
    bool m_catalogErrorReported = false;
    bool m_refreshing = false;
    QString m_forgeError;
    QString m_neoForgeError;
    QString m_fabricError;
    QString m_optiFineError;
    QString m_fabricApiError;

    QString m_selectedLoader;
    LoaderOption m_selectedLoaderOption;
    FabricApiOption m_selectedFabricApi;

    LoaderRow *m_forgeRow = nullptr;
    LoaderRow *m_neoForgeRow = nullptr;
    LoaderRow *m_fabricRow = nullptr;
    LoaderRow *m_optiFineRow = nullptr;
    LoaderRow *m_fabricApiRow = nullptr;
    AccentButton *m_startButton = nullptr;
    QLineEdit *m_versionNameEdit = nullptr;
    AccentButton *m_refreshButton = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
};
