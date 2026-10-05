/*
 * SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
 * SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Modified 2026-10-05: see the header and docs/UPSTREAM.md.
 */
#include "tesseractocr.h"
#include <QtConcurrent>
#include <algorithm>
#include <memory>
#include <QPainter>
#include <cmath>
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
    recognizeLayout(source, dpi, false);
}

void TesseractOcr::recognizeLayout(const QImage &source, int dpi, bool paragraphLayout)
{
    if (!isConfigured() || source.isNull()) {
        emit failed(tr("OCR needs a valid image and the English and Simplified Chinese models."));
        return;
    }
    if (isBusy()) return;
    // Screen text is often only 9–14 pixels tall. Enlarge small captures and
    // add a margin so LSTM segmentation can see characters at crop edges.
    const int scale = dpi < 180 && qint64(source.width()) * source.height() <= 1500000 ? 2 : 1;
    const int border = 12;
    QImage image = source.convertToFormat(QImage::Format_RGB888);
    qint64 brightness = 0, samples = 0;
    for (int x = 0; x < image.width(); x += qMax(1, image.width() / 100)) {
        brightness += qGray(image.pixel(x, 0)) + qGray(image.pixel(x, image.height() - 1));
        samples += 2;
    }
    for (int y = 0; y < image.height(); y += qMax(1, image.height() / 100)) {
        brightness += qGray(image.pixel(0, y)) + qGray(image.pixel(image.width() - 1, y));
        samples += 2;
    }
    if (samples && brightness / samples < 128) image.invertPixels();
    if (scale > 1) image = image.scaled(image.size() * scale, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage padded(image.size() + QSize(border * 2, border * 2), QImage::Format_RGB888);
    padded.fill(Qt::white);
    { QPainter painter(&padded); painter.drawImage(border, border, image); }
    image = padded;
    m_canceled.store(false);
    m_busy = true;
    emit started();
    m_future = QtConcurrent::run([this, image, dpi, scale, border, originalSize = source.size(), paragraphLayout] {
        OcrResult result;
        for (const auto mode : {paragraphLayout ? tesseract::PSM_AUTO : tesseract::PSM_SPARSE_TEXT,
                                paragraphLayout ? tesseract::PSM_SPARSE_TEXT : tesseract::PSM_AUTO}) {
            m_tesseract.SetPageSegMode(mode);
            m_tesseract.SetImage(image.constBits(), image.width(), image.height(), 3, image.bytesPerLine());
            m_tesseract.SetSourceResolution(qMax(70, dpi * scale));
            if (m_tesseract.Recognize(&m_monitor) != 0) {
                result.canceled = m_canceled.load();
                if (!result.canceled) result.error = tr("Could not recognize this screen area.");
                return result;
            }
            if (m_canceled.load()) { result.canceled = true; return result; }
            QVector<OcrLine> lines;
            const std::unique_ptr<tesseract::ResultIterator> iterator(m_tesseract.GetIterator());
            auto bounds = [&](int left, int top, int right, int bottom) {
                return QRect(QPoint(int(std::floor(double(left - border) / scale)), int(std::floor(double(top - border) / scale))),
                             QPoint(int(std::ceil(double(right - border) / scale)) - 1, int(std::ceil(double(bottom - border) / scale)) - 1))
                    .intersected(QRect(QPoint(), originalSize));
            };
            int block = -1, paragraph = -1;
            if (iterator) do {
                if (iterator->IsAtBeginningOf(tesseract::RIL_BLOCK)) ++block;
                if (iterator->IsAtBeginningOf(tesseract::RIL_PARA)) ++paragraph;
                int left, top, right, bottom;
                if (!iterator->BoundingBox(tesseract::RIL_TEXTLINE, &left, &top, &right, &bottom)) continue;
                const std::unique_ptr<char[]> text(iterator->GetUTF8Text(tesseract::RIL_TEXTLINE));
                if (!text) continue;
                OcrLine line;
                line.text = QString::fromUtf8(text.get()).simplified();
                line.bounds = bounds(left, top, right, bottom);
                line.confidence = iterator->Confidence(tesseract::RIL_TEXTLINE);
                line.block = block; line.paragraph = paragraph;
                tesseract::ResultIterator wordIterator(*iterator);
                int cursor = 0;
                do {
                    const std::unique_ptr<char[]> wordText(wordIterator.GetUTF8Text(tesseract::RIL_WORD));
                    if (wordText && wordIterator.BoundingBox(tesseract::RIL_WORD, &left, &top, &right, &bottom)) {
                        const auto word = QString::fromUtf8(wordText.get()).simplified();
                        const int offset = line.text.indexOf(word, cursor);
                        line.words.append({word, bounds(left, top, right, bottom),
                                           wordIterator.Confidence(tesseract::RIL_WORD), offset});
                        if (offset >= 0) cursor = offset + word.size();
                    }
                    if (wordIterator.IsAtFinalElement(tesseract::RIL_TEXTLINE, tesseract::RIL_WORD)) break;
                } while (wordIterator.Next(tesseract::RIL_WORD));
                lines.append(line);
            } while (iterator->Next(tesseract::RIL_TEXTLINE));
            bool readable = false;
            for (const auto &line : lines) readable |= line.confidence >= 35 && !line.text.isEmpty();
            if (result.lines.isEmpty() || readable) result.lines = lines;
            if (readable) break;
        }
        return result;
    });
    m_watcher.setFuture(m_future);
}

void TesseractOcr::cancel() { m_canceled.store(true); }
