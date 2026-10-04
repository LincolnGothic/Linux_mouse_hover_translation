// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>
#include <QVariantMap>
#include <QDBusMessage>

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
private slots:
    void response(uint code, const QVariantMap &results, const QDBusMessage &message);
private:
    QString m_path;
    QTimer m_timeout;
};
