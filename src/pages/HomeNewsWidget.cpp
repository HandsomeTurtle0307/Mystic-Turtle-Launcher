/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "HomeNewsWidget.h"

#include "AccentButton.h"
#include "IconFactory.h"
#include "LoadingIndicator.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLayoutItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#include <functional>
#include <utility>

namespace {

constexpr int kItemsPerPage = 5;
constexpr int kMaximumItems = 20;
constexpr int kRequestTimeoutMs = 10000;

const QColor kTextMuted(0x70, 0x80, 0x8B);
const QColor kTextSecondary(0xA4, 0xB1, 0xB9);
const QColor kAccent(0x6E, 0xE7, 0xB7);

QString sourceNameForEndpoint(const QString &endpoint)
{
    if (endpoint.contains(QStringLiteral("zhihu"))) {
        return QStringLiteral("知乎热榜");
    }
    if (endpoint.contains(QStringLiteral("baidu"))) {
        return QStringLiteral("百度热搜");
    }
    return QStringLiteral("聚合热榜");
}

class HotItemCard : public QPushButton
{
public:
    HotItemCard(const QString &title, const QString &description,
                const QString &source, QWidget *parent)
        : QPushButton(parent)
    {
        setFlat(true);
        setText(QString());
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setMinimumHeight(90);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(title);
        setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(255,255,255,7);"
            "border: 1px solid rgba(255,255,255,12); border-radius: 11px;"
            "text-align: left; padding: 0; }"
            "QPushButton:hover { background: rgba(110,231,183,28);"
            "border-color: rgba(110,231,183,92); }"
            "QPushButton:pressed { background: rgba(110,231,183,45); }"));

        const QString displayTitle = title.size() > 44
            ? title.left(43) + QStringLiteral("…")
            : title;
        const QString fallbackDescription = QStringLiteral("点击查看详情");
        QString displayDescription = description.trimmed().isEmpty()
            ? fallbackDescription
            : description.trimmed();
        if (displayDescription.size() > 110) {
            displayDescription = displayDescription.left(109) + QStringLiteral("…");
        }

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 9, 14, 9);
        layout->setSpacing(2);

        auto *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(8);
        auto *titleLabel = Theme::label(displayTitle, 12, QFont::DemiBold,
                                        Theme::TextPrimary, this);
        titleLabel->setWordWrap(true);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleRow->addWidget(titleLabel, 1);
        auto *sourceLabel = Theme::label(source, 8, QFont::Bold, kAccent, this);
        sourceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleRow->addWidget(sourceLabel, 0, Qt::AlignTop);
        layout->addLayout(titleRow);

        auto *descriptionLabel = Theme::label(displayDescription, 9, QFont::Normal,
                                              kTextMuted, this);
        descriptionLabel->setWordWrap(true);
        descriptionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(descriptionLabel);
    }
};

} // namespace

HomeNewsWidget::HomeNewsWidget(QWidget *parent)
    : QWidget(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    m_animationsEnabled = Theme::animationsEnabled();

    // Open hot-list aggregators. No API key or web scraping is involved.
    // Direct public hot-list endpoints. These return plain JSON and avoid the
    // dead/Bing-scraping aggregators and their anti-bot gates.
    m_endpoints = QStringList{
        QStringLiteral("https://api.zhihu.com/topstory/hot-list?limit=20"),
        QStringLiteral("https://top.baidu.com/api/board?platform=wise&tab=realtime"),
        QStringLiteral("https://api.vvhan.com/api/hotlist?type=zhihu"),
        QStringLiteral("https://api.vvhan.com/api/hotlist?type=baidu"),
        QStringLiteral("https://api.guole.fun/hotlist?type=zhihu"),
        QStringLiteral("https://api.guole.fun/hotlist?type=baidu")
    };

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);
    header->addWidget(Theme::label(QStringLiteral("热搜"), 19, QFont::DemiBold,
                                   Theme::TextPrimary, this));
    header->addStretch();
    m_countLabel = Theme::label(QStringLiteral("共 0 条"), 10, QFont::Normal,
                                kTextMuted, this);
    header->addWidget(m_countLabel, 0, Qt::AlignVCenter);
    m_sourceLabel = Theme::label(QString(), 9, QFont::Normal,
                                  kTextMuted, this);
    header->addWidget(m_sourceLabel, 0, Qt::AlignVCenter);
    m_loadingIndicator = new LoadingIndicator(this, kAccent);
    m_loadingIndicator->setFixedSize(15, 15);
    m_loadingIndicator->hide();
    m_statusLabel = Theme::label(QString(), 9, QFont::Normal,
                                 kTextMuted, this);
    header->addWidget(m_loadingIndicator, 0, Qt::AlignVCenter);
    header->addWidget(m_statusLabel, 0, Qt::AlignVCenter);
    m_refreshButton = new AccentButton(QStringLiteral("刷新"),
                                       Icons::Type::Refresh, this);
    m_refreshButton->setVariant(AccentButton::Variant::Ghost);
    m_refreshButton->setCompact(true);
    connect(m_refreshButton, &QAbstractButton::clicked,
            this, &HomeNewsWidget::refresh);
    header->addWidget(m_refreshButton, 0, Qt::AlignVCenter);
    root->addLayout(header);
    root->addSpacing(8);

    m_errorLabel = Theme::label(QString(), 9, QFont::Normal,
                                Theme::Danger, this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_errorLabel->setStyleSheet(QStringLiteral(
        "background: rgba(255, 107, 120, 14); border-radius: 7px;"
        "padding: 7px 9px; color: %1;").arg(Theme::Danger.name()));
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);
    root->addSpacing(12);

    m_itemsContainer = new QWidget(this);
    m_itemsContainer->setAttribute(Qt::WA_NoSystemBackground);
    m_itemsLayout = new QVBoxLayout(m_itemsContainer);
    m_itemsLayout->setContentsMargins(0, 0, 0, 0);
    m_itemsLayout->setSpacing(8);
    m_itemsContainer->setMinimumHeight(230);
    root->addWidget(m_itemsContainer, 1);
    root->addSpacing(8);

    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(8);
    m_disclaimerButton = new AccentButton(QStringLiteral("免责声明"),
                                           Icons::Type::Info, this);
    m_disclaimerButton->setVariant(AccentButton::Variant::Ghost);
    m_disclaimerButton->setCompact(true);
    connect(m_disclaimerButton, &QAbstractButton::clicked,
            this, &HomeNewsWidget::showDisclaimer);
    footer->addWidget(m_disclaimerButton);
    footer->addStretch();

    m_previousButton = new AccentButton(QStringLiteral("上一页"),
                                        Icons::Type::ChevronLeft, this);
    m_previousButton->setVariant(AccentButton::Variant::Secondary);
    m_previousButton->setCompact(true);
    connect(m_previousButton, &QAbstractButton::clicked, this, [this]() {
        if (m_page > 0) {
            --m_page;
            updatePage();
        }
    });
    m_pageLabel = Theme::label(QStringLiteral("1/1"), 10, QFont::DemiBold,
                               kTextSecondary, this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setMinimumWidth(38);
    m_nextButton = new AccentButton(QStringLiteral("下一页"),
                                    Icons::Type::ChevronRight, this);
    m_nextButton->setVariant(AccentButton::Variant::Secondary);
    m_nextButton->setCompact(true);
    connect(m_nextButton, &QAbstractButton::clicked, this, [this]() {
        const int pageCount = qMax(1, (m_items.size() + kItemsPerPage - 1)
                                          / kItemsPerPage);
        if (m_page + 1 < pageCount) {
            ++m_page;
            updatePage();
        }
    });
    footer->addWidget(m_previousButton);
    footer->addWidget(m_pageLabel);
    footer->addWidget(m_nextButton);
    root->addLayout(footer);

    updatePage();
    refresh();
}

void HomeNewsWidget::setAnimationsEnabled(bool enabled)
{
    m_animationsEnabled = enabled;
    if (m_loadingIndicator) {
        m_loadingIndicator->setAnimationsEnabled(enabled);
    }
}

void HomeNewsWidget::refresh()
{
    ++m_generation;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_items.clear();
    m_endpointIndex = 0;
    m_page = 0;
    m_loading = true;
    m_lastErrors.clear();
    if (m_errorLabel) {
        m_errorLabel->clear();
        m_errorLabel->hide();
    }
    if (m_itemsLayout) {
        while (QLayoutItem *item = m_itemsLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->hide();
                widget->deleteLater();
            }
            delete item;
        }
    }
    if (m_countLabel) {
        m_countLabel->setText(QStringLiteral("共 0 条"));
    }
    if (m_sourceLabel) {
        m_sourceLabel->clear();
    }
    if (m_statusLabel) {
        m_statusLabel->setText(QStringLiteral("正在获取…"));
        m_statusLabel->show();
    }
    if (m_loadingIndicator) {
        m_loadingIndicator->setAnimationsEnabled(m_animationsEnabled);
        m_loadingIndicator->show();
    }
    if (m_refreshButton) {
        m_refreshButton->setEnabled(false);
    }
    updatePage();
    requestHotList();
}

void HomeNewsWidget::requestHotList()
{
    if (m_endpointIndex >= m_endpoints.size()) {
        finishLoading();
        return;
    }

    const int generation = m_generation;
    QNetworkRequest request(QUrl(m_endpoints.at(m_endpointIndex)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                     "AppleWebKit/537.36 Chrome/124.0 Safari/537.36 "
                                     "MysticTurtle/3.0"));
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->setInterval(kRequestTimeoutMs);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("timedOut", true);
        reply->abort();
    });
    timeout->start();

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, timeout, generation]() {
        timeout->stop();
        if (generation != m_generation) {
            reply->deleteLater();
            return;
        }
        if (reply == m_reply) {
            m_reply = nullptr;
        }
        handleReply(reply);
        reply->deleteLater();
    });
}

void HomeNewsWidget::handleReply(QNetworkReply *reply)
{
    const QString endpoint = m_endpointIndex >= 0
                                 && m_endpointIndex < m_endpoints.size()
                                 ? m_endpoints.at(m_endpointIndex)
                                 : QString();
    const bool timedOut = !reply || reply->property("timedOut").toBool();
    const int httpStatus = reply
        ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
        : 0;
    const bool failed = !reply
        || timedOut
        || reply->error() != QNetworkReply::NoError;
    QVector<HotItem> parsed;
    QString failureReason;
    if (failed) {
        if (timedOut) {
            failureReason = QStringLiteral("请求超时（10 秒）");
        } else if (httpStatus > 0) {
            failureReason = QStringLiteral("HTTP %1").arg(httpStatus);
        } else if (reply) {
            failureReason = reply->errorString();
        } else {
            failureReason = QStringLiteral("请求未完成");
        }
    } else {
        parsed = parseHotList(reply->readAll(),
                              sourceNameForEndpoint(endpoint));
        if (parsed.isEmpty()) {
            failureReason = QStringLiteral(
                "接口返回成功，但没有可解析的 data/title/url 条目（接口格式可能已变化）");
        }
    }

    if (!parsed.isEmpty()) {
        m_items = parsed;
        finishLoading();
        return;
    }

    m_lastErrors.append(QStringLiteral("%1 → %2")
                            .arg(endpoint, failureReason));
    ++m_endpointIndex;
    if (m_endpointIndex < m_endpoints.size()) {
        requestHotList();
        return;
    }
    finishLoading();
}

QVector<HomeNewsWidget::HotItem> HomeNewsWidget::parseHotList(
    const QByteArray &data, const QString &source) const
{
    QVector<HotItem> result;
    QSet<QString> seen;
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (document.isNull()) {
        return result;
    }

    std::function<void(const QJsonValue &)> walk =
        [&](const QJsonValue &value) {
        if (result.size() >= kMaximumItems) {
            return;
        }
        if (value.isArray()) {
            const QJsonArray array = value.toArray();
            for (const QJsonValue &child : array) {
                walk(child);
                if (result.size() >= kMaximumItems) {
                    return;
                }
            }
            return;
        }
        if (!value.isObject()) {
            return;
        }

        const QJsonObject object = value.toObject();
        QString title = object.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty()) {
            title = object.value(QStringLiteral("word")).toString().trimmed();
        }
        if (title.isEmpty()) {
            title = object.value(QStringLiteral("name")).toString().trimmed();
        }
        QString description =
            object.value(QStringLiteral("excerpt")).toString().trimmed();
        if (description.isEmpty()) {
            description =
                object.value(QStringLiteral("description")).toString().trimmed();
        }
        if (description.isEmpty()) {
            description =
                object.value(QStringLiteral("desc")).toString().trimmed();
        }
        if (description.isEmpty()) {
            description =
                object.value(QStringLiteral("labelTagName")).toString().trimmed();
        }
        if (description.isEmpty()) {
            description = object.value(QStringLiteral("hotTag")).toString().trimmed();
        }
        QString url = object.value(QStringLiteral("url")).toString().trimmed();
        if (url.isEmpty()) {
            url = object.value(QStringLiteral("link")).toString().trimmed();
        }
        if (url.isEmpty()) {
            url = object.value(QStringLiteral("mobileUrl")).toString().trimmed();
        }

        const QString type = object.value(QStringLiteral("type")).toString();
        if (!title.isEmpty() && url.startsWith(QStringLiteral("http"))) {
            if (url.contains(QStringLiteral("api.zhihu.com/questions/"))) {
                const QString id = url.section(QLatin1Char('/'), -1);
                if (!id.isEmpty()) {
                    url = QStringLiteral("https://www.zhihu.com/question/%1").arg(id);
                }
            } else if (type == QStringLiteral("question")
                       && !object.value(QStringLiteral("id")).isUndefined()) {
                url = QStringLiteral("https://www.zhihu.com/question/%1")
                          .arg(object.value(QStringLiteral("id")).toVariant().toString());
            }
            const QString key = title + QLatin1Char('\n') + url;
            if (!seen.contains(key)) {
                seen.insert(key);
                result.append({title, description, url, source});
            }
        }

        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.value().isArray() || it.value().isObject()) {
                walk(it.value());
                if (result.size() >= kMaximumItems) {
                    return;
                }
            }
        }
    };

    walk(document.isArray() ? QJsonValue(document.array())
                            : QJsonValue(document.object()));
    return result;
}

void HomeNewsWidget::finishLoading()
{
    m_loading = false;
    if (m_loadingIndicator) {
        m_loadingIndicator->hide();
    }
    if (m_refreshButton) {
        m_refreshButton->setEnabled(true);
    }
    if (m_statusLabel) {
        if (m_items.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("获取失败，可刷新重试"));
            m_statusLabel->show();
        } else {
            m_statusLabel->hide();
        }
    }
    if (m_errorLabel) {
        if (m_items.isEmpty() && !m_lastErrors.isEmpty()) {
            m_errorLabel->setText(m_lastErrors.join(QLatin1Char('\n')));
            m_errorLabel->show();
        } else {
            m_errorLabel->clear();
            m_errorLabel->hide();
        }
    }
    m_page = 0;
    updateCount();
    if (m_sourceLabel) {
        m_sourceLabel->setText(
            m_items.isEmpty()
                ? QString()
                : QStringLiteral("来源：%1").arg(m_items.first().source));
    }
    updatePage();
}

void HomeNewsWidget::updateCount()
{
    if (m_countLabel) {
        m_countLabel->setText(QStringLiteral("共 %1 条").arg(m_items.size()));
    }
}

void HomeNewsWidget::updatePage()
{
    if (!m_itemsLayout) {
        return;
    }

    while (QLayoutItem *item = m_itemsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    const int pageCount = qMax(1, (m_items.size() + kItemsPerPage - 1)
                                      / kItemsPerPage);
    m_page = qBound(0, m_page, pageCount - 1);
    const int start = m_page * kItemsPerPage;
    const int end = qMin(start + kItemsPerPage, m_items.size());
    for (int i = start; i < end; ++i) {
        const HotItem &item = m_items.at(i);
        auto *card = new HotItemCard(item.title, item.description,
                                     item.source, m_itemsContainer);
        card->setToolTip(QStringLiteral("用 Bing 搜索：%1").arg(item.title));
        const QString searchTitle = item.title;
        connect(card, &QAbstractButton::clicked, this,
                [this, searchTitle]() { openBingSearch(searchTitle); });
        m_itemsLayout->addWidget(card);
    }
    m_itemsLayout->addStretch(1);

    if (m_pageLabel) {
        m_pageLabel->setText(QStringLiteral("%1/%2").arg(m_page + 1).arg(pageCount));
    }
    if (m_previousButton) {
        m_previousButton->setEnabled(!m_loading && m_page > 0);
    }
    if (m_nextButton) {
        m_nextButton->setEnabled(!m_loading && m_page + 1 < pageCount);
    }
}

void HomeNewsWidget::openBingSearch(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QUrl searchUrl(QStringLiteral("https://www.bing.com/search"));
    QUrlQuery parameters;
    parameters.addQueryItem(QStringLiteral("q"), trimmed);
    searchUrl.setQuery(parameters);
    openUrl(searchUrl);
}

void HomeNewsWidget::openUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        return;
    }

    // Re-encode the URL before handing it to the browser. This avoids malformed
    // UTF-8/percent sequences being passed through the Windows shell.
    const QUrl safeUrl = QUrl::fromEncoded(url.toEncoded(QUrl::FullyEncoded),
                                           QUrl::StrictMode);
    if (!safeUrl.isValid() || safeUrl.host().isEmpty()) {
        return;
    }

    if (QDesktopServices::openUrl(safeUrl)) {
        return;
    }

#ifdef Q_OS_WIN
    // Do not fall back to "cmd /c start": '&' inside Baidu query URLs would be
    // parsed as a shell separator and can launch/crash the browser with a
    // truncated command line. ShellExecute receives the URL as one argument.
    const std::wstring wideUrl =
        safeUrl.toString(QUrl::FullyEncoded).toStdWString();
    ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
#else
    QProcess::startDetached(QStringLiteral("xdg-open"),
                            {safeUrl.toString(QUrl::FullyEncoded)});
#endif
}

void HomeNewsWidget::showDisclaimer()
{
    QDialog dialog(window() ? window() : this);
    dialog.setWindowTitle(QStringLiteral("免责声明"));
    dialog.setModal(true);
    dialog.setMinimumWidth(540);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #111821; }"
        "QTextBrowser { background: transparent; border: none;"
        "color: #A4B1B9; }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(12);
    layout->addWidget(Theme::label(QStringLiteral("免责声明"), 19,
                                   QFont::DemiBold, Theme::TextPrimary, &dialog));

    auto *body = new QTextBrowser(&dialog);
    body->setFrameShape(QFrame::NoFrame);
    body->setOpenExternalLinks(false);
    body->setMinimumHeight(250);
    body->setHtml(QStringLiteral(
        "<p>本页资讯来自互联网公开接口，仅用于信息展示。</p>"
        "<p>MysticTurtle 不对所拉取内容的真实性、准确性、完整性或合法性"
        "作出任何保证，也不对用户基于这些内容进行的操作承担责任。</p>"
        "<p>点击条目会跳转至第三方网站，相关内容及服务由该网站提供，"
        "其版权与责任归原网站所有。</p>"
        "<p>请在浏览和使用这些内容时遵守所在国家或地区的法律法规，"
        "自行判断内容风险，并对自己进行的操作负责。</p>"));
    layout->addWidget(body, 1);

    auto *close = new AccentButton(QStringLiteral("知道了"),
                                   Icons::Type::Check, &dialog);
    close->setVariant(AccentButton::Variant::Secondary);
    close->setMinimumHeight(38);
    close->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(close, &QAbstractButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(close);

    dialog.exec();
}
