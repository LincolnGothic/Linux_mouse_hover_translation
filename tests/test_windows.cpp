// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include "portalcapture.h"
#include "screenreader.h"
#include "mockserver.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QPainter>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QtTest>
#include <cstdio>
#define NOMINMAX
#include <windows.h>

// A separate process lets the test exercise real desktop capture while the
// controller rejects windows belonging to its own process.
class ReadingSurface : public QWidget {
public:
    ReadingSurface()
    {
        setWindowTitle("Hover Translate Windows reading fixture");
        resize(850, 510);
        move(QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(30, 30));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        painter.setPen(Qt::black);
        QFont font("Microsoft YaHei"); font.setPixelSize(40); painter.setFont(font);
        painter.drawText(QRect(80, 100, 700, 80), Qt::AlignLeft | Qt::AlignVCenter, "Hello world");
        painter.drawText(QRect(80, 240, 700, 80), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("你好世界"));
        painter.drawText(QRect(80, 360, 700, 65), Qt::AlignLeft | Qt::AlignVCenter, "Hello world.");
        painter.drawText(QRect(80, 415, 700, 65), Qt::AlignLeft | Qt::AlignVCenter, "This is a second line.");
    }
};

class WindowsTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void nativeCaptureAndHover_data();
    void nativeCaptureAndHover();
    void movementAndPauseCancelResults();
    void ignoresOwnWindows();
    void escapeCopyPinAndClose();
    void manualCapture();
    void localWindowsPaths();
    void typedAndRegionTranslation();
private:
    HoverSettings settings(const MockServer &server) const;
    QProcess m_fixture;
    QPoint m_english, m_chinese, m_sentence;
    HWND m_window = nullptr;
    std::unique_ptr<QMimeData> m_clipboard;
    QPoint m_originalPointer;
};

void WindowsTest::initTestCase()
{
    QVERIFY2(HoverController::platformProblem().isEmpty(), qPrintable(HoverController::platformProblem()));
    m_originalPointer = QCursor::pos();
    m_clipboard = std::make_unique<QMimeData>();
    const auto *clipboard = QApplication::clipboard()->mimeData();
    for (const auto &format : clipboard->formats()) m_clipboard->setData(format, clipboard->data(format));
    m_fixture.start(QCoreApplication::applicationFilePath(), {"--reading-fixture"});
    QVERIFY(m_fixture.waitForStarted());
    QVERIFY(m_fixture.waitForReadyRead(5000));
    const auto ready = QJsonDocument::fromJson(m_fixture.readLine()).object();
    QVERIFY2(ready.contains("english"), qPrintable(QString::fromUtf8(QJsonDocument(ready).toJson())));
    const auto point = [&ready](const QString &name) {
        const auto coordinates = ready.value(name).toArray();
        return QPoint(coordinates[0].toInt(), coordinates[1].toInt());
    };
    m_english = point("english"); m_chinese = point("chinese"); m_sentence = point("sentence");
    m_window = reinterpret_cast<HWND>(ready.value("window").toString().toULongLong());
}

void WindowsTest::cleanupTestCase()
{
    if (m_window) PostMessageW(m_window, WM_CLOSE, 0, 0);
    if (!m_fixture.waitForFinished(2000)) { m_fixture.kill(); m_fixture.waitForFinished(); }
    QApplication::clipboard()->setMimeData(m_clipboard.release());
    QCursor::setPos(m_originalPointer);
}

void WindowsTest::init()
{
    SetForegroundWindow(m_window);
    QCursor::setPos(QGuiApplication::primaryScreen()->geometry().topLeft() + QPoint(5, 5));
    QTest::qWait(150);
}

HoverSettings WindowsTest::settings(const MockServer &server) const
{
    HoverSettings value;
    value.provider = "mozhi"; value.instance = server.url(); value.enabled = true;
    value.textMode = "line"; value.dwellMs = 100;
    value.tessdataPath = qEnvironmentVariable("TESSDATA_PREFIX");
    return value;
}

void WindowsTest::nativeCaptureAndHover_data()
{
    QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("source"); QTest::addColumn<QString>("translated");
    QTest::newRow("english-line") << QString("line") << QString("zh-CN") << QString("Hello world") << QString::fromUtf8("你好，世界");
    QTest::newRow("chinese-line") << QString("line") << QString("en") << QString::fromUtf8("你好世界") << QString("Hello world");
    QTest::newRow("english-word") << QString("word") << QString("zh-CN") << QString("Hello") << QString::fromUtf8("你好，世界");
    QTest::newRow("english-sentence") << QString("sentence") << QString("zh-CN") << QString("Hello world.") << QString::fromUtf8("你好，世界");
}

void WindowsTest::nativeCaptureAndHover()
{
    QFETCH(QString, mode); QFETCH(QString, target); QFETCH(QString, source); QFETCH(QString, translated);
    MockServer server; TesseractOcr ocr; TranslationService translator; HoverController hover(&ocr, &translator);
    auto configuration = settings(server); configuration.textMode = mode; configuration.target = target;
    QString diagnostic, problem;
    connect(&hover, &HoverController::statusChanged, this, [&](const QString &message) { diagnostic += message + '\n'; });
    QVERIFY2(hover.configure(configuration, &problem), qPrintable(problem));
    QSignalSpy popup(&hover, &HoverController::popupShown);
    QApplication::clipboard()->setText("clipboard sentinel");
    const auto foreground = GetForegroundWindow();
    const QPoint point = target == "en" ? m_chinese : mode == "sentence" ? m_sentence : m_english;
    QCursor::setPos(point);
    QVERIFY2(popup.wait(10000), qPrintable(diagnostic));
    QString recognized = popup.last()[0].toString(); recognized.remove(' ');
    QString expected = source; expected.remove(' ');
    QVERIFY2(recognized.contains(expected), qPrintable(recognized));
    if (mode == "word") QCOMPARE(popup.last()[0].toString(), QString("Hello"));
    QCOMPARE(popup.last()[1].toString(), translated);
    QCOMPARE(QApplication::clipboard()->text(), QString("clipboard sentinel"));
    QCOMPARE(GetForegroundWindow(), foreground);
    QVERIFY(hover.popup()->isVisible());
    SourceHighlight *highlight = nullptr;
    for (auto *widget : QApplication::topLevelWidgets())
        if (auto *candidate = dynamic_cast<SourceHighlight *>(widget); candidate && candidate->isVisible()) highlight = candidate;
    QVERIFY(highlight);
    QVERIFY2(highlight->frameGeometry().contains(point), qPrintable(QString("Highlight %1,%2 %3x%4 misses pointer %5,%6")
        .arg(highlight->x()).arg(highlight->y()).arg(highlight->width()).arg(highlight->height()).arg(point.x()).arg(point.y())));
    QVERIFY(QGuiApplication::screenAt(point)->availableGeometry().contains(hover.popup()->frameGeometry()));
    QCursor::setPos(point + QPoint(0, 90));
    QTRY_VERIFY(!hover.popup()->isVisible());
    QCursor::setPos(point);
    QTRY_COMPARE_WITH_TIMEOUT(popup.count(), 2, 10000);
    QCOMPARE(server.requests.size(), 1);
}

void WindowsTest::movementAndPauseCancelResults()
{
    MockServer server; server.delayMs = 700;
    TesseractOcr ocr; TranslationService translator; HoverController hover(&ocr, &translator);
    auto configuration = settings(server); QVERIFY(hover.configure(configuration));
    QSignalSpy request(&server, &MockServer::received), popup(&hover, &HoverController::popupShown);
    QCursor::setPos(m_english); QVERIFY(request.wait(10000));
    QCursor::setPos(m_english + QPoint(0, 90)); QTest::qWait(900);
    QCOMPARE(popup.count(), 0); QVERIFY(!hover.popup()->isVisible());
    QCursor::setPos(m_english); QVERIFY(request.wait(10000));
    configuration.enabled = false; QVERIFY(hover.configure(configuration)); QTest::qWait(900);
    QCOMPARE(popup.count(), 0); QVERIFY(!hover.enabled());
}

void WindowsTest::ignoresOwnWindows()
{
    ReadingSurface own; own.show(); QVERIFY(QTest::qWaitForWindowExposed(&own));
    own.activateWindow(); QTest::qWait(150);
    MockServer server; TesseractOcr ocr; TranslationService translator; HoverController hover(&ocr, &translator);
    QVERIFY(hover.configure(settings(server)));
    QCursor::setPos(own.mapToGlobal(QPoint(140, 140))); QTest::qWait(800);
    QCOMPARE(server.requests.size(), 0); QVERIFY(!hover.popup()->isVisible());
}

void WindowsTest::escapeCopyPinAndClose()
{
    MockServer server; TesseractOcr ocr; TranslationService translator; HoverController hover(&ocr, &translator);
    QVERIFY(hover.configure(settings(server)));
    QSignalSpy popup(&hover, &HoverController::popupShown);
    QCursor::setPos(m_english); QVERIFY(popup.wait(10000));
    auto *pin = hover.popup()->findChild<QPushButton *>("pinTranslationButton"); QVERIFY(pin);
    QTest::mouseClick(pin, Qt::LeftButton); QVERIFY(hover.popup()->pinned());
    QCursor::setPos(m_english + QPoint(0, 90)); QTest::qWait(700); QVERIFY(hover.popup()->isVisible());
    auto *copy = hover.popup()->findChild<QPushButton *>("copyTranslationButton"); QVERIFY(copy);
    QTest::mouseClick(copy, Qt::LeftButton); QCOMPARE(QApplication::clipboard()->text(), QString::fromUtf8("你好，世界"));
    INPUT key{}; key.type = INPUT_KEYBOARD; key.ki.wVk = VK_ESCAPE;
    QCOMPARE(SendInput(1, &key, sizeof(key)), UINT(1)); QTest::qWait(100);
    key.ki.dwFlags = KEYEVENTF_KEYUP; QCOMPARE(SendInput(1, &key, sizeof(key)), UINT(1));
    QTRY_VERIFY(!hover.popup()->isVisible());
    QCursor::setPos(m_english); QTRY_COMPARE_WITH_TIMEOUT(popup.count(), 2, 10000);
    key.ki.dwFlags = 0; QCOMPARE(SendInput(1, &key, sizeof(key)), UINT(1)); QTest::qWait(100);
    key.ki.dwFlags = KEYEVENTF_KEYUP; QCOMPARE(SendInput(1, &key, sizeof(key)), UINT(1));
    QTRY_VERIFY(!hover.popup()->isVisible()); QTest::qWait(500); QCOMPARE(popup.count(), 2);
    QCursor::setPos(m_english + QPoint(0, 90)); QTest::qWait(500);
    QCursor::setPos(m_english); QTRY_COMPARE_WITH_TIMEOUT(popup.count(), 3, 10000);
    auto *close = hover.popup()->findChild<QPushButton *>("closeTranslationButton"); QVERIFY(close);
    QTest::mouseClick(close, Qt::LeftButton); QVERIFY(!hover.popup()->isVisible()); QVERIFY(!hover.popup()->pinned());
}

void WindowsTest::manualCapture()
{
    PortalCapture capture; QSignalSpy result(&capture, &PortalCapture::captured);
    capture.capture(); QTRY_COMPARE(result.count(), 1);
    const auto image = qvariant_cast<QImage>(result.first().first()); QVERIFY(!image.isNull());
    TesseractOcr ocr; QVERIFY(ocr.init("eng+chi_sim", qEnvironmentVariable("TESSDATA_PREFIX").toUtf8()));
    QSignalSpy text(&ocr, &TesseractOcr::recognized); ocr.recognize(image, 96);
    QVERIFY(text.wait(10000)); QVERIFY(text.first().first().toString().contains("Hello world"));
    QSignalSpy canceled(&capture, &PortalCapture::captured);
    capture.capture(); capture.cancel(); QTest::qWait(100); QCOMPARE(canceled.count(), 0);
}

void WindowsTest::typedAndRegionTranslation()
{
    MockServer server; ScreenReader reader; reader.configure(settings(server)); reader.show();
    QVERIFY(QTest::qWaitForWindowExposed(&reader));
    auto *source = reader.findChild<QPlainTextEdit *>("sourceText");
    auto *result = reader.findChild<QPlainTextEdit *>("resultText");
    auto *translate = reader.findChild<QPushButton *>("translateButton");
    QVERIFY(source && result && translate);
    source->setPlainText("Hello world"); QTest::mouseClick(translate, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(result->toPlainText(), QString::fromUtf8("你好，世界"), 5000);
    bool selected = false;
    QTimer selection;
    selection.setInterval(25);
    connect(&selection, &QTimer::timeout, &reader, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || dialog == &reader) return;
        selection.stop();
        // Map the known fixture's logical screen region into the fitted image
        // preview, independently of the native capture's physical pixel scale.
        const auto screen = QGuiApplication::primaryScreen()->geometry();
        const QSizeF preview = QSizeF(screen.size()).scaled(QSizeF(dialog->size()), Qt::KeepAspectRatio);
        const QPointF origin((dialog->width() - preview.width()) / 2, (dialog->height() - preview.height()) / 2);
        const qreal scale = preview.width() / screen.width();
        const QPointF topLeft = origin + QPointF(m_english - QPoint(75, 35) - screen.topLeft()) * scale;
        const QPointF bottomRight = topLeft + QPointF(330, 75) * scale;
        QTest::mousePress(dialog, Qt::LeftButton, Qt::NoModifier, topLeft.toPoint());
        QTest::mouseMove(dialog, bottomRight.toPoint());
        QTest::mouseRelease(dialog, Qt::LeftButton, Qt::NoModifier, bottomRight.toPoint());
        selected = true;
    });
    selection.start();
    reader.capture();
    // The fixture must be in the foreground when the delayed desktop grab runs.
    SetForegroundWindow(m_window);
    QTRY_VERIFY_WITH_TIMEOUT(selected, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(source->toPlainText().contains("Hello world"), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(result->toPlainText(), QString::fromUtf8("你好，世界"), 5000);
    reader.close();
}

void WindowsTest::localWindowsPaths()
{
    const auto app = QCoreApplication::applicationDirPath();
    QCOMPARE(offlineDataDirectory(), app + "/data");
    QCOMPARE(SettingsStore().fileName(), app + "/data/settings.ini");
    HoverSettings configuration; configuration.pythonPath = "D:/selected-runtime/python.exe";
    QCOMPARE(offlinePython(configuration), configuration.pythonPath);
    configuration.pythonPath.clear();
    const auto managed = app + "/data/argos-env/Scripts/python.exe";
    QCOMPARE(offlinePython(configuration), QFileInfo::exists(managed) ? managed : QString("python.exe"));
    QTemporaryDir directory;
    SettingsStore store(directory.filePath("settings.ini"));
    if (QFileInfo::exists(app + "/tessdata/eng.traineddata") && QFileInfo::exists(app + "/tessdata/chi_sim.traineddata"))
        QCOMPARE(store.load().tessdataPath, app + "/tessdata");
    configuration.tessdataPath = "D:/selected-ocr/models";
    QVERIFY(store.save(configuration)); QCOMPARE(store.load().tessdataPath, configuration.tessdataPath);
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    if (application.arguments().contains("--reading-fixture")) {
        ReadingSurface surface; surface.show(); surface.activateWindow();
        QTimer::singleShot(400, &surface, [&] {
            const auto coordinates = [&surface](QPoint local) { const auto point = surface.mapToGlobal(local); return QJsonArray{point.x(), point.y()}; };
            const auto ready = QJsonDocument(QJsonObject{{"english", coordinates({140, 140})},
                {"chinese", coordinates({140, 280})}, {"sentence", coordinates({140, 392})},
                {"window", QString::number(quintptr(surface.winId()))}}).toJson(QJsonDocument::Compact) + '\n';
            std::fwrite(ready.constData(), 1, ready.size(), stdout); std::fflush(stdout);
        });
        return application.exec();
    }
    WindowsTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_windows.moc"
