/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "DownloadTask.h"

#include "MojangMirror.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <utility>

namespace {

constexpr int kMaxRetries = 3;
constexpr int kTransferTimeoutMs = 30000;
constexpr int kRetryDelayMs = 500;

QByteArray normalizeSha1(const QByteArray &hash)
{
    QByteArray value = hash.trimmed().toLower();
    value.replace(" ", "");
    value.replace(":", "");
    value.replace("-", "");
    if (value.startsWith("sha1")) {
        value.remove(0, 4);
    }
    return value;
}

bool isSha1Hex(const QByteArray &hash)
{
    if (hash.size() != 40) {
        return false;
    }
    for (char ch : hash) {
        const bool isDigit = ch >= '0' && ch <= '9';
        const bool isLowerHex = ch >= 'a' && ch <= 'f';
        if (!isDigit && !isLowerHex) {
            return false;
        }
    }
    return true;
}

} // namespace

class DownloadWorker : public QObject
{
    Q_OBJECT

public:
    explicit DownloadWorker(QUrl url,
                            QString saveFilePath,
                            QByteArray expectedSha1,
                            QObject *parent = nullptr)
        : QObject(parent)
        , m_url(std::move(url))
        , m_saveFilePath(std::move(saveFilePath))
        , m_expectedSha1(std::move(expectedSha1))
    {
    }

public slots:
    void start();
    void cancel();
    void shutdown();

signals:
    void progressChanged(int percent);
    void bytesProgressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void succeeded(const QString &filePath);
    void failed(const QString &reason);
    void finished();

private:
    void beginAttempt();
    void onReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onReplyFinished();
    bool writeToFile(const QByteArray &data);
    void scheduleRetry(const QString &reason);
    void finishWithFailure(const QString &reason);
    void finishWithSuccess();
    void cleanupReply();

    QUrl m_url;
    QUrl m_requestUrl;
    QString m_saveFilePath;
    QByteArray m_expectedSha1;
    QNetworkAccessManager *m_manager = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QTimer *m_retryTimer = nullptr;
    QString m_localError;
    qint64 m_receivedBytes = 0;
    qint64 m_totalBytes = -1;
    int m_lastPercent = -1;
    int m_retriesUsed = 0;
    int m_mirrorTimeoutCount = 0;
    bool m_usingMirror = false;
    bool m_started = false;
    bool m_cancelled = false;
    bool m_finished = false;
    bool m_fileTouched = false;
    bool m_shutdown = false;
};

void DownloadWorker::start()
{
    if (m_started) {
        return;
    }
    m_started = true;

    if (!m_url.isValid()
        || (m_url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0
            && m_url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)
        || m_url.host().isEmpty()) {
        finishWithFailure(QStringLiteral("下载 URL 无效，或协议不是 HTTP/HTTPS。"));
        return;
    }

    if (m_saveFilePath.trimmed().isEmpty() || !QFileInfo(m_saveFilePath).isAbsolute()) {
        finishWithFailure(QStringLiteral("本地保存路径必须是绝对路径。"));
        return;
    }

    const QFileInfo targetInfo(m_saveFilePath);
    if (targetInfo.isDir()) {
        finishWithFailure(QStringLiteral("本地保存路径不能是目录。"));
        return;
    }

    m_expectedSha1 = normalizeSha1(m_expectedSha1);
    if (!m_expectedSha1.isEmpty() && !isSha1Hex(m_expectedSha1)) {
        finishWithFailure(QStringLiteral("预期的 SHA1 哈希格式无效，应为 40 位十六进制字符串。"));
        return;
    }

    QDir targetDir = targetInfo.absoluteDir();
    if (!targetDir.exists() && !targetDir.mkpath(QStringLiteral("."))) {
        finishWithFailure(QStringLiteral("无法创建本地保存目录：%1").arg(targetDir.absolutePath()));
        return;
    }

    const QString officialUrl = m_url.toString();
    const QString optimizedUrl = MojangMirror::optimizedUrl(officialUrl);
    m_requestUrl = QUrl(optimizedUrl);
    if (!m_requestUrl.isValid()) {
        m_requestUrl = m_url;
    }
    m_usingMirror = optimizedUrl != officialUrl;
    m_mirrorTimeoutCount = 0;

    m_manager = new QNetworkAccessManager(this);
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &DownloadWorker::beginAttempt);

    beginAttempt();
}

void DownloadWorker::beginAttempt()
{
    if (m_finished || m_cancelled) {
        return;
    }

    cleanupReply();
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (m_fileTouched) {
        QFile::remove(m_saveFilePath);
        m_fileTouched = false;
    }

    m_file.setFileName(m_saveFilePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finishWithFailure(QStringLiteral("无法写入本地文件：%1").arg(m_file.errorString()));
        return;
    }
    m_fileTouched = true;

    m_localError.clear();
    m_receivedBytes = 0;
    m_totalBytes = -1;
    m_lastPercent = -1;
    emit progressChanged(0);
    emit bytesProgressChanged(0, 0);

    QNetworkRequest request(m_requestUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MysticTurtle/0.2"));
    request.setTransferTimeout(kTransferTimeoutMs);

    m_reply = m_manager->get(request);
    QNetworkReply *reply = m_reply.data();
    if (!reply) {
        finishWithFailure(QStringLiteral("无法发起下载请求。"));
        return;
    }

    connect(reply, &QIODevice::readyRead, this, &DownloadWorker::onReadyRead);
    connect(reply, &QNetworkReply::downloadProgress,
            this, &DownloadWorker::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &DownloadWorker::onReplyFinished);
}

void DownloadWorker::onReadyRead()
{
    if (m_finished || !m_reply) {
        return;
    }

    const QByteArray data = m_reply->readAll();
    if (!data.isEmpty() && !writeToFile(data)) {
        m_reply->abort();
    }
}

bool DownloadWorker::writeToFile(const QByteArray &data)
{
    if (!m_file.isOpen()) {
        m_localError = QStringLiteral("本地文件已关闭，无法继续写入。");
        return false;
    }

    qint64 offset = 0;
    while (offset < data.size()) {
        const qint64 written = m_file.write(data.constData() + offset, data.size() - offset);
        if (written <= 0) {
            m_localError = QStringLiteral("写入本地文件失败：%1").arg(m_file.errorString());
            return false;
        }
        offset += written;
    }
    return true;
}

void DownloadWorker::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_finished) {
        return;
    }

    m_receivedBytes = bytesReceived;
    m_totalBytes = bytesTotal;

    int percent = 0;
    if (bytesTotal > 0) {
        const double ratio = static_cast<double>(bytesReceived)
            / static_cast<double>(bytesTotal);
        percent = qBound(0, static_cast<int>(ratio * 100.0), 100);
    }

    if (percent != m_lastPercent) {
        m_lastPercent = percent;
        emit progressChanged(percent);
    }
    emit bytesProgressChanged(bytesReceived, bytesTotal);
}

void DownloadWorker::onReplyFinished()
{
    QNetworkReply *reply = m_reply.data();
    if (!reply) {
        return;
    }

    m_reply = nullptr;
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();

    if (m_finished) {
        return;
    }

    if (!m_localError.isEmpty()) {
        finishWithFailure(m_localError);
        return;
    }

    if (m_cancelled) {
        finishWithFailure(QStringLiteral("下载已取消。"));
        return;
    }

    if (m_file.isOpen()) {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty() && !writeToFile(remaining)) {
            finishWithFailure(m_localError);
            return;
        }
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 400) {
        if (m_usingMirror && httpStatus < 500) {
            // A mirror may not carry every legacy artifact. Fall back to the
            // official URL instead of failing the file immediately.
            m_requestUrl = m_url;
            m_usingMirror = false;
            m_mirrorTimeoutCount = 0;
            m_retriesUsed = 0;
            scheduleRetry(QStringLiteral("镜像返回 HTTP %1，已回退到官方源。")
                              .arg(httpStatus));
            return;
        }
        if (m_usingMirror) {
            m_mirrorTimeoutCount = 0;
        }
        const QString reason = QStringLiteral("服务器返回 HTTP %1。").arg(httpStatus);
        if (httpStatus >= 500) {
            scheduleRetry(reason);
        } else {
            finishWithFailure(reason);
        }
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        bool timedOut = reply->error() == QNetworkReply::TimeoutError;
        if (!timedOut && reply->error() == QNetworkReply::OperationCanceledError) {
            // User cancellation is handled above. Any remaining cancellation
            // during a transfer is the transfer timeout path used by Qt.
            timedOut = true;
        }

        if (m_usingMirror) {
            if (timedOut) {
                ++m_mirrorTimeoutCount;
                if (m_mirrorTimeoutCount >= 2) {
                    // Two consecutive mirror timeouts: give the official
                    // source a fresh retry budget.
                    m_requestUrl = m_url;
                    m_usingMirror = false;
                    m_mirrorTimeoutCount = 0;
                    m_retriesUsed = 0;
                    scheduleRetry(QStringLiteral("BMCLAPI 镜像连续超时，已回退到官方源。"));
                    return;
                }
            } else {
                m_mirrorTimeoutCount = 0;
            }
        }

        scheduleRetry(QStringLiteral("网络错误：%1").arg(reply->errorString()));
        return;
    }

    if (!m_file.flush()) {
        finishWithFailure(QStringLiteral("写入本地文件失败：%1").arg(m_file.errorString()));
        return;
    }
    m_file.close();

    if (!m_expectedSha1.isEmpty()) {
        QFile downloadedFile(m_saveFilePath);
        if (!downloadedFile.open(QIODevice::ReadOnly)) {
            finishWithFailure(QStringLiteral("无法读取下载文件进行校验：%1")
                                  .arg(downloadedFile.errorString()));
            return;
        }

        QCryptographicHash hasher(QCryptographicHash::Sha1);
        const bool hashed = hasher.addData(&downloadedFile);
        downloadedFile.close();
        if (!hashed) {
            finishWithFailure(QStringLiteral("计算本地文件 SHA1 失败。"));
            return;
        }

        const QByteArray actualSha1 = hasher.result().toHex();
        if (actualSha1 != m_expectedSha1) {
            finishWithFailure(
                QStringLiteral("SHA1 校验失败（期望 %1，实际 %2），已删除下载文件。")
                    .arg(QString::fromLatin1(m_expectedSha1),
                         QString::fromLatin1(actualSha1)));
            return;
        }
    }

    finishWithSuccess();
}

void DownloadWorker::scheduleRetry(const QString &reason)
{
    if (m_finished) {
        return;
    }

    cleanupReply();
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (m_fileTouched) {
        QFile::remove(m_saveFilePath);
        m_fileTouched = false;
    }

    if (m_retriesUsed >= kMaxRetries) {
        finishWithFailure(QStringLiteral("下载失败（已重试 %1 次）：%2")
                              .arg(kMaxRetries)
                              .arg(reason));
        return;
    }

    ++m_retriesUsed;
    m_receivedBytes = 0;
    m_totalBytes = -1;
    m_lastPercent = -1;
    emit progressChanged(0);
    emit bytesProgressChanged(0, 0);

    if (m_retryTimer) {
        m_retryTimer->start(kRetryDelayMs);
    }
}

void DownloadWorker::cancel()
{
    if (m_cancelled || m_finished) {
        return;
    }
    m_cancelled = true;

    if (m_retryTimer) {
        m_retryTimer->stop();
    }

    if (m_reply) {
        m_reply->abort();
        return;
    }

    finishWithFailure(QStringLiteral("下载已取消。"));
}

void DownloadWorker::finishWithFailure(const QString &reason)
{
    if (m_finished) {
        return;
    }
    m_finished = true;

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    cleanupReply();
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (m_fileTouched) {
        QFile::remove(m_saveFilePath);
        m_fileTouched = false;
    }

    emit failed(reason);
    emit finished();
}

void DownloadWorker::finishWithSuccess()
{
    if (m_finished) {
        return;
    }
    m_finished = true;

    if (m_retryTimer) {
        m_retryTimer->stop();
    }
    cleanupReply();
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_fileTouched = false;

    emit progressChanged(100);
    emit bytesProgressChanged(m_receivedBytes, m_totalBytes);
    emit succeeded(m_saveFilePath);
    emit finished();
}

void DownloadWorker::shutdown()
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    m_cancelled = true;

    if (m_retryTimer) {
        m_retryTimer->stop();
    }

    if (m_reply) {
        QNetworkReply *reply = m_reply.data();
        m_reply = nullptr;
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
    }
    if (m_manager) {
        delete m_manager;
        m_manager = nullptr;
    }
    if (m_retryTimer) {
        delete m_retryTimer;
        m_retryTimer = nullptr;
    }

    if (m_file.isOpen()) {
        m_file.close();
    }
    if (!m_finished && m_fileTouched) {
        QFile::remove(m_saveFilePath);
        m_fileTouched = false;
    }

    m_finished = true;
}

void DownloadWorker::cleanupReply()
{
    if (!m_reply) {
        return;
    }

    QNetworkReply *reply = m_reply.data();
    m_reply = nullptr;
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

DownloadTask::DownloadTask(const QUrl &url,
                           const QString &saveFilePath,
                           const QByteArray &expectedSha1,
                           QObject *parent)
    : QObject(parent)
    , m_url(url)
    , m_saveFilePath(saveFilePath)
    , m_expectedSha1(normalizeSha1(expectedSha1))
{
}

DownloadTask::~DownloadTask()
{
    cleanupThread();
}

QUrl DownloadTask::url() const
{
    return m_url;
}

QString DownloadTask::saveFilePath() const
{
    return m_saveFilePath;
}

QByteArray DownloadTask::expectedSha1() const
{
    return m_expectedSha1;
}

bool DownloadTask::isRunning() const
{
    return m_started && !m_finished;
}

bool DownloadTask::isFinished() const
{
    return m_finished;
}

void DownloadTask::start()
{
    if (m_started) {
        return;
    }
    m_started = true;

    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("MysticTurtleDownload"));
    m_worker = new DownloadWorker(m_url, m_saveFilePath, m_expectedSha1);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &DownloadWorker::start);
    connect(m_worker, &DownloadWorker::progressChanged,
            this, &DownloadTask::progressChanged);
    connect(m_worker, &DownloadWorker::bytesProgressChanged,
            this, &DownloadTask::bytesProgressChanged);
    connect(m_worker, &DownloadWorker::succeeded, this, &DownloadTask::succeeded);
    connect(m_worker, &DownloadWorker::failed, this, &DownloadTask::failed);
    connect(m_worker, &DownloadWorker::finished, this, [this]() {
        if (m_finished) {
            return;
        }
        m_finished = true;
        emit finished();
    });

    m_thread->start();
}

void DownloadTask::cancel()
{
    if (!m_started || m_finished || !m_worker) {
        return;
    }

    if (m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, "cancel", Qt::QueuedConnection);
    }
}

void DownloadTask::cleanupThread()
{
    if (!m_thread) {
        return;
    }

    if (m_thread->isRunning() && m_worker) {
        // Worker-owned objects must be destroyed in the worker thread.
        QMetaObject::invokeMethod(m_worker, "shutdown", Qt::BlockingQueuedConnection);
    }

    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }

    delete m_worker;
    m_worker = nullptr;
    delete m_thread;
    m_thread = nullptr;
}

#include "DownloadTask.moc"
