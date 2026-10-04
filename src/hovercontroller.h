// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hoverpolicy.h"
#include "settings.h"
#include "translationpopup.h"
#include "tesseractocr.h"
#include "translationservice.h"
#include "x11escape.h"
#include <QCache>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

class HoverController : public QObject {
    Q_OBJECT
public:
    HoverController(TesseractOcr *ocr, TranslationService *translator, QObject *parent = nullptr);
    ~HoverController() override;
    bool configure(const HoverSettings &settings, QString *error = nullptr);
    void setIgnoredWidgets(const QList<QWidget *> &widgets);
    bool enabled() const { return m_enabled; }
    TranslationPopup *popup() const { return m_popup; }
    static QString platformProblem();
signals:
    void statusChanged(const QString &message);
    void popupShown(const QString &source, const QString &translation);
private:
    void poll();
    void invalidate();
    void capture();
    bool current(quint64 generation) const;
    bool blocked(QPoint pointer) const;
    QString cacheKey(const QString &text, const QString &source) const;
    void showResult(const QString &text, bool error = false);
    HoverPolicy m_policy;
    HoverSettings m_settings;
    TesseractOcr *m_ocr;
    TranslationService *m_translator;
    TranslationPopup *m_popup;
    X11Escape *m_escape;
    QTimer m_poll;
    QElapsedTimer m_clock;
    QList<QPointer<QWidget>> m_ignored;
    QCache<QString, QString> m_cache{128};
    QPoint m_capturePointer, m_imagePointer;
    quint64 m_ocrGeneration = 0, m_translationGeneration = 0;
    QString m_sourceText, m_pendingKey;
    bool m_enabled = false;
    bool m_haveOcr = false;
    bool m_capturePending = false;
};
