// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include "onlinetranslator.h"

class TranslationService : public QObject {
    Q_OBJECT
public:
    explicit TranslationService(QObject *parent = nullptr);
    void translate(quint64 request, const QString &text, const QString &source,
                   const QString &target, const QString &instance);
    void cancel();
    void setTimeoutMs(int timeout) { m_timeoutMs = timeout; }
signals:
    void translated(quint64 request, const QString &text);
    void failed(quint64 request, const QString &error);
private:
    QPointer<OnlineTranslator> m_translator;
    QTimer m_timeout;
    int m_timeoutMs = 10000;
    quint64 m_request = 0;
};
