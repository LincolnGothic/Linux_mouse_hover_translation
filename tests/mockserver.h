// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <memory>

class MockServer : public QObject {
    Q_OBJECT
public:
    explicit MockServer(QObject *parent = nullptr) : QObject(parent)
    {
        server.listen(QHostAddress::LocalHost, 0);
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                    *buffer += socket->readAll();
                    if (!buffer->contains("\r\n\r\n") || socket->property("answered").toBool()) return;
                    socket->setProperty("answered", true);
                    const auto path = buffer->split(' ').value(1);
                    const QUrl request = QUrl::fromEncoded(path);
                    requests << request;
                    const QUrlQuery query(request);
                    const QString from = query.queryItemValue("from", QUrl::FullyDecoded);
                    const auto body = malformed ? QByteArray("not json")
                        : QJsonDocument(QJsonObject{{"translated-text", from == "en" ? QString("你好，世界") : QString("Hello world")},
                            {"detected", from}}).toJson(QJsonDocument::Compact);
                    const int code = httpStatus;
                    QPointer<QTcpSocket> guarded(socket);
                    QTimer::singleShot(delayMs, this, [guarded, body, code] {
                        if (!guarded || guarded->state() != QAbstractSocket::ConnectedState) return;
                        guarded->write("HTTP/1.1 " + QByteArray::number(code) + " Response\r\n"
                            "Content-Type: application/json; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                            + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                        guarded->disconnectFromHost();
                    });
                    emit received();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
            }
        });
    }
    QString url() const { return "http://127.0.0.1:" + QString::number(server.serverPort()); }
    QTcpServer server;
    QList<QUrl> requests;
    int delayMs = 0;
    int httpStatus = 200;
    bool malformed = false;
signals:
    void received();
};
