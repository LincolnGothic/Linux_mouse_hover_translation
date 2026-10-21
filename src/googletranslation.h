// SPDX-FileCopyrightText: 2026 Hover Translate contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>

// Experimental keyless transport; this is not the Google Cloud Translation API.
QUrl googleTranslationUrl(const QString &text, const QString &source, const QString &target);
QStringList googleTranslationChunks(const QString &text, const QString &source, const QString &target);
QString parseGoogleTranslation(const QByteArray &body, QString *error);

class GoogleTranslation : public QObject {
    Q_OBJECT
public:
    explicit GoogleTranslation(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~GoogleTranslation() override;
    void start(const QString &text, const QString &source, const QString &target, int timeoutMs);
    void cancel();
signals:
    void translated(const QString &text);
    void failed(const QString &error);
    void requestSubmitted(qint64 characters);
private:
    void submit();
    void finish(const QString &error);
    void clearReply();
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout, m_retry;
    QStringList m_chunks;
    QString m_source, m_target, m_output;
    QByteArray m_received;
    int m_chunk = 0, m_attempt = 0, m_timeoutMs = 10000;
    bool m_active = false;
};
