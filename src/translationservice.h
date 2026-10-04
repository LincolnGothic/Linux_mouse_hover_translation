// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QProcess>
#include "settings.h"
#include "onlinetranslator.h"

class TranslationService : public QObject {
    Q_OBJECT
public:
    explicit TranslationService(QObject *parent = nullptr);
    ~TranslationService() override;
    void translate(quint64 request, const QString &text, const QString &source,
                   const QString &target, const HoverSettings &settings);
    void translate(quint64 request, const QString &text, const QString &source,
                   const QString &target, const QString &instance);
    void cancel();
    void resetOffline() { cancel(); stopWorker(); }
    void setTimeoutMs(int timeout) { m_timeoutMs = timeout; }
    void setOfflineTimeoutMs(int timeout) { m_offlineTimeoutMs = timeout; }
signals:
    void translated(quint64 request, const QString &text);
    void failed(quint64 request, const QString &error);
private:
    void readOfflineOutput();
    void sendOfflineRequest();
    void stopWorker();
    QPointer<OnlineTranslator> m_translator;
    QProcess m_worker;
    QByteArray m_output, m_pending;
    QString m_workerKey;
    quint64 m_serial = 0;
    bool m_active = false, m_ready = false, m_busy = false;
    QTimer m_timeout;
    int m_timeoutMs = 10000;
    int m_offlineTimeoutMs = 120000;
    quint64 m_request = 0;
};
