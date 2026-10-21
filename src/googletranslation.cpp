// SPDX-FileCopyrightText: 2026 Hover Translate contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "googletranslation.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkRequest>

namespace {
constexpr int maximumUrl = 1800; // Local encoded URL budget, not a Google quota.
constexpr int maximumResponse = 2 * 1024 * 1024;
}

QUrl googleTranslationUrl(const QString &text, const QString &source, const QString &target)
{
    return QUrl::fromEncoded("https://translate.googleapis.com/translate_a/single?client=gtx&sl="
        + QUrl::toPercentEncoding(source) + "&tl=" + QUrl::toPercentEncoding(target)
        + "&dt=t&q=" + QUrl::toPercentEncoding(text));
}

QStringList googleTranslationChunks(const QString &text, const QString &source, const QString &target)
{
    const auto points = text.toStdU32String();
    const auto length = qsizetype(points.size());
    QStringList chunks;
    for (qsizetype start = 0; start < length;) {
        qsizetype low = 1, high = qMin(qsizetype(maximumUrl), length - start);
        while (low <= high) {
            const auto middle = (low + high) / 2;
            const auto candidate = QString::fromUcs4(points.data() + start, middle);
            if (googleTranslationUrl(candidate, source, target).toEncoded().size() <= maximumUrl) low = middle + 1;
            else high = middle - 1;
        }
        qsizetype count = qMax(qsizetype(1), high);
        if (start + count < length) {
            for (qsizetype i = count; i > count / 2; --i) {
                const auto ch = points[start + i - 1];
                if (QChar::isSpace(ch) || ch == U'.' || ch == U'!' || ch == U'?' ||
                    ch == U'。' || ch == U'！' || ch == U'？') { count = i; break; }
            }
        }
        chunks.append(QString::fromUcs4(points.data() + start, count));
        start += count;
    }
    return chunks;
}

QString parseGoogleTranslation(const QByteArray &body, QString *error)
{
    if (error) error->clear();
    if (body.size() > maximumResponse) {
        if (error) *error = QObject::tr("Google Translate returned a response that is too large.");
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(body, &parseError);
    QString output;
    bool valid = parseError.error == QJsonParseError::NoError && document.isArray()
        && !document.array().isEmpty() && document.array().at(0).isArray();
    if (valid) {
        const auto segments = document.array().at(0).toArray();
        valid = !segments.isEmpty();
        for (const auto &segment : segments) {
            if (!segment.isArray() || segment.toArray().isEmpty() || !segment.toArray().at(0).isString()) { valid = false; break; }
            output += segment.toArray().at(0).toString();
        }
    }
    if (!valid || output.trimmed().isEmpty()) {
        if (error) *error = QObject::tr("Google Translate returned an invalid or empty response. Try again later.");
        return {};
    }
    return output;
}

GoogleTranslation::GoogleTranslation(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent), m_network(network)
{
    m_timeout.setSingleShot(true); m_retry.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish(tr("Google Translate timed out. Check your connection and try again."));
    });
    connect(&m_retry, &QTimer::timeout, this, &GoogleTranslation::submit);
}

GoogleTranslation::~GoogleTranslation() { cancel(); }

void GoogleTranslation::clearReply()
{
    m_timeout.stop();
    if (!m_reply) return;
    auto *reply = m_reply.data(); m_reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort(); reply->deleteLater();
}

void GoogleTranslation::cancel()
{
    m_active = false; m_retry.stop(); clearReply();
}

void GoogleTranslation::finish(const QString &error)
{
    cancel();
    emit failed(error);
}

void GoogleTranslation::start(const QString &text, const QString &source, const QString &target, int timeoutMs)
{
    cancel();
    if (text.trimmed().isEmpty() || text.toUcs4().size() > 10000 ||
        (source != "en" && source != "zh-CN") || (target != "en" && target != "zh-CN")) {
        emit failed(tr("Use English or Simplified Chinese text, up to 10,000 characters.")); return;
    }
    if (source == target) { emit translated(text); return; }
    m_chunks = googleTranslationChunks(text, source, target);
    m_source = source; m_target = target; m_output.clear();
    m_chunk = 0; m_attempt = 0; m_timeoutMs = qMax(1, timeoutMs); m_active = true;
    submit();
}

void GoogleTranslation::submit()
{
    if (!m_active) return;
    while (m_chunk < m_chunks.size() && m_chunks[m_chunk].trimmed().isEmpty())
        m_output += m_chunks[m_chunk++];
    if (m_chunk == m_chunks.size()) {
        m_active = false; emit translated(m_output); return;
    }
    const auto &chunk = m_chunks[m_chunk];
    emit requestSubmitted(chunk.toUcs4().size());
    if (!m_active) return;
    QNetworkRequest request(googleTranslationUrl(chunk, m_source, m_target));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("User-Agent", "HoverTranslate/0.7.0");
    request.setRawHeader("Accept", "application/json");
    m_received.clear();
    auto *reply = m_network->get(request); m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (!m_active || m_reply != reply) return;
        m_received += reply->readAll();
        if (m_received.size() > maximumResponse)
            finish(tr("Google Translate returned a response that is too large."));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (!m_active || m_reply != reply) return;
        m_received += reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        clearReply();
        // One bounded retry for transient failures. Never retry access blocks,
        // certificate errors, redirects or rate limits, or switch providers.
        if (m_attempt == 0 && (status == 502 || status == 503 || status == 504 ||
            (!status && (networkError == QNetworkReply::TemporaryNetworkFailureError ||
                         networkError == QNetworkReply::RemoteHostClosedError)))) {
            ++m_attempt; m_retry.start(500); return;
        }
        QString error;
        if (status == 429) error = tr("Google Translate rate limit reached. Wait a while before trying again.");
        else if (status == 403) error = tr("Google Translate blocked this request. The free experimental service may be unavailable. Try again later.");
        else if (status && status != 200) error = tr("Google Translate is unavailable. Try again later.");
        else if (!status || networkError != QNetworkReply::NoError)
            error = tr("Google Translate network request failed. Check your connection and try again.");
        QString result;
        if (error.isEmpty()) result = parseGoogleTranslation(m_received, &error);
        if (!error.isEmpty()) { finish(error); return; }
        const auto &chunk = m_chunks[m_chunk];
        qsizetype first = 0, last = chunk.size();
        while (first < last && chunk[first].isSpace()) ++first;
        while (last > first && chunk[last - 1].isSpace()) --last;
        m_output += chunk.left(first) + result.trimmed() + chunk.mid(last);
        ++m_chunk; m_attempt = 0;
        submit();
    });
    m_timeout.start(m_timeoutMs);
}
