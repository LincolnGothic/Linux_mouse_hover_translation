/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Modified 2026-10-04: see the header and docs/UPSTREAM.md.
 */
#include "tesseractocr.h"
#include <QtConcurrent>
#include <algorithm>
#include <memory>
#include <tesseract/resultiterator.h>

TesseractOcr::TesseractOcr(QObject *parent) : AOcrProvider(parent)
{
    m_monitor.cancel_this = &m_canceled;
    m_monitor.cancel = [](void *flag, int) {
        return static_cast<std::atomic_bool *>(flag)->load();
    };
    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, [this] {
        const auto result = m_future.result();
        m_busy = false;
        if (result.canceled || m_canceled.load()) emit canceled();
        else if (!result.error.isEmpty()) emit failed(result.error);
        else {
            emit linesRecognized(result.lines);
            QStringList text;
            for (const auto &line : result.lines) text << line.text;
            emit recognized(text.join('\n'));
        }
    });
}

TesseractOcr::~TesseractOcr()
{
    cancel();
    m_future.waitForFinished();
}

bool TesseractOcr::init(const QByteArray &languages, const QByteArray &path,
                       const QMap<QString, QVariant> &parameters)
{
    if (isBusy()) return m_languages == languages && m_path == path && parameters.isEmpty();
    if (m_languages != languages || m_path != path || !isConfigured()) {
        m_tesseract.End();
        m_languages.clear();
        if (m_tesseract.Init(path.isEmpty() ? nullptr : path.constData(),
                            languages.constData(), tesseract::OEM_LSTM_ONLY) != 0)
            return false;
        std::vector<std::string> loaded;
        m_tesseract.GetLoadedLanguagesAsVector(&loaded);
        for (const auto &required : languages.split('+'))
            if (std::find(loaded.cbegin(), loaded.cend(), required.toStdString()) == loaded.cend()) {
                m_tesseract.End();
                return false;
            }
        m_languages = languages;
        m_path = path;
    }
    m_tesseract.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        m_tesseract.SetVariable(it.key().toUtf8().constData(), it.value().toByteArray().constData());
    return true;
}

void TesseractOcr::recognize(const QImage &source, int dpi)
{
    if (!isConfigured() || source.isNull()) {
        emit failed(tr("OCR needs a valid image and the English and Simplified Chinese models."));
        return;
    }
    if (isBusy()) return;
    const QImage image = source.convertToFormat(QImage::Format_RGB888);
    m_canceled.store(false);
    m_busy = true;
    emit started();
    m_future = QtConcurrent::run([this, image, dpi] {
        OcrResult result;
        m_tesseract.SetImage(image.constBits(), image.width(), image.height(), 3, image.bytesPerLine());
        m_tesseract.SetSourceResolution(dpi);
        if (m_tesseract.Recognize(&m_monitor) != 0) {
            result.canceled = m_canceled.load();
            if (!result.canceled) result.error = tr("Could not recognize this screen area.");
            return result;
        }
        if (m_canceled.load()) { result.canceled = true; return result; }
        const std::unique_ptr<tesseract::ResultIterator> iterator(m_tesseract.GetIterator());
        if (!iterator) return result;
        do {
            int left, top, right, bottom;
            if (!iterator->BoundingBox(tesseract::RIL_TEXTLINE, &left, &top, &right, &bottom))
                continue;
            const std::unique_ptr<char[]> text(iterator->GetUTF8Text(tesseract::RIL_TEXTLINE));
            if (text)
                result.lines.append({QString::fromUtf8(text.get()).trimmed(),
                    QRect(left, top, right - left, bottom - top),
                    iterator->Confidence(tesseract::RIL_TEXTLINE)});
        } while (iterator->Next(tesseract::RIL_TEXTLINE));
        return result;
    });
    m_watcher.setFuture(m_future);
}

void TesseractOcr::cancel() { m_canceled.store(true); }
