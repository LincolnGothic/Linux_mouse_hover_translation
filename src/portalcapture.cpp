// SPDX-License-Identifier: GPL-3.0-or-later
#include "portalcapture.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QImageReader>
#include <QUrl>
#include <QUuid>

static const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
static const QString requestInterface = QStringLiteral("org.freedesktop.portal.Request");

PortalCapture::PortalCapture(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(180000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        cancel(); emit failed(tr("Screen capture timed out. Try Translate screen region again."));
    });
}

void PortalCapture::cancel()
{
    m_timeout.stop();
    if (m_path.isEmpty()) return;
    const QString path = m_path; m_path.clear();
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(service, path, requestInterface, "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)));
    auto close = QDBusMessage::createMethodCall(service, path, requestInterface, "Close");
    bus.asyncCall(close);
}

void PortalCapture::capture()
{
    cancel();
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) { emit failed(tr("The desktop session bus is unavailable.")); return; }
    QString sender = bus.baseService().mid(1); sender.replace('.', '_');
    const QString token = "hover_" + QUuid::createUuid().toString(QUuid::Id128);
    m_path = "/org/freedesktop/portal/desktop/request/" + sender + '/' + token;
    if (!bus.connect(service, m_path, requestInterface, "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)))) {
        m_path.clear(); emit failed(tr("Could not connect to the desktop capture service.")); return;
    }
    const QString expected = m_path;
    auto message = QDBusMessage::createMethodCall(service, "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot", "Screenshot");
    message << QString() << QVariantMap{{"handle_token", token}, {"interactive", true}, {"modal", false}};
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 10000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, expected](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        call->deleteLater();
        if (m_path != expected) return;
        if (reply.isError()) {
            cancel();
            emit failed(tr("Desktop capture is unavailable. Install xdg-desktop-portal and your desktop's portal backend, then sign in again. Details: ") + reply.error().message());
        } else if (reply.value().path() != expected) {
            auto bus = QDBusConnection::sessionBus();
            bus.disconnect(service, expected, requestInterface, "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)));
            m_path = reply.value().path();
            bus.connect(service, m_path, requestInterface, "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)));
        }
    });
    m_timeout.start();
}

void PortalCapture::response(uint code, const QVariantMap &results, const QDBusMessage &message)
{
    if (m_path.isEmpty() || message.path() != m_path) return;
    const QString path = m_path; m_path.clear(); m_timeout.stop();
    QDBusConnection::sessionBus().disconnect(service, path, requestInterface, "Response", this, SLOT(response(uint,QVariantMap,QDBusMessage)));
    if (code == 1) { emit cancelled(); return; }
    if (code != 0) { emit failed(tr("The desktop could not capture the screen.")); return; }
    const QUrl url(results.value("uri").toString());
    if (!url.isLocalFile()) { emit failed(tr("The desktop returned an invalid screenshot location.")); return; }
    QImageReader reader(url.toLocalFile());
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) {
        emit failed(tr("The screenshot is too large or unreadable. Select a smaller area.")); return;
    }
    const QImage image = reader.read();
    if (image.isNull()) { emit failed(tr("Could not open the desktop screenshot.")); return; }
    emit captured(image);
}
