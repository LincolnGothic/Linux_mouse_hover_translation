// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>
#include <QVariantMap>
#ifndef Q_OS_WIN
#include <QDBusMessage>
#endif

class PortalCapture : public QObject {
    Q_OBJECT
public:
    explicit PortalCapture(QObject *parent = nullptr);
    void capture();
    void cancel();
    void setTimeoutMs(int timeout) { m_timeout.setInterval(timeout); }
signals:
    void captured(const QImage &image);
    void failed(const QString &message);
    void cancelled();
#ifndef Q_OS_WIN
private slots:
    void response(uint code, const QVariantMap &results, const QDBusMessage &message);
#endif
private:
#ifdef Q_OS_WIN
    quint64 m_generation = 0;
#else
    QString m_path;
#endif
    QTimer m_timeout;
};
