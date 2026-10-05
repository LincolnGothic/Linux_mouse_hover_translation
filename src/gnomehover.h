// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings.h"
#include "tesseractocr.h"
#include "translationservice.h"
#include <QCache>
#include <QDBusServiceWatcher>
#include <optional>

class GnomeHover : public QObject {
    Q_OBJECT
public:
    explicit GnomeHover(QObject *parent = nullptr);
    ~GnomeHover() override;
    static bool available();
    bool configure(const HoverSettings &settings, QString *error = nullptr);
    bool enabled() const { return m_enabled; }
signals:
    void statusChanged(const QString &message);
    void popupShown(const QString &source, const QString &translation);
    void availabilityChanged();
private slots:
    void capture(uint token, const QByteArray &png, double x, double y);
    void invalidated(uint token);
    void problem(uint token, const QString &message);
private:
    struct Pending { QImage image; QPoint point; quint64 epoch; };
    void startPending();
    void cancel();
    void result(const QString &source, const QString &text, bool error = false);
    HoverSettings m_settings;
    TesseractOcr m_ocr;
    TranslationService m_translator;
    QDBusServiceWatcher m_watcher;
    QCache<QString, QString> m_cache{128};
    std::optional<Pending> m_pending;
    QPoint m_point;
    QString m_source, m_key;
    quint64 m_epoch = 0, m_ocrEpoch = 0;
    uint m_token = 0;
    bool m_enabled = false;
};
