/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 */

#pragma once

#include <QDialog>
#include <QStringList>

class AccentButton;
class CardWidget;
class ModernLineEdit;
class QListWidget;

class ModernSelectionDialog : public QDialog
{
public:
    static QString getItem(QWidget *parent,
                           const QString &title,
                           const QString &description,
                           const QStringList &items,
                           int currentIndex = 0,
                           bool *accepted = nullptr);

private:
    ModernSelectionDialog(QWidget *parent,
                          const QString &title,
                          const QString &description,
                          const QStringList &items,
                          int currentIndex);

    void accept() override;
    void selectFirstVisible();
    void paintEvent(QPaintEvent *event) override;

    QListWidget *m_list = nullptr;
    ModernLineEdit *m_search = nullptr;
    QString m_selected;
};
