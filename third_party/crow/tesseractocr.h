/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Modified 2026-10-04 for Hover Translate: expose line geometry, use a
 * cancellation flag and a future watcher, and decouple Crow's settings UI.
 */
#pragma once
#include "aocrprovider.h"
#include "hoverpolicy.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <QVariant>
#include <atomic>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

class TesseractOcr : public AOcrProvider {
    Q_OBJECT
public:
    explicit TesseractOcr(QObject *parent = nullptr);
    ~TesseractOcr() override;
    bool init(const QByteArray &languages, const QByteArray &languagesPath,
              const QMap<QString, QVariant> &parameters = {});
    QString engineName() const override { return QStringLiteral("tesseract"); }
    bool isConfigured() const override { return !m_languages.isEmpty(); }
    bool isBusy() const { return m_busy; }
    void recognize(const QImage &image, int dpi) override;
    void cancel() override;
signals:
    void linesRecognized(const QVector<OcrLine> &lines);
private:
    tesseract::TessBaseAPI m_tesseract;
    tesseract::ETEXT_DESC m_monitor;
    std::atomic_bool m_canceled{false};
    QFuture<OcrResult> m_future;
    QFutureWatcher<OcrResult> m_watcher;
    QByteArray m_languages, m_path;
    bool m_busy = false;
};
