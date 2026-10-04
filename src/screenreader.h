// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "settings.h"
#include "portalcapture.h"
#include "tesseractocr.h"
#include "translationservice.h"
#include <QDialog>
class QPlainTextEdit;
class QLabel;
class ScreenReader : public QDialog {
    Q_OBJECT
public:
    explicit ScreenReader(QWidget *parent = nullptr);
    void configure(const HoverSettings &settings);
    void capture();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void translateText();
    void readImage(const QImage &image);
    HoverSettings m_settings;
    PortalCapture m_capture;
    TesseractOcr m_ocr;
    TranslationService m_translator;
    QPlainTextEdit *m_source, *m_result;
    QLabel *m_status;
    QTimer m_captureDelay;
    quint64 m_generation = 0;
};
