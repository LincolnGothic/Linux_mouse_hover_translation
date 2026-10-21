// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include <QCursor>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <cstdlib>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <xcb/xcb.h>
#endif

namespace {
#ifdef Q_OS_WIN
bool sessionAvailable()
{
    const auto desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    wchar_t name[256]{}; DWORD length = 0;
    const bool available = desktop && GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &length)
        && QString::fromWCharArray(name).compare("Default", Qt::CaseInsensitive) == 0;
    if (desktop) CloseDesktop(desktop);
    return available;
}

HWND pointerWindow()
{
    POINT pointer{};
    return GetCursorPos(&pointer) ? GetAncestor(WindowFromPoint(pointer), GA_ROOT) : nullptr;
}

QRect windowUnderPointer()
{
    POINT pointer{}; RECT bounds{};
    const auto window = pointerWindow();
    auto *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!window || !screen || !GetCursorPos(&pointer) || !GetWindowRect(window, &bounds)) return {};
    MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromPoint(pointer, MONITOR_DEFAULTTONEAREST), &monitor)) return {};
    const auto geometry = screen->geometry();
    const qreal scaleX = qreal(geometry.width()) / (monitor.rcMonitor.right - monitor.rcMonitor.left);
    const qreal scaleY = qreal(geometry.height()) / (monitor.rcMonitor.bottom - monitor.rcMonitor.top);
    return QRectF(geometry.x() + (bounds.left - monitor.rcMonitor.left) * scaleX,
        geometry.y() + (bounds.top - monitor.rcMonitor.top) * scaleY,
        (bounds.right - bounds.left) * scaleX, (bounds.bottom - bounds.top) * scaleY).toAlignedRect();
}
#else
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
#endif
}

HoverController::HoverController(TesseractOcr *ocr, TranslationService *translator, QObject *parent)
    : QObject(parent), m_ocr(ocr), m_translator(translator),
      m_popup(new TranslationPopup), m_escape(new X11Escape(this)), m_gnome(new GnomeHover(this))
{
#ifdef Q_OS_WIN
    m_highlight.setWindowFlag(Qt::WindowTransparentForInput);
#endif
    connect(m_gnome, &GnomeHover::statusChanged, this, [this](const QString &message) {
        if (m_usingGnome) emit statusChanged(message);
    });
    connect(m_gnome, &GnomeHover::popupShown, this, &HoverController::popupShown);
    connect(m_gnome, &GnomeHover::availabilityChanged, this, &HoverController::availabilityChanged);
    connect(m_popup,&TranslationPopup::dismissed,this,[this] { m_policy.dismiss(); invalidate(); });
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
        m_sourceText = HoverPolicy::textAt(lines, m_imagePointer, m_captureMode);
        if (m_captureMode == "word") {
            const auto word = m_dictionary.chineseAt(lines, m_imagePointer);
            if (!word.isEmpty()) m_sourceText = word;
        }
        if (m_sourceText.isEmpty()) {
            m_highlight.hide();
            const bool retrying = m_policy.retry(m_clock.elapsed());
            emit statusChanged(retrying ? tr("Text was not clear yet. Retrying this hover…")
                : tr("No readable word found. Try Line mode or a screen region."));
            return;
        }
        if (m_settings.highlightSource) {
            auto boxes=HoverPolicy::sourceBounds(lines,m_imagePointer,m_sourceText);
            for (auto &box:boxes)
                box = QRectF(m_cropOrigin.x() + box.x() / m_imageScale.x(),
                    m_cropOrigin.y() + box.y() / m_imageScale.y(),
                    box.width() / m_imageScale.x(), box.height() / m_imageScale.y()).toAlignedRect();
            m_highlight.showBoxes(boxes);
        }
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
#ifdef Q_OS_WIN
    return QGuiApplication::platformName() == "windows" && !QGuiApplication::screens().isEmpty()
        ? QString() : tr("Automatic hover requires an interactive Windows desktop.");
#else
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland"
        || qEnvironmentVariableIsSet("WAYLAND_DISPLAY") || QGuiApplication::platformName() == "wayland")
        return GnomeHover::available() ? QString() : tr("Enable the Hover Translate GNOME extension for Wayland hover. Click Set up GNOME hover, then sign out and back in.");
    if (QGuiApplication::platformName() != "xcb")
        return tr("Automatic hover needs the GNOME extension on Wayland, or an X11 desktop.");
    const auto screens = QGuiApplication::screens();
    if (screens.size() != 1 || !qFuzzyCompare(screens.first()->devicePixelRatio(), 1.0))
        return tr("This first version supports one monitor at 100% scaling.");
    return {};
#endif
}

bool HoverController::configure(const HoverSettings &settings, QString *error)
{
    invalidate();
    m_poll.stop();
    m_enabled = false;
    m_policy.setDwellMs(settings.dwellMs);
    m_cache.clear();
    m_settings = settings;
    m_dictionary.load(settings.provider == "offline" && settings.useDictionary ? (settings.dictionaryPath.isEmpty() ? offlineDataDirectory() + "/cedict.u8" : settings.dictionaryPath) : QString());
    QString problem;
    const bool valid = SettingsStore::validate(settings, &problem);
#ifdef Q_OS_WIN
    const bool wayland = false;
#else
    const bool wayland = qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland"
        || qEnvironmentVariableIsSet("WAYLAND_DISPLAY") || QGuiApplication::platformName() == "wayland";
#endif
    const bool wasGnome = m_usingGnome;
    m_usingGnome = wayland && GnomeHover::available();
    if (wasGnome && !m_usingGnome) {
        auto paused = settings; paused.enabled = false; m_gnome->configure(paused);
    }
    if (valid && m_usingGnome) {
        const bool configured = m_gnome->configure(settings, &problem);
        m_enabled = configured && settings.enabled;
        if (error) *error = problem;
        return configured;
    }
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
#ifdef Q_OS_WIN
    if (!sessionAvailable()) return true;
    DWORD process = 0;
    const auto window = pointerWindow();
    if (window) GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId()) return true;
#endif
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) return true;
    if (m_popup->isVisible() && m_popup->frameGeometry().contains(pointer)) return true;
    for (const auto &widget : m_ignored)
        if (widget && widget->isVisible() && widget->frameGeometry().contains(pointer))
            return true;
    return false;
}

bool HoverController::current(quint64 generation) const
{
#ifdef Q_OS_WIN
    if (quintptr(pointerWindow()) != m_pointerWindow) return false;
#endif
    return m_enabled && generation == m_policy.generation()
        && !HoverPolicy::moved(QCursor::pos(), m_policy.anchor()) && !blocked(QCursor::pos());
}

void HoverController::invalidate()
{
    m_haveOcr = false;
    m_capturePending = false;
    m_ocr->cancel();
    m_translator->cancel();
    m_popup->dismiss(); m_highlight.hide(); m_popupGrace=-1;
    m_escape->release();
}

void HoverController::poll()
{
#ifdef Q_OS_WIN
    if (!sessionAvailable()) { m_policy.reset(); invalidate(); return; }
    const auto window = quintptr(pointerWindow());
    if (window != m_pointerWindow && !m_popup->pinned()
        && (!m_popup->isVisible() || (!m_popup->frameGeometry().contains(QCursor::pos())
            && !HoverPolicy::moved(QCursor::pos(), m_capturePointer)))) {
        m_pointerWindow = window; m_policy.reset(); invalidate();
    }
#endif
    if (m_popup->isVisible() && m_popup->pinned()) return;
    const auto modifiers=QGuiApplication::queryKeyboardModifiers();
    const auto effective = m_settings.temporaryModes && (modifiers & Qt::ShiftModifier)
        ? ((modifiers & Qt::ControlModifier) ? QString("sentence") : QString("word")) : m_settings.textMode;
    if (effective!=m_effectiveMode) { m_effectiveMode=effective; m_policy.reset(); invalidate(); }
    if (m_popup->isVisible()) {
        if (m_popup->pinned() || m_popup->frameGeometry().contains(QCursor::pos())) { m_popupGrace=-1; return; }
        if (HoverPolicy::moved(QCursor::pos(),m_capturePointer)) {
            if (m_popupGrace<0) m_popupGrace=m_clock.elapsed();
            if (m_clock.elapsed()-m_popupGrace<350) return;
        }
    }
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
    auto *screen = QGuiApplication::screenAt(m_capturePointer);
    if (!screen) return;
    QRect crop = QRect(m_capturePointer - QPoint(350, 80), QSize(700, 160))
        .intersected(screen->geometry());
    // Keep the OCR crop inside the hovered window. Dark desktop borders and
    // adjacent windows can otherwise disrupt sparse-text segmentation.
    const auto window = windowUnderPointer();
    if (window.contains(m_capturePointer)) crop = crop.intersected(window);
    if (crop.isEmpty()) return;
#ifdef Q_OS_WIN
    // A full monitor capture avoids platform-specific sub-area coordinate
    // conventions. OCR uses image pixels; pointer/highlight coordinates use Qt
    // logical pixels, including fractional scaling and nonzero monitor origins.
    const QImage desktop = screen->grabWindow(0).toImage();
    if (desktop.isNull()) { emit statusChanged(tr("Could not capture this screen area.")); return; }
    const auto geometry = screen->geometry();
    m_imageScale = QPointF(qreal(desktop.width()) / geometry.width(), qreal(desktop.height()) / geometry.height());
    const auto offset = crop.topLeft() - geometry.topLeft();
    const QRect pixels = QRectF(offset.x() * m_imageScale.x(), offset.y() * m_imageScale.y(),
        crop.width() * m_imageScale.x(), crop.height() * m_imageScale.y()).toAlignedRect().intersected(desktop.rect());
    QImage image = desktop.copy(pixels);
    // OCR preprocessing and token bounds operate on physical image pixels.
    // Retaining the screen DPR would shrink QPainter's padded OCR input.
    image.setDevicePixelRatio(1.0);
#else
    const QImage image = screen->grabWindow(0, crop.x(), crop.y(), crop.width(), crop.height()).toImage();
#endif
    if (image.isNull()) {
        emit statusChanged(tr("Could not capture this screen area."));
        return;
    }
    m_captureMode=m_effectiveMode;
#ifdef Q_OS_WIN
    m_imagePointer = QPoint(qRound((m_capturePointer.x() - geometry.x()) * m_imageScale.x()) - pixels.x(),
        qRound((m_capturePointer.y() - geometry.y()) * m_imageScale.y()) - pixels.y());
    m_cropOrigin = QPoint(qRound(geometry.x() + pixels.x() / m_imageScale.x()),
        qRound(geometry.y() + pixels.y() / m_imageScale.y()));
#else
    m_cropOrigin = crop.topLeft();
    m_imagePointer = m_capturePointer - crop.topLeft();
#endif
    m_ocrGeneration = m_policy.generation();
    m_haveOcr = true;
    emit statusChanged(tr("Reading text under the pointer…"));
    m_ocr->recognizeLayout(image, 96, m_captureMode == "sentence", m_imagePointer, m_captureMode);
}

QString HoverController::cacheKey(const QString &text, const QString &source) const
{
    return translationCacheKey(m_settings, text, source);
}

void HoverController::showResult(const QString &text, bool error)
{
    m_popupGrace=-1;
    m_popup->showTranslation(m_capturePointer, m_sourceText, text, m_settings.target, error);
    m_escape->grab();
    emit statusChanged(error ? text : tr("Ready — move the pointer to translate more text."));
    emit popupShown(m_sourceText, text);
}
