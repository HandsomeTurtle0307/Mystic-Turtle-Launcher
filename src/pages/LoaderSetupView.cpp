/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "LoaderSetupView.h"

#include "AccentButton.h"
#include "CardWidget.h"
#include "IconFactory.h"
#include "LoadingIndicator.h"
#include "Theme.h"

#include <QFileInfo>
#include <QFrame>
#include <QDir>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QMouseEvent>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QStringList>
#include <QStyle>
#include <QUrlQuery>
#include <QVariant>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {

const QString kFabricLoaderUrl =
    QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1");
const QString kForgeListUrl =
    QStringLiteral("https://bmclapi2.bangbang93.com/forge/minecraft/%1");
const QString kNeoForgeListUrl =
    QStringLiteral("https://bmclapi2.bangbang93.com/neoforge/list/%1");
const QString kOptiFineListUrl =
    QStringLiteral("https://bmclapi2.bangbang93.com/optifine/%1");
const QString kForgeMavenBase =
    QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/");

QString formatLoaderVersion(const QString &loader, const QString &version)
{
    if (loader == QStringLiteral("Fabric")) {
        return QStringLiteral("Fabric %1").arg(version);
    }
    return version;
}

} // namespace

class LoaderRow : public QFrame
{
public:
    explicit LoaderRow(const QString &title, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("loaderRow"));
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(64);
        const QString border = QStringLiteral("rgba(%1, %2, %3, %4)")
                                   .arg(Theme::Border.red())
                                   .arg(Theme::Border.green())
                                   .arg(Theme::Border.blue())
                                   .arg(Theme::Border.alpha());
        setStyleSheet(QStringLiteral(
            "QFrame#loaderRow {"
            "background: transparent;"
            "border: 1px solid %1;"
            "border-radius: 14px; }"
            "QFrame#loaderRow:hover {"
            "background: rgba(110, 231, 183, 12);"
            "border-color: %2; }"
            "QFrame#loaderRow[selected=\"true\"] {"
            "background: rgba(110, 231, 183, 18);"
            "border-color: #2F80ED; }")
            .arg(border, Theme::Accent.name()));

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 10, 18, 10);
        layout->setSpacing(12);

        m_title = Theme::label(title, 14, QFont::DemiBold, Theme::TextPrimary, this);
        m_title->setMinimumWidth(105);
        layout->addWidget(m_title);
        layout->addStretch();

        auto *statusWidget = new QWidget(this);
        auto *statusLayout = new QHBoxLayout(statusWidget);
        statusLayout->setContentsMargins(0, 0, 0, 0);
        statusLayout->setSpacing(6);
        m_loadingIndicator = new LoadingIndicator(statusWidget, Theme::Accent);
        m_loadingIndicator->setFixedSize(16, 16);
        statusLayout->addWidget(m_loadingIndicator);
        m_status = Theme::label(QStringLiteral("正在获取…"), 11, QFont::Normal,
                                Theme::TextMuted, statusWidget);
        m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLayout->addWidget(m_status);
        layout->addWidget(statusWidget);

        m_chevron = new QLabel(this);
        m_chevron->setPixmap(Icons::icon(Icons::Type::ChevronRight,
                                         Theme::TextMuted, 16).pixmap(16, 16));
        m_chevron->setFixedSize(16, 16);
        m_chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(m_chevron);
    }

    void setStatus(const QString &text, const QColor &color = Theme::TextMuted)
    {
        m_status->setText(text);
        QPalette palette = m_status->palette();
        palette.setColor(QPalette::WindowText, color);
        palette.setColor(QPalette::Text, color);
        m_status->setPalette(palette);
    }

    void setLoading(bool loading)
    {
        m_loadingIndicator->setVisible(loading);
    }

    void setSelected(bool selected)
    {
        if (property("selected").toBool() == selected) {
            return;
        }
        setProperty("selected", selected);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    void setChevronVisible(bool visible)
    {
        m_chevron->setVisible(visible);
        setCursor(visible ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    void setClickHandler(std::function<void()> handler)
    {
        m_clickHandler = std::move(handler);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_clickHandler) {
            m_clickHandler();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QLabel *m_title = nullptr;
    QLabel *m_status = nullptr;
    LoadingIndicator *m_loadingIndicator = nullptr;
    QLabel *m_chevron = nullptr;
    std::function<void()> m_clickHandler;
};

LoaderSetupView::LoaderSetupView(const QString &gameVersion, const QString &gameDirectory,
                                 QWidget *parent)
    : QWidget(parent)
    , m_gameVersion(gameVersion)
    , m_gameDirectory(QDir(gameDirectory).absolutePath())
    , m_network(new QNetworkAccessManager(this))
{
    buildUi();
    setGameDirectory(m_gameDirectory);
    fetchCatalogs();
}

QString LoaderSetupView::gameVersion() const
{
    return m_gameVersion;
}

QString LoaderSetupView::gameDirectory() const
{
    return m_gameDirectory;
}

void LoaderSetupView::setGameDirectory(const QString &directory)
{
    m_gameDirectory = QDir(directory).absolutePath();
    if (!m_pathLabel) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(m_gameDirectory);
    QString displayPath = nativePath;
    if (displayPath.size() > 74) {
        displayPath = displayPath.left(22) + QStringLiteral("…")
            + displayPath.right(48);
    }
    m_pathLabel->setText(QStringLiteral("下载目录：%1").arg(displayPath));
    m_pathLabel->setToolTip(nativePath);
}

void LoaderSetupView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 22, 32, 18);
    root->setSpacing(0);

    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);
    auto *back = new AccentButton(QString(), Icons::Type::ChevronLeft, this);
    back->setVariant(AccentButton::Variant::Ghost);
    back->setCompact(true);
    back->setFixedWidth(42);
    back->setToolTip(QStringLiteral("返回版本列表"));
    back->setAccessibleName(QStringLiteral("返回版本列表"));
    connect(back, &QAbstractButton::clicked, this, &LoaderSetupView::backRequested);
    topRow->addWidget(back);
    topRow->addWidget(Theme::label(QStringLiteral("安装加载器"), 17, QFont::DemiBold,
                                   Theme::TextPrimary, this));
    topRow->addStretch();
    root->addLayout(topRow);
    root->addSpacing(14);

    auto *versionCard = new CardWidget(this);
    versionCard->setAccentColor(Theme::Accent);
    versionCard->setCornerRadius(18.0);
    versionCard->setInteractive(false);
    versionCard->setFixedHeight(84);
    auto *versionLayout = new QHBoxLayout(versionCard);
    versionLayout->setContentsMargins(18, 14, 18, 14);
    versionLayout->setSpacing(14);

    auto *iconLabel = new QLabel(versionCard);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setPixmap(Icons::icon(Icons::Type::Cube, Theme::Accent, 26).pixmap(26, 26));
    iconLabel->setStyleSheet(QStringLiteral(
        "background: rgba(110, 231, 183, 22);"
        "border: 1px solid rgba(110, 231, 183, 55);"
        "border-radius: 13px;"));
    versionLayout->addWidget(iconLabel);

    auto *versionText = new QVBoxLayout();
    versionText->setContentsMargins(0, 0, 0, 0);
    versionText->setSpacing(2);
    versionText->addWidget(Theme::label(QStringLiteral("Minecraft %1").arg(m_gameVersion),
                                        20, QFont::DemiBold, Theme::TextPrimary,
                                        versionCard, true));
    versionText->addWidget(Theme::label(QStringLiteral("选择加载器版本，开始下载时获取对应文件。"),
                                        11, QFont::Normal, Theme::TextMuted, versionCard));
    versionLayout->addLayout(versionText, 1);
    root->addWidget(versionCard);
    root->addSpacing(8);

    m_pathLabel = Theme::label(QString(), 10, QFont::Normal, Theme::TextMuted, this);
    m_pathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(m_pathLabel);
    root->addSpacing(14);

    m_forgeRow = new LoaderRow(QStringLiteral("Forge"), this);
    m_forgeRow->setClickHandler([this]() { chooseLoader(QStringLiteral("Forge")); });
    root->addWidget(m_forgeRow);
    root->addSpacing(10);

    m_neoForgeRow = new LoaderRow(QStringLiteral("NeoForge"), this);
    m_neoForgeRow->setClickHandler([this]() { chooseLoader(QStringLiteral("NeoForge")); });
    root->addWidget(m_neoForgeRow);
    root->addSpacing(10);

    m_fabricRow = new LoaderRow(QStringLiteral("Fabric"), this);
    m_fabricRow->setClickHandler([this]() { chooseLoader(QStringLiteral("Fabric")); });
    root->addWidget(m_fabricRow);
    root->addSpacing(10);

    m_optiFineRow = new LoaderRow(QStringLiteral("OptiFine"), this);
    m_optiFineRow->setClickHandler([this]() { chooseLoader(QStringLiteral("OptiFine")); });
    root->addWidget(m_optiFineRow);
    root->addSpacing(10);

    m_fabricApiRow = new LoaderRow(QStringLiteral("Fabric API"), this);
    m_fabricApiRow->setClickHandler([this]() { chooseFabricApi(); });
    m_fabricApiRow->hide();
    root->addWidget(m_fabricApiRow);

    root->addStretch(1);

    m_hintLabel = Theme::label(QStringLiteral("未选择加载器时，将直接下载原版游戏。"),
                               10, QFont::Normal, Theme::TextMuted, this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_hintLabel);
    root->addSpacing(12);

    auto *startRow = new QHBoxLayout();
    startRow->setContentsMargins(0, 0, 0, 0);
    startRow->addStretch();
    m_startButton = new AccentButton(QStringLiteral("开始下载"), Icons::Type::Download, this);
    m_startButton->setVariant(AccentButton::Variant::Primary);
    m_startButton->setMinimumWidth(220);
    m_startButton->setFixedHeight(42);
    connect(m_startButton, &QAbstractButton::clicked, this, [this]() {
        if (m_selectedLoader == QStringLiteral("Fabric")
            && m_selectedFabricApi.version.isEmpty()) {
            emit notifyRequested(QStringLiteral("请先选择 Fabric API 版本。"));
            return;
        }
        emit startRequested();
    });
    startRow->addWidget(m_startButton);
    startRow->addStretch();
    root->addLayout(startRow);

    updateRowStates();
    updateStartButton();
}

void LoaderSetupView::fetchCatalogs()
{
    requestJson(kFabricLoaderUrl.arg(m_gameVersion), [this](const QJsonDocument &document,
                                                            const QString &error) {
        m_fabricLoaded = true;
        m_fabricError = error;
        if (m_fabricError.isEmpty() && !document.isArray()) {
            m_fabricError = QStringLiteral("服务器返回了无效的数据格式");
        }
        if (error.isEmpty() && document.isArray()) {
            for (const QJsonValue &value : document.array()) {
                const QJsonObject object = value.toObject();
                const QJsonObject loader = object.value(QStringLiteral("loader")).toObject();
                const QString version = loader.value(QStringLiteral("version")).toString();
                if (version.isEmpty()) {
                    continue;
                }
                LoaderOption option;
                option.version = version;
                option.label = version;
                m_fabricOptions.append(option);
            }
        }
        updateRowStates();
    });

    QUrl fabricApiUrl(QStringLiteral("https://api.modrinth.com/v2/project/fabric-api/version"));
    QUrlQuery fabricApiQuery;
    fabricApiQuery.addQueryItem(QStringLiteral("game_versions"),
                                QStringLiteral("[\"%1\"]").arg(m_gameVersion));
    fabricApiQuery.addQueryItem(QStringLiteral("loaders"), QStringLiteral("[\"fabric\"]"));
    fabricApiUrl.setQuery(fabricApiQuery);
    requestJson(fabricApiUrl.toString(), [this](const QJsonDocument &document,
                                                const QString &error) {
        m_fabricApiLoaded = true;
        m_fabricApiError = error;
        if (m_fabricApiError.isEmpty() && !document.isArray()) {
            m_fabricApiError = QStringLiteral("服务器返回了无效的数据格式");
        }
        if (error.isEmpty() && document.isArray()) {
            for (const QJsonValue &value : document.array()) {
                const QJsonObject object = value.toObject();
                const QString version = object.value(QStringLiteral("version_number"))
                                            .toString()
                                            .trimmed();
                const QJsonArray files = object.value(QStringLiteral("files")).toArray();
                if (version.isEmpty() || files.isEmpty()) {
                    continue;
                }
                QJsonObject file = files.first().toObject();
                for (const QJsonValue &fileValue : files) {
                    const QJsonObject candidate = fileValue.toObject();
                    if (candidate.value(QStringLiteral("primary")).toBool()) {
                        file = candidate;
                        break;
                    }
                }
                const QString url = file.value(QStringLiteral("url")).toString().trimmed();
                if (url.isEmpty()) {
                    continue;
                }
                FabricApiOption option;
                option.version = version;
                option.url = url;
                option.sha1 = file.value(QStringLiteral("hashes")).toObject()
                                  .value(QStringLiteral("sha1")).toString().trimmed();
                option.filename = file.value(QStringLiteral("filename")).toString().trimmed();
                m_fabricApiOptions.append(option);
                if (m_fabricApiOptions.size() >= 40) {
                    break;
                }
            }
        }
        updateRowStates();
    });

    requestJson(kForgeListUrl.arg(m_gameVersion), [this](const QJsonDocument &document,
                                                         const QString &error) {
        m_forgeLoaded = true;
        m_forgeError = error;
        if (m_forgeError.isEmpty() && !document.isArray()) {
            m_forgeError = QStringLiteral("服务器返回了无效的数据格式");
        }
        if (error.isEmpty() && document.isArray()) {
            const QJsonArray entries = document.array();
            for (int i = entries.size() - 1; i >= 0; --i) {
                const QJsonObject object = entries.at(i).toObject();
                const QString version = object.value(QStringLiteral("version")).toString();
                if (version.isEmpty()) {
                    continue;
                }
                QString installerSha1;
                const QJsonArray files = object.value(QStringLiteral("files")).toArray();
                for (const QJsonValue &fileValue : files) {
                    const QJsonObject file = fileValue.toObject();
                    if (file.value(QStringLiteral("category")).toString()
                        == QStringLiteral("installer")) {
                        installerSha1 = file.value(QStringLiteral("hash")).toString();
                        break;
                    }
                }
                const QString coordinate = QStringLiteral("%1-%2").arg(m_gameVersion, version);
                LoaderOption option;
                option.version = version;
                option.label = version;
                option.filename =
                    QStringLiteral("forge-%1-installer.jar").arg(coordinate);
                option.sha1 = installerSha1;
                option.url = kForgeMavenBase + coordinate + QStringLiteral("/forge-")
                    + coordinate + QStringLiteral("-installer.jar");
                m_forgeOptions.append(option);
            }
        }
        updateRowStates();
    });

    requestJson(kNeoForgeListUrl.arg(m_gameVersion), [this](const QJsonDocument &document,
                                                            const QString &error) {
        m_neoForgeLoaded = true;
        m_neoForgeError = error;
        if (m_neoForgeError.isEmpty() && !document.isArray()) {
            m_neoForgeError = QStringLiteral("服务器返回了无效的数据格式");
        }
        if (error.isEmpty() && document.isArray()) {
            const QJsonArray entries = document.array();
            for (int i = entries.size() - 1; i >= 0; --i) {
                const QJsonObject object = entries.at(i).toObject();
                const QString version = object.value(QStringLiteral("version")).toString();
                const QString installerPath =
                    object.value(QStringLiteral("installerPath")).toString();
                if (version.isEmpty() || installerPath.isEmpty()) {
                    continue;
                }
                LoaderOption option;
                option.version = version;
                option.label = version;
                option.filename = QFileInfo(installerPath).fileName();
                option.url = QStringLiteral("https://bmclapi2.bangbang93.com") + installerPath;
                m_neoForgeOptions.append(option);
            }
        }
        updateRowStates();
    });

    requestJson(kOptiFineListUrl.arg(m_gameVersion), [this](const QJsonDocument &document,
                                                            const QString &error) {
        m_optiFineLoaded = true;
        m_optiFineError = error;
        if (m_optiFineError.isEmpty() && !document.isArray()) {
            m_optiFineError = QStringLiteral("服务器返回了无效的数据格式");
        }
        if (error.isEmpty() && document.isArray()) {
            for (const QJsonValue &value : document.array()) {
                const QJsonObject object = value.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                const QString patch = object.value(QStringLiteral("patch")).toString();
                const QString filename =
                    object.value(QStringLiteral("filename")).toString();
                if (type.isEmpty() || patch.isEmpty() || filename.isEmpty()) {
                    continue;
                }
                LoaderOption option;
                option.version = QStringLiteral("%1 %2").arg(type, patch);
                option.label = filename;
                option.filename = filename;
                option.url = QStringLiteral("https://bmclapi2.bangbang93.com/optifine/")
                    + m_gameVersion + QStringLiteral("/") + type + QStringLiteral("/")
                    + patch;
                m_optiFineOptions.append(option);
            }
        }
        updateRowStates();
    });
}

void LoaderSetupView::requestJson(
    const QString &url,
    const std::function<void(const QJsonDocument &, const QString &)> &handler)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MysticTurtle/0.2"));
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_network->get(request);
    if (!reply) {
        handler(QJsonDocument(), QStringLiteral("无法发起网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [reply, handler]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            handler(QJsonDocument(), reply->errorString());
            return;
        }
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            handler(QJsonDocument(QJsonArray()), QString());
            return;
        }
        if (status >= 400) {
            handler(QJsonDocument(), QStringLiteral("HTTP %1").arg(status));
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            handler(QJsonDocument(), QStringLiteral("JSON 解析失败"));
            return;
        }
        handler(document, QString());
    });
}

void LoaderSetupView::updateRowStates()
{
    const auto setLoaderRow = [this](LoaderRow *row, const QString &loader,
                                     bool loaded, const QString &error,
                                     const QVector<LoaderOption> &options) {
        if (!row) {
            return;
        }
        row->setLoading(!loaded && m_selectedLoader != loader);
        if (m_selectedLoader == loader) {
            row->setStatus(formatLoaderVersion(loader, m_selectedLoaderOption.version),
                           Theme::Accent);
            row->setSelected(true);
            row->setChevronVisible(true);
            return;
        }

        row->setSelected(false);
        if (!loaded) {
            row->setStatus(QStringLiteral("正在获取…"), Theme::TextMuted);
            row->setChevronVisible(false);
        } else if (!error.isEmpty()) {
            row->setStatus(QStringLiteral("获取失败"), Theme::Danger);
            row->setChevronVisible(false);
        } else if (options.isEmpty()) {
            row->setStatus(QStringLiteral("无"), Theme::TextMuted);
            row->setChevronVisible(false);
        } else {
            row->setStatus(QStringLiteral("可以添加"), Theme::TextSecondary);
            row->setChevronVisible(true);
        }
    };

    setLoaderRow(m_forgeRow, QStringLiteral("Forge"), m_forgeLoaded,
                 m_forgeError, m_forgeOptions);
    setLoaderRow(m_neoForgeRow, QStringLiteral("NeoForge"), m_neoForgeLoaded,
                 m_neoForgeError, m_neoForgeOptions);
    setLoaderRow(m_fabricRow, QStringLiteral("Fabric"), m_fabricLoaded,
                 m_fabricError, m_fabricOptions);
    setLoaderRow(m_optiFineRow, QStringLiteral("OptiFine"), m_optiFineLoaded,
                 m_optiFineError, m_optiFineOptions);

    if (!m_fabricApiRow) {
        return;
    }
    const bool fabricSelected = m_selectedLoader == QStringLiteral("Fabric");
    m_fabricApiRow->setVisible(fabricSelected);
    if (!fabricSelected) {
        updateStartButton();
        maybeShowCatalogErrors();
        return;
    }

    if (m_selectedFabricApi.version.isEmpty() && !m_fabricApiOptions.isEmpty()) {
        m_selectedFabricApi = m_fabricApiOptions.first();
    }

    if (!m_selectedFabricApi.version.isEmpty()) {
        m_fabricApiRow->setStatus(
            QStringLiteral("Fabric API %1").arg(m_selectedFabricApi.version),
            Theme::Accent);
        m_fabricApiRow->setSelected(true);
        m_fabricApiRow->setChevronVisible(true);
    } else {
        m_fabricApiRow->setSelected(false);
        m_fabricApiRow->setLoading(!m_fabricApiLoaded);
        if (!m_fabricApiLoaded) {
            m_fabricApiRow->setStatus(QStringLiteral("正在获取…"), Theme::TextMuted);
            m_fabricApiRow->setChevronVisible(false);
        } else if (!m_fabricApiError.isEmpty()) {
            m_fabricApiRow->setStatus(QStringLiteral("获取失败"), Theme::Danger);
            m_fabricApiRow->setChevronVisible(false);
        } else if (m_fabricApiOptions.isEmpty()) {
            m_fabricApiRow->setStatus(QStringLiteral("无"), Theme::TextMuted);
            m_fabricApiRow->setChevronVisible(false);
        } else {
            m_fabricApiRow->setStatus(QStringLiteral("请选择版本"), Theme::Amber);
            m_fabricApiRow->setChevronVisible(true);
        }
    }

    updateStartButton();
    maybeShowCatalogErrors();
}

void LoaderSetupView::maybeShowCatalogErrors()
{
    if (m_catalogErrorReported || !m_forgeLoaded || !m_neoForgeLoaded
        || !m_fabricLoaded || !m_optiFineLoaded || !m_fabricApiLoaded) {
        return;
    }

    QStringList failures;
    if (!m_forgeError.isEmpty()) {
        failures.append(QStringLiteral("Forge：%1").arg(m_forgeError));
    }
    if (!m_neoForgeError.isEmpty()) {
        failures.append(QStringLiteral("NeoForge：%1").arg(m_neoForgeError));
    }
    if (!m_fabricError.isEmpty()) {
        failures.append(QStringLiteral("Fabric：%1").arg(m_fabricError));
    }
    if (!m_optiFineError.isEmpty()) {
        failures.append(QStringLiteral("OptiFine：%1").arg(m_optiFineError));
    }
    if (!m_fabricApiError.isEmpty()) {
        failures.append(QStringLiteral("Fabric API：%1").arg(m_fabricApiError));
    }
    if (failures.isEmpty()) {
        return;
    }

    m_catalogErrorReported = true;
    QMessageBox::warning(
        this, QStringLiteral("加载器版本加载失败"),
        QStringLiteral("部分加载器版本目录无法加载：\n%1")
            .arg(failures.join(QLatin1Char('\n'))));
}

void LoaderSetupView::updateStartButton()
{
    if (!m_startButton || !m_hintLabel) {
        return;
    }

    const bool needsFabricApi =
        m_selectedLoader == QStringLiteral("Fabric")
        && m_selectedFabricApi.version.isEmpty();
    m_startButton->setEnabled(!needsFabricApi);

    if (needsFabricApi) {
        const bool apiUnavailable =
            m_fabricApiLoaded
            && (!m_fabricApiError.isEmpty() || m_fabricApiOptions.isEmpty());
        if (apiUnavailable) {
            m_hintLabel->setText(
                QStringLiteral("Fabric API 暂无可用版本，请选择其他加载器或返回重试。"));
        } else {
            m_hintLabel->setText(QStringLiteral("Fabric 需要同时选择 Fabric API 版本。"));
        }
    } else if (m_selectedLoader.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("未选择加载器时，将直接下载原版游戏。"));
    } else if (m_selectedLoader == QStringLiteral("Fabric")) {
        m_hintLabel->setText(QStringLiteral("将下载 Fabric 配置、依赖库与 Fabric API。"));
    } else if (m_selectedLoader == QStringLiteral("OptiFine")) {
        m_hintLabel->setText(
            QStringLiteral("OptiFine 安装器将下载到 installers，需手动运行；官方没有静默安装参数。"));
    } else {
        m_hintLabel->setText(
            QStringLiteral("%1 %2 安装器下载完成后会自动安装。")
                .arg(m_selectedLoader, m_selectedLoaderOption.version));
    }
}

void LoaderSetupView::chooseLoader(const QString &loader)
{
    const QVector<LoaderOption> &options = optionsFor(loader);
    if (options.isEmpty()) {
        if (loader == QStringLiteral("Forge") && !m_forgeLoaded) {
            emit notifyRequested(QStringLiteral("正在获取 Forge 版本列表，请稍候。"));
        } else if (loader == QStringLiteral("NeoForge") && !m_neoForgeLoaded) {
            emit notifyRequested(QStringLiteral("正在获取 NeoForge 版本列表，请稍候。"));
        } else if (loader == QStringLiteral("Fabric") && !m_fabricLoaded) {
            emit notifyRequested(QStringLiteral("正在获取 Fabric 版本列表，请稍候。"));
        } else if (loader == QStringLiteral("OptiFine") && !m_optiFineLoaded) {
            emit notifyRequested(QStringLiteral("正在获取 OptiFine 版本列表，请稍候。"));
        } else {
            emit notifyRequested(QStringLiteral("当前版本没有可用的 %1。").arg(loader));
        }
        return;
    }

    QStringList labels;
    int currentIndex = 0;
    for (int i = 0; i < options.size(); ++i) {
        labels.append(options.at(i).label);
        if (m_selectedLoader == loader
            && m_selectedLoaderOption.version == options.at(i).version) {
            currentIndex = i;
        }
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(
        this, QStringLiteral("选择 %1 版本").arg(loader),
        QStringLiteral("版本"), labels, currentIndex, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }

    const int index = labels.indexOf(selected);
    if (index < 0 || index >= options.size()) {
        return;
    }

    m_selectedLoader = loader;
    m_selectedLoaderOption = options.at(index);
    if (loader != QStringLiteral("Fabric")) {
        m_selectedFabricApi = FabricApiOption();
    }
    updateRowStates();
    updateStartButton();
}

void LoaderSetupView::chooseFabricApi()
{
    if (m_selectedLoader != QStringLiteral("Fabric")) {
        return;
    }
    if (m_fabricApiOptions.isEmpty()) {
        emit notifyRequested(QStringLiteral("当前版本没有可用的 Fabric API。"));
        return;
    }

    QStringList labels;
    int currentIndex = 0;
    for (int i = 0; i < m_fabricApiOptions.size(); ++i) {
        labels.append(m_fabricApiOptions.at(i).version);
        if (m_selectedFabricApi.version == m_fabricApiOptions.at(i).version) {
            currentIndex = i;
        }
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(
        this, QStringLiteral("选择 Fabric API 版本"), QStringLiteral("Fabric API"),
        labels, currentIndex, false, &ok);
    if (!ok || selected.isEmpty()) {
        return;
    }

    const int index = labels.indexOf(selected);
    if (index < 0 || index >= m_fabricApiOptions.size()) {
        return;
    }
    m_selectedFabricApi = m_fabricApiOptions.at(index);
    updateRowStates();
    updateStartButton();
}

const QVector<LoaderSetupView::LoaderOption> &LoaderSetupView::optionsFor(
    const QString &loader) const
{
    static const QVector<LoaderOption> empty;
    if (loader == QStringLiteral("Forge")) {
        return m_forgeOptions;
    }
    if (loader == QStringLiteral("NeoForge")) {
        return m_neoForgeOptions;
    }
    if (loader == QStringLiteral("Fabric")) {
        return m_fabricOptions;
    }
    if (loader == QStringLiteral("OptiFine")) {
        return m_optiFineOptions;
    }
    return empty;
}

LoaderSelection LoaderSetupView::selection() const
{
    LoaderSelection result;
    result.loader = m_selectedLoader;
    result.loaderVersion = m_selectedLoaderOption.version;
    result.downloadUrl = m_selectedLoaderOption.url;
    result.downloadSha1 = m_selectedLoaderOption.sha1;
    result.downloadFilename = m_selectedLoaderOption.filename;

    if (m_selectedLoader == QStringLiteral("Fabric")) {
        result.fabricApiVersion = m_selectedFabricApi.version;
        result.fabricApiUrl = m_selectedFabricApi.url;
        result.fabricApiSha1 = m_selectedFabricApi.sha1;
        result.fabricApiFilename = m_selectedFabricApi.filename;
    }
    return result;
}
