// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gnomehover.h"
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QImageReader>
#include <QBuffer>
#include <cmath>

namespace {
const QString service = QStringLiteral("io.github.LincolnGothic.HoverTranslate.Gnome");
const QString path = QStringLiteral("/io/github/LincolnGothic/HoverTranslate/Gnome");
QDBusMessage method(const QString &name)
{
    return QDBusMessage::createMethodCall(service, path, service, name);
}
}

GnomeHover::GnomeHover(QObject *parent) : QObject(parent),
    m_watcher(service, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this)
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(service, path, service, "Capture", this, SLOT(capture(uint,QByteArray,double,double)));
    bus.connect(service, path, service, "Invalidated", this, SLOT(invalidated(uint)));
    bus.connect(service, path, service, "Problem", this, SLOT(problem(uint,QString)));
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &owner) {
        cancel(); m_enabled = false;
        if (!owner.isEmpty() && m_settings.enabled) configure(m_settings);
        else emit statusChanged(tr("GNOME hover is waiting for the extension."));
        emit availabilityChanged();
    });
    connect(&m_ocr, &TesseractOcr::linesRecognized, this, [this](const QVector<OcrLine> &lines) {
        if (m_enabled && m_ocrEpoch == m_epoch) {
            m_source = HoverPolicy::lineAt(lines, m_point);
            const auto source = HoverPolicy::sourceLanguage(m_source);
            if (!m_source.isEmpty() && !source.isEmpty() && source != m_settings.target) {
                m_key = source + QChar(0) + m_settings.target + QChar(0) + m_source;
                if (const auto *cached = m_cache.object(m_key)) result(m_source, *cached);
                else {
                    emit statusChanged(tr("Translating the line under the pointer…"));
                    m_translator.translate(m_epoch, m_source, source, m_settings.target, m_settings);
                }
            } else emit statusChanged(tr("Ready — hover over readable text in the other language."));
        }
        startPending();
    });
    connect(&m_ocr, &TesseractOcr::failed, this, [this](const QString &error) {
        if (m_enabled && m_ocrEpoch == m_epoch) result({}, error, true);
        startPending();
    });
    connect(&m_ocr, &TesseractOcr::canceled, this, &GnomeHover::startPending);
    connect(&m_translator, &TranslationService::translated, this, [this](quint64 epoch, const QString &text) {
        if (!m_enabled || epoch != m_epoch) return;
        m_cache.insert(m_key, new QString(text)); result(m_source, text);
    });
    connect(&m_translator, &TranslationService::failed, this, [this](quint64 epoch, const QString &error) {
        if (m_enabled && epoch == m_epoch) result(m_source, error, true);
    });
}

GnomeHover::~GnomeHover()
{
    cancel();
    if (m_enabled) {
        auto message = method("Configure"); message << false << m_settings.dwellMs;
        QDBusConnection::sessionBus().asyncCall(message, 1000);
    }
}

bool GnomeHover::available()
{
    auto *interface = QDBusConnection::sessionBus().interface();
    return interface && interface->isServiceRegistered(service).value();
}

bool GnomeHover::configure(const HoverSettings &settings, QString *error)
{
    cancel(); m_cache.clear(); m_settings = settings;
    QString failure;
    if (settings.enabled && !available()) failure = tr("Install and enable the Hover Translate GNOME extension, then reopen the app.");
    else if (settings.enabled && !m_ocr.init("eng+chi_sim", settings.tessdataPath.toUtf8()))
        failure = tr("Install tesseract-ocr-eng and tesseract-ocr-chi-sim before enabling hover.");
    m_enabled = settings.enabled && failure.isEmpty();
    if (error) *error = failure;
    if (!failure.isEmpty()) { emit statusChanged(failure); return false; }
    if (available()) {
        auto message = method("Configure"); message << m_enabled << settings.dwellMs;
        const auto epoch = m_epoch;
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, epoch](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<> reply = *call; call->deleteLater();
            if (reply.isError() && epoch == m_epoch) {
                cancel(); m_enabled = false;
                emit statusChanged(tr("Could not enable GNOME hover: ") + reply.error().message());
                emit availabilityChanged();
            }
        });
    }
    emit statusChanged(m_enabled ? tr("Ready — hover over text. GNOME captures the line locally.") : tr("Hover translation is paused."));
    return true;
}

void GnomeHover::cancel()
{
    ++m_epoch; m_pending.reset(); m_ocr.cancel(); m_translator.cancel();
}

void GnomeHover::invalidated(uint token)
{
    cancel(); m_token = token;
}

void GnomeHover::problem(uint token, const QString &message)
{
    if (m_enabled && token == m_token) emit statusChanged(message);
}

void GnomeHover::capture(uint token, const QByteArray &png, double x, double y)
{
    if (!m_enabled || token != m_token || png.size() > 8 * 1024 * 1024 || !std::isfinite(x) || !std::isfinite(y)
        || x < 0 || y < 0 || x >= 1 || y >= 1) return;
    QBuffer buffer; buffer.setData(png); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 16000000) return;
    const auto image = reader.read();
    if (image.isNull()) return;
    cancel(); m_token = token;
    m_pending = Pending{image, QPoint(int(x * image.width()), int(y * image.height())), m_epoch};
    startPending();
}

void GnomeHover::startPending()
{
    if (!m_enabled || !m_pending || m_ocr.isBusy()) return;
    auto pending = std::move(*m_pending); m_pending.reset();
    m_ocrEpoch = pending.epoch; m_point = pending.point;
    emit statusChanged(tr("Reading the line under the pointer…"));
    m_ocr.recognize(pending.image, 96);
}

void GnomeHover::result(const QString &source, const QString &text, bool error)
{
    if (!m_enabled) return;
    auto message = method("Result"); message << m_token << source << text << error;
    QDBusConnection::sessionBus().asyncCall(message, 2000);
    emit statusChanged(error ? text : tr("Ready — move the pointer to translate another line."));
    emit popupShown(source, text);
}
