/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "HomeStatusWidget.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "LoadingIndicator.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kStatusTimeoutMs = 20000;

// BMCLAPI is used for Mojang so the check also works on networks where the
// official endpoints are slow or blocked. Modrinth/GitHub stay official.
const QUrl kMojangManifestUrl(
    QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json"));
const QUrl kModrinthUrl(
    QStringLiteral("https://api.modrinth.com/v2/tag/loader"));
const QUrl kGithubUrl(
    QStringLiteral("https://api.github.com/rate_limit"));
const QUrl kHotListUrl(
    QStringLiteral("https://api.zhihu.com/topstory/hot-list?limit=1"));

} // namespace

HomeStatusWidget::HomeStatusWidget(QWidget *parent)
    : QWidget(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    m_animationsEnabled = Theme::animationsEnabled();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);
    header->addWidget(Theme::label(QStringLiteral("服务与版本状态"), 19,
                                   QFont::DemiBold, Theme::TextPrimary, this));
    header->addStretch();
    root->addLayout(header);
    root->addSpacing(10);
    root->addWidget(Theme::divider(this));
    root->addSpacing(8);

    const struct
    {
        QString name;
        Icons::Type icon;
    } services[] = {
        {QStringLiteral("Mojang 服务器"), Icons::Type::Cube},
        {QStringLiteral("Modrinth 服务器"), Icons::Type::Package},
        {QStringLiteral("GitHub 服务器"), Icons::Type::External},
        {QStringLiteral("热搜 API"), Icons::Type::Grid}
    };

    for (int i = 0; i < 4; ++i) {
        auto *row = new QWidget(this);
        row->setFixedHeight(38);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(9);

        auto *icon = new QLabel(row);
        icon->setPixmap(Icons::icon(services[i].icon, Theme::Blue, 17)
                            .pixmap(17, 17));
        icon->setFixedSize(19, 19);
        icon->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(icon);
        rowLayout->addWidget(Theme::label(services[i].name, 11, QFont::Normal,
                                          Theme::TextSecondary, row));
        rowLayout->addStretch();

        auto *loading = new LoadingIndicator(row, Theme::Amber);
        loading->setFixedSize(14, 14);
        loading->setAnimationsEnabled(m_animationsEnabled);
        loading->hide();
        rowLayout->addWidget(loading, 0, Qt::AlignVCenter);

        auto *dot = new QLabel(QStringLiteral("●"), row);
        dot->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                               .arg(Theme::TextMuted.name()));
        dot->setFixedWidth(12);
        rowLayout->addWidget(dot, 0, Qt::AlignVCenter);

        auto *state = Theme::label(QStringLiteral("未检测"), 10, QFont::Normal,
                                   Theme::TextMuted, row);
        state->setMinimumWidth(48);
        state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(state, 0, Qt::AlignVCenter);

        m_rows.append(StatusRow{dot, state, loading});
        root->addWidget(row);
        if (i < 3) {
            root->addWidget(Theme::divider(this));
        }
    }

    root->addSpacing(14);
    root->addWidget(Theme::divider(this));
    root->addSpacing(12);

    auto *versionHeader = new QHBoxLayout();
    versionHeader->setContentsMargins(0, 0, 0, 0);
    versionHeader->setSpacing(10);
    versionHeader->addWidget(Theme::label(QStringLiteral("版本信息"), 13,
                                          QFont::DemiBold, Theme::TextPrimary,
                                          this));
    versionHeader->addStretch();
    m_versionStatus = Theme::label(QStringLiteral("正在获取…"), 9,
                                   QFont::Normal, Theme::TextMuted, this);
    versionHeader->addWidget(m_versionStatus);
    root->addLayout(versionHeader);
    root->addSpacing(7);

    const auto addVersionRow = [this, root](const QString &name,
                                             QLabel **value) {
        auto *row = new QWidget(this);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 2, 0, 2);
        layout->setSpacing(10);
        layout->addWidget(Theme::label(name, 10, QFont::Normal,
                                       Theme::TextMuted, row));
        layout->addStretch();
        *value = Theme::label(QStringLiteral("获取中…"), 11, QFont::DemiBold,
                              Theme::TextSecondary, row);
        (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(*value);
        root->addWidget(row);
    };
    addVersionRow(QStringLiteral("最新正式版"), &m_releaseValue);
    addVersionRow(QStringLiteral("最新快照"), &m_snapshotValue);

    auto *wikiRow = new QWidget(this);
    auto *wikiLayout = new QHBoxLayout(wikiRow);
    wikiLayout->setContentsMargins(0, 2, 0, 2);
    wikiLayout->setSpacing(10);
    wikiLayout->addWidget(Theme::label(QStringLiteral("官方 Wiki"), 10,
                                       QFont::Normal, Theme::TextMuted, wikiRow));
    wikiLayout->addStretch();
    m_wikiLabel = new QLabel(
        QStringLiteral("<a style=\"color:%1; text-decoration:none;\" "
                       "href=\"https://zh.minecraft.wiki/\">zh.minecraft.wiki</a>")
            .arg(Theme::Accent.name()),
        wikiRow);
    m_wikiLabel->setTextFormat(Qt::RichText);
    m_wikiLabel->setOpenExternalLinks(true);
    m_wikiLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; color: %1; }")
        .arg(Theme::Accent.name()));
    wikiLayout->addWidget(m_wikiLabel);
    root->addWidget(wikiRow);

    m_rowErrors = QStringList{QString(), QString(), QString(), QString()};
    m_errorLabel = Theme::label(QString(), 9, QFont::Normal,
                                Theme::Danger, this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_errorLabel->setStyleSheet(QStringLiteral(
        "background: rgba(255, 107, 120, 14); border-radius: 7px;"
        "padding: 7px 9px; color: %1;").arg(Theme::Danger.name()));
    m_errorLabel->hide();
    root->addSpacing(12);
    root->addWidget(m_errorLabel);

    root->addStretch(1);
    root->addSpacing(12);
    m_refreshButton = new AccentButton(QStringLiteral("刷新状态"),
                                       Icons::Type::Refresh, this);
    m_refreshButton->setVariant(AccentButton::Variant::Secondary);
    m_refreshButton->setMinimumHeight(38);
    m_refreshButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_refreshButton, &QAbstractButton::clicked,
            this, &HomeStatusWidget::refresh);
    root->addWidget(m_refreshButton);
}

void HomeStatusWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_started) {
        m_started = true;
        // Start after the event loop is running. Requests created while the
        // main window is still being built can otherwise hit their 5s timeout
        // before the network layer ever gets a chance to run.
        QTimer::singleShot(0, this, &HomeStatusWidget::refresh);
    }
}

void HomeStatusWidget::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    for (StatusRow &row : m_rows) {
        if (row.loading) {
            row.loading->setAnimationsEnabled(enabled);
        }
    }
}

void HomeStatusWidget::refresh()
{
    ++m_generation;
    if (m_releaseValue) {
        m_releaseValue->setText(QStringLiteral("获取中…"));
    }
    if (m_snapshotValue) {
        m_snapshotValue->setText(QStringLiteral("获取中…"));
    }
    if (m_versionStatus) {
        m_versionStatus->setText(QStringLiteral("正在获取…"));
    }
    m_rowErrors = QStringList{QString(), QString(), QString(), QString()};
    m_versionError.clear();
    if (m_errorLabel) {
        m_errorLabel->clear();
        m_errorLabel->hide();
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        setRowState(i, QStringLiteral("检测中…"), Theme::Amber, true);
    }
    if (m_refreshButton) {
        m_refreshButton->setEnabled(false);
    }

    startCheck(0, kMojangManifestUrl, true);
    startCheck(1, kModrinthUrl, false);
    startCheck(2, kGithubUrl, false);
    startCheck(3, kHotListUrl, false);
}

void HomeStatusWidget::startCheck(int row, const QUrl &url, bool versionManifest)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("MysticTurtle/3.0"));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = m_network->get(request);
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->setInterval(kStatusTimeoutMs);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("timedOut", true);
        reply->abort();
    });
    timeout->start();

    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, timeout, generation, row, versionManifest, url]() {
        timeout->stop();
        if (generation != m_generation) {
            reply->deleteLater();
            return;
        }

        const bool timedOut = reply->property("timedOut").toBool();
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool networkOk = !timedOut
            && (reply->error() == QNetworkReply::NoError
                || (httpStatus >= 200 && httpStatus < 500));
        bool ok = networkOk;
        QByteArray data;
        if (networkOk) {
            data = reply->readAll();
        }
        QString failureDetail;
        if (!networkOk) {
            if (timedOut) {
                failureDetail = QStringLiteral("20 秒内无响应");
            } else if (httpStatus > 0) {
                failureDetail = QStringLiteral("HTTP %1").arg(httpStatus);
            } else {
                failureDetail = reply->errorString();
                if (failureDetail.isEmpty()) {
                    failureDetail = QStringLiteral("网络请求失败");
                }
            }
        }
        if (ok && versionManifest) {
            ok = parseVersionManifest(data);
            if (!ok) {
                failureDetail = QStringLiteral(
                    "版本清单返回成功，但 latest.release/snapshot 缺失或 JSON 无效");
            }
        }

        if (ok) {
            if (row >= 0 && row < m_rowErrors.size()) {
                m_rowErrors[row].clear();
            }
            if (versionManifest) {
                m_versionError.clear();
            }
        } else {
            if (row >= 0 && row < m_rowErrors.size()) {
                m_rowErrors[row] = QStringLiteral("%1 → %2")
                                       .arg(url.toString(), failureDetail);
            }
            if (versionManifest) {
                m_versionError = failureDetail;
                if (m_releaseValue) {
                    m_releaseValue->setText(QStringLiteral("获取失败"));
                }
                if (m_snapshotValue) {
                    m_snapshotValue->setText(QStringLiteral("获取失败"));
                }
                if (m_versionStatus) {
                    m_versionStatus->setText(QStringLiteral("获取失败"));
                }
            }
        }

        if (versionManifest && ok && m_versionStatus) {
            m_versionStatus->setText(QStringLiteral("已同步"));
        }

        setRowState(row,
                    ok ? QStringLiteral("正常")
                       : timedOut ? QStringLiteral("超时")
                                  : QStringLiteral("失败"),
                    ok ? Theme::Accent : Theme::Danger, false);
        updateErrorSummary();
        if (m_refreshButton) {
            m_refreshButton->setEnabled(true);
        }
        reply->deleteLater();
    });
}

void HomeStatusWidget::setRowState(int row, const QString &text,
                                   const QColor &color, bool loading)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    StatusRow &status = m_rows[row];
    if (status.loading) {
        status.loading->setVisible(loading);
    }
    if (status.dot) {
        status.dot->setVisible(!loading);
        status.dot->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(color.name()));
    }
    if (status.state) {
        status.state->setText(text);
        status.state->setStyleSheet(
            QStringLiteral("color: %1; background: transparent;")
                .arg(color.name()));
    }
}

void HomeStatusWidget::updateErrorSummary()
{
    const QString names[] = {
        QStringLiteral("Mojang 服务器"),
        QStringLiteral("Modrinth 服务器"),
        QStringLiteral("GitHub 服务器"),
        QStringLiteral("热搜 API")
    };
    QStringList lines;
    for (int i = 0; i < m_rows.size() && i < 4; ++i) {
        if (i < m_rowErrors.size() && !m_rowErrors.at(i).isEmpty()) {
            lines.append(QStringLiteral("%1：%2").arg(names[i], m_rowErrors.at(i)));
        }
        if (m_rows.at(i).state) {
            m_rows.at(i).state->setToolTip(
                i < m_rowErrors.size() ? m_rowErrors.at(i) : QString());
        }
    }
    if (!m_versionError.isEmpty()) {
        lines.append(QStringLiteral("版本信息：%1").arg(m_versionError));
    }
    if (m_errorLabel) {
        m_errorLabel->setText(lines.join(QLatin1Char('\n')));
        m_errorLabel->setVisible(!lines.isEmpty());
    }
}

bool HomeStatusWidget::parseVersionManifest(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject latest =
        document.object().value(QStringLiteral("latest")).toObject();
    const QString release =
        latest.value(QStringLiteral("release")).toString().trimmed();
    const QString snapshot =
        latest.value(QStringLiteral("snapshot")).toString().trimmed();
    if (release.isEmpty() || snapshot.isEmpty()) {
        return false;
    }
    if (m_releaseValue) {
        m_releaseValue->setText(release);
    }
    if (m_snapshotValue) {
        m_snapshotValue->setText(snapshot);
    }
    return true;
}
