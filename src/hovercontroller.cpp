// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include <QCursor>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <cstdlib>
#include <xcb/xcb.h>

namespace {
QRect windowUnderPointer()
{
    auto *connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(connection)) { xcb_disconnect(connection); return {}; }
    const auto screen = xcb_setup_roots_iterator(xcb_get_setup(connection));
    if (!screen.rem) { xcb_disconnect(connection); return {}; }
    const auto root = screen.data->root;
    auto *pointer = xcb_query_pointer_reply(connection, xcb_query_pointer(connection, root), nullptr);
    const xcb_window_t window = pointer ? pointer->child : xcb_window_t(XCB_WINDOW_NONE);
    std::free(pointer);
    QRect bounds;
    if (window != XCB_WINDOW_NONE) {
        auto *geometry = xcb_get_geometry_reply(connection, xcb_get_geometry(connection, window), nullptr);
        auto *origin = xcb_translate_coordinates_reply(connection,
            xcb_translate_coordinates(connection, window, root, 0, 0), nullptr);
        if (geometry && origin)
            bounds = QRect(origin->dst_x, origin->dst_y, geometry->width, geometry->height);
        std::free(geometry);
        std::free(origin);
    }
    xcb_disconnect(connection);
    return bounds;
}
}

HoverController::HoverController(TesseractOcr *ocr, TranslationService *translator, QObject *parent)
    : QObject(parent), m_ocr(ocr), m_translator(translator),
      m_popup(new TranslationPopup), m_escape(new X11Escape(this))
{
    m_clock.start();
    m_poll.setInterval(50);
    connect(&m_poll, &QTimer::timeout, this, &HoverController::poll);
    connect(m_escape, &X11Escape::pressed, this, [this] {
        m_policy.dismiss();
        invalidate();
        // Keep this hover marked as attempted, so Esc does not immediately
        // recreate the same popup. Moving the pointer re-arms the policy.
    });
    connect(m_ocr, &TesseractOcr::linesRecognized, this, [this](const QVector<OcrLine> &lines) {
        if (!m_haveOcr || !current(m_ocrGeneration)) return;
        m_haveOcr = false;
        m_sourceText = HoverPolicy::lineAt(lines, m_imagePointer);
        const QString source = HoverPolicy::sourceLanguage(m_sourceText);
        if (m_sourceText.isEmpty() || source.isEmpty() || source == m_settings.target) {
            emit statusChanged(tr("Ready — hover over text in the other language."));
            return;
        }
        m_pendingKey = cacheKey(m_sourceText, source);
        if (const auto *cached = m_cache.object(m_pendingKey)) {
            showResult(*cached);
            return;
        }
        m_translationGeneration = m_policy.generation();
        emit statusChanged(tr("Translating…"));
        m_translator->translate(m_translationGeneration, m_sourceText, source,
                                m_settings.target, m_settings);
    });
    connect(m_ocr, &TesseractOcr::failed, this, [this](const QString &error) {
        if (m_haveOcr && current(m_ocrGeneration)) emit statusChanged(error);
        m_haveOcr = false;
    });
    connect(m_ocr, &TesseractOcr::canceled, this, [this] { m_haveOcr = false; });
    connect(m_translator, &TranslationService::translated, this, [this](quint64 request, const QString &text) {
        if (request != m_translationGeneration || !current(request)) return;
        m_cache.insert(m_pendingKey, new QString(text));
        showResult(text);
    });
    connect(m_translator, &TranslationService::failed, this, [this](quint64 request, const QString &error) {
        if (request == m_translationGeneration && current(request)) showResult(error, true);
    });
}

HoverController::~HoverController()
{
    m_poll.stop();
    invalidate();
    delete m_popup;
}

QString HoverController::platformProblem()
{
    if (QGuiApplication::platformName() != "xcb"
        || qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland"
        || qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
        return tr("Automatic hover requires X11/Xorg. On Wayland, use Translate screen region instead.");
    const auto screens = QGuiApplication::screens();
    if (screens.size() != 1 || !qFuzzyCompare(screens.first()->devicePixelRatio(), 1.0))
        return tr("This first version supports one monitor at 100% scaling.");
    return {};
}

bool HoverController::configure(const HoverSettings &settings, QString *error)
{
    invalidate();
    m_poll.stop();
    m_enabled = false;
    m_policy.setDwellMs(settings.dwellMs);
    m_cache.clear();
    m_settings = settings;
    QString problem;
    const bool valid = SettingsStore::validate(settings, &problem);
    if (valid && settings.enabled) {
        problem = platformProblem();
        if (problem.isEmpty() && !m_ocr->init("eng+chi_sim", settings.tessdataPath.toUtf8()))
            problem = tr("Install the English and Simplified Chinese Tesseract models, then enable hover again.");
    }
    if (error) *error = problem;
    if (!problem.isEmpty()) { emit statusChanged(problem); return false; }
    m_enabled = settings.enabled;
    if (m_enabled) m_poll.start();
    emit statusChanged(m_enabled ? tr("Ready — hover over text in the other language.") : tr("Hover translation is paused."));
    return true;
}

void HoverController::setIgnoredWidgets(const QList<QWidget *> &widgets)
{
    m_ignored.clear();
    for (auto *widget : widgets) m_ignored << widget;
}

bool HoverController::blocked(QPoint pointer) const
{
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return true;
    if (m_popup->isVisible() && m_popup->frameGeometry().contains(pointer)) return true;
    for (const auto &widget : m_ignored)
        if (widget && widget->isVisible() && widget->frameGeometry().contains(pointer))
            return true;
    return false;
}

bool HoverController::current(quint64 generation) const
{
    return m_enabled && generation == m_policy.generation()
        && !HoverPolicy::moved(QCursor::pos(), m_capturePointer) && !blocked(QCursor::pos());
}

void HoverController::invalidate()
{
    m_haveOcr = false;
    m_capturePending = false;
    m_ocr->cancel();
    m_translator->cancel();
    m_popup->hide();
    m_escape->release();
}

void HoverController::poll()
{
    const auto action = m_policy.update(QCursor::pos(), blocked(QCursor::pos()), m_clock.elapsed());
    if (action == HoverPolicy::Invalidated) invalidate();
    if (action == HoverPolicy::Capture) m_capturePending = true;
    // If a canceled OCR job is finishing, retain the newest capture until it
    // can start; do not block the UI or lose this hover.
    if (m_capturePending && !m_ocr->isBusy()) {
        m_capturePending = false;
        capture();
    }
}

void HoverController::capture()
{
    if (!m_enabled || blocked(QCursor::pos())) return;
    m_capturePointer = QCursor::pos();
    auto *screen = QGuiApplication::primaryScreen();
    QRect crop = QRect(m_capturePointer - QPoint(350, 80), QSize(700, 160))
        .intersected(screen->geometry());
    // Keep the OCR crop inside the hovered window. Dark desktop borders and
    // adjacent windows can otherwise disrupt sparse-text segmentation.
    const auto window = windowUnderPointer();
    if (window.contains(m_capturePointer)) crop = crop.intersected(window);
    const QImage image = screen->grabWindow(0, crop.x(), crop.y(), crop.width(), crop.height()).toImage();
    if (image.isNull()) {
        emit statusChanged(tr("Could not capture this screen area."));
        return;
    }
    m_imagePointer = m_capturePointer - crop.topLeft();
    m_ocrGeneration = m_policy.generation();
    m_haveOcr = true;
    emit statusChanged(tr("Reading the line under the pointer…"));
    m_ocr->recognize(image, 96);
}

QString HoverController::cacheKey(const QString &text, const QString &source) const
{
    return m_settings.provider + QChar(0) + m_settings.instance + QChar(0)
        + m_settings.pythonPath + QChar(0) + m_settings.packagesPath + QChar(0)
        + m_settings.dictionaryPath + QString::number(m_settings.useDictionary) + QChar(0)
        + source + QChar(0) + m_settings.target + QChar(0) + text;
}

void HoverController::showResult(const QString &text, bool error)
{
    m_popup->showTranslation(m_capturePointer, m_sourceText, text, m_settings.target, error);
    m_escape->grab();
    emit statusChanged(error ? text : tr("Ready — move the pointer to translate another line."));
    emit popupShown(m_sourceText, text);
}
