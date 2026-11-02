/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

class AccentButton;
class QLabel;
class QLineEdit;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;

class AccountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AccountDialog(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void rebuildList();
    void beginRename(const QString &id);
    void leaveFormEdit();
    void submitForm();
    void handleDelete(const QString &id);
    void setError(const QString &message);

    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLabel *m_formLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    AccentButton *m_submitButton = nullptr;
    AccentButton *m_cancelButton = nullptr;
    QString m_editingId;
    QString m_deleteArmedId;
    QImage m_backgroundImage;
    int m_backgroundOpacity = 35;
};
