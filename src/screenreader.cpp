// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenreader.h"
#include "hoverpolicy.h"
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

// Preview selection also supports portals returning a whole monitor screenshot.
class RegionSelector : public QDialog {
public:
    explicit RegionSelector(const QImage &image, QWidget *parent)
        : QDialog(parent), m_image(image)
    {
        setWindowTitle(tr("Drag around text — Enter for whole image, Esc to cancel"));
        setCursor(Qt::CrossCursor);
        const auto available = QGuiApplication::primaryScreen()->availableGeometry().size();
        resize(image.size().scaled(available * 0.85, Qt::KeepAspectRatio));
    }
    QImage selectedImage() const
    {
        if (m_selection.isEmpty()) return m_image;
        const QRectF bounds = imageBounds();
        const QRectF selected = QRectF(m_selection).intersected(bounds);
        const qreal factor = m_image.width() / bounds.width();
        const int left = int(std::floor((selected.left() - bounds.left()) * factor));
        const int top = int(std::floor((selected.top() - bounds.top()) * factor));
        const int right = int(std::ceil((selected.right() - bounds.left()) * factor));
        const int bottom = int(std::ceil((selected.bottom() - bounds.top()) * factor));
        return m_image.copy(QRect(left, top, right - left, bottom - top).intersected(m_image.rect()));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#17201d"));
        painter.drawImage(imageBounds(), m_image);
        if (!m_selection.isEmpty()) {
            painter.setPen(QPen(QColor("#208bd3"), 2));
            painter.fillRect(m_selection, QColor(32,139,211,35));
            painter.drawRect(m_selection);
        }
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && imageBounds().contains(event->position())) {
            m_start = event->position().toPoint(); m_dragging = true; m_selection = {}; update();
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) { m_selection = QRect(m_start, event->position().toPoint()).normalized().intersected(imageBounds().toRect()); update(); }
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (!m_dragging || event->button() != Qt::LeftButton) return;
        m_dragging = false;
        m_selection = QRect(m_start, event->position().toPoint()).normalized().intersected(imageBounds().toRect());
        if (m_selection.width() >= 5 && m_selection.height() >= 5) accept();
        else { m_selection = {}; update(); }
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) { m_selection = {}; accept(); }
        else QDialog::keyPressEvent(event);
    }
private:
    QRectF imageBounds() const
    {
        const QSizeF size = QSizeF(m_image.size()).scaled(QSizeF(this->size()), Qt::KeepAspectRatio);
        return QRectF(QPointF((width()-size.width())/2, (height()-size.height())/2), size);
    }
    QImage m_image;
    QRect m_selection;
    QPoint m_start;
    bool m_dragging = false;
};

ScreenReader::ScreenReader(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Translate text or screen region"));
    resize(680, 540);
    m_captureDelay.setSingleShot(true);
    connect(&m_captureDelay, &QTimer::timeout, &m_capture, &PortalCapture::capture);
    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Capture a region, or type text below. Ctrl+Enter translates."), this);
    intro->setWordWrap(true); layout->addWidget(intro);
    m_source = new QPlainTextEdit(this); m_source->setObjectName("sourceText");
    m_source->setPlaceholderText(tr("English or Chinese text…")); layout->addWidget(m_source);
    m_result = new QPlainTextEdit(this); m_result->setObjectName("resultText");
    m_result->setReadOnly(true); layout->addWidget(m_result);
    m_status = new QLabel(this); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *captureButton = buttons->addButton(tr("Capture screen region"), QDialogButtonBox::ActionRole);
    captureButton->setObjectName("captureButton");
    auto *translate = buttons->addButton(tr("Translate"), QDialogButtonBox::ActionRole);
    translate->setObjectName("translateButton");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &ScreenReader::close);
    connect(captureButton, &QPushButton::clicked, this, &ScreenReader::capture);
    connect(translate, &QPushButton::clicked, this, &ScreenReader::translateText);
    auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcut, &QShortcut::activated, this, &ScreenReader::translateText);
    connect(&m_capture, &PortalCapture::captured, this, &ScreenReader::readImage);
    connect(&m_capture, &PortalCapture::failed, this, [this](const QString &error) { show(); m_status->setText(error); });
    connect(&m_capture, &PortalCapture::cancelled, this, [this] { show(); m_status->setText(tr("Capture cancelled.")); });
    connect(&m_ocr, &TesseractOcr::linesRecognized, this, [this](const QVector<OcrLine> &lines) {
        QStringList text;
        for (const auto &line : lines) if (line.confidence >= 35 && !line.text.trimmed().isEmpty()) text.append(line.text.trimmed());
        m_source->setPlainText(text.join('\n'));
        if (text.isEmpty()) {
            // Keep uncertain OCR available for correction instead of silently
            // discarding all recognized text and looking like capture failed.
            for (const auto &line : lines) if (!line.text.trimmed().isEmpty()) text.append(line.text.trimmed());
            m_source->setPlainText(text.join('\n'));
            m_status->setText(text.isEmpty()
                ? tr("No readable text found. Enlarge the original text and select complete lines with a margin.")
                : tr("OCR confidence is low. Review or correct the text, then click Translate."));
        }
        else translateText();
    });
    connect(&m_ocr, &TesseractOcr::failed, this, [this](const QString &error) { m_status->setText(error); });
    connect(&m_translator, &TranslationService::translated, this, [this](quint64 generation, const QString &result) {
        if (generation != m_generation) return;
        m_result->setPlainText(result); m_status->setText(tr("Done."));
    });
    connect(&m_translator, &TranslationService::failed, this, [this](quint64 generation, const QString &error) {
        if (generation == m_generation) m_status->setText(error);
    });
    connect(m_source, &QPlainTextEdit::textChanged, this, [this] { ++m_generation; m_translator.cancel(); m_result->clear(); });
}

void ScreenReader::configure(const HoverSettings &settings)
{
    ++m_generation; m_translator.cancel(); m_captureDelay.stop(); m_capture.cancel(); m_ocr.cancel();
    m_result->clear(); m_settings = settings;
    m_status->setText(settings.provider == "offline"
        ? tr("Offline: text stays on this computer. Translation models must be installed once.")
        : tr("Online: text will be sent to your selected Mozhi server."));
}

void ScreenReader::capture()
{
    ++m_generation; m_translator.cancel(); m_ocr.cancel(); m_capture.cancel();
    if (m_ocr.isBusy()) { m_status->setText(tr("The previous OCR job is finishing. Try again in a moment.")); return; }
    m_status->setText(tr("Choose a screenshot in the desktop dialog, then drag around the text."));
    hide();
    m_captureDelay.start(180);
}

void ScreenReader::readImage(const QImage &image)
{
    RegionSelector selector(image, this);
    if (selector.exec() != QDialog::Accepted) { show(); m_status->setText(tr("Capture cancelled.")); return; }
    const QImage crop = selector.selectedImage();
    show();
    if (crop.isNull()) { m_status->setText(tr("Select a region containing text.")); return; }
    if (!m_ocr.init("eng+chi_sim", m_settings.tessdataPath.toUtf8())) {
        m_status->setText(tr("Install tesseract-ocr-eng and tesseract-ocr-chi-sim, then try again.")); return;
    }
    m_status->setText(tr("Reading text locally…"));
    m_ocr.recognize(crop, 96);
}

void ScreenReader::translateText()
{
    const QString text = m_source->toPlainText().trimmed();
    const QString source = HoverPolicy::sourceLanguage(text);
    if (source.isEmpty()) { m_status->setText(tr("Enter English or Chinese text.")); return; }
    ++m_generation;
    if (source == m_settings.target) { m_result->setPlainText(text); m_status->setText(tr("Already in the selected target language.")); return; }
    m_status->setText(tr("Translating… The first offline request loads the model."));
    m_translator.translate(m_generation, text, source, m_settings.target, m_settings);
}

void ScreenReader::closeEvent(QCloseEvent *event)
{
    ++m_generation; m_captureDelay.stop(); m_capture.cancel(); m_ocr.cancel(); m_translator.cancel();
    QDialog::closeEvent(event);
}
