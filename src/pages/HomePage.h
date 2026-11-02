/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include "SampleData.h"

#include <QString>
#include <QVector>
#include <QWidget>

class AccentButton;
class QGridLayout;
class QLabel;
class LoadingIndicator;
class VersionCard;

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

    QString gameDirectory() const;
    VersionInfo versionAt(int versionIndex) const;
    VersionInfo selectedVersion() const;
    int selectedVersionIndex() const;

public slots:
    void reloadLocalVersions();
    void openAccountDialog();
    void setGameDirectory(const QString &directory);

signals:
    void detailsRequested(int versionIndex);
    void notifyRequested(const QString &message);
    void gameDirectoryChanged(const QString &directory);
    void launchRequested();

private:
    void chooseGameDirectory();
    void rebuildVersionCards();
    void updatePathDisplay();
    void updateAccountUi();
    void selectVersion(int index);
    void deleteVersion(int index);
    void updateSelectedVersionUi();

    QString m_gameDirectory;
    QVector<VersionInfo> m_versions;
    QVector<VersionCard *> m_versionCards;
    int m_selectedVersionIndex = -1;

    QGridLayout *m_versionGrid = nullptr;
    QLabel *m_versionCountBadge = nullptr;
    QLabel *m_scanStatusLabel = nullptr;
    LoadingIndicator *m_scanLoadingIndicator = nullptr;
    QLabel *m_emptyVersionsLabel = nullptr;
    QLabel *m_gamePathLabel = nullptr;
    QLabel *m_accountNameLabel = nullptr;
    QLabel *m_accountTypeBadge = nullptr;
    AccentButton *m_accountButton = nullptr;
    AccentButton *m_launchButton = nullptr;
    quint64 m_scanGeneration = 0;
};
