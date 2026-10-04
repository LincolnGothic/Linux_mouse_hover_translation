// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include "mockserver.h"
#include <QClipboard>
#include <QCursor>
#include <QLineEdit>
#include <QPainter>
#include <QProcess>
#include <QSignalSpy>
#include <QScreen>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdlib>
#include <xcb/xtest.h>

class ReadingSurface : public QWidget {
public:
    ReadingSurface()
    {
        resize(1000,650);
        move(40,40);
        edit = new QLineEdit("Keep this selected", this);
        edit->setGeometry(100, 500, 300, 40);
        setWindowTitle("Reading surface");
    }
    QLineEdit *edit;
    QPoint englishPoint() const { return mapToGlobal(QPoint(170,160)); }
    QPoint chinesePoint() const { return mapToGlobal(QPoint(155,320)); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        painter.setPen(Qt::black);
        painter.setFont(QFont("Noto Sans CJK SC", 30));
        painter.drawText(QRect(100,120,760,80), Qt::AlignLeft | Qt::AlignVCenter, "Hello world");
        painter.drawText(QRect(100,280,760,80), Qt::AlignLeft | Qt::AlignVCenter, "你好世界");
    }
};

class HoverTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void actualHoverBothDirectionsAndCache();
    void movementDropsSlowTranslation();
    void pauseDropsSlowTranslation();
    void ignoresSettingsWindow();
    void sameTargetMakesNoRequest();
    void escapeDismissesWithoutReappearing();
    void commandLineUsesActualBinary();
    void settingsWindowUsesActualBinary();
};

void HoverTest::init()
{
    QVERIFY2(HoverController::platformProblem().isEmpty(), qPrintable(HoverController::platformProblem()));
    QCursor::setPos(10, 10);
}

void HoverTest::actualHoverBothDirectionsAndCache()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    surface.activateWindow();
    surface.edit->setFocus();
    surface.edit->selectAll();
    QApplication::clipboard()->setText("clipboard sentinel");
    QTest::qWait(100);
    auto *focus = QApplication::focusWidget();
    const auto selection = surface.edit->selectedText();
    QSignalSpy clipboard(QApplication::clipboard(), &QClipboard::dataChanged);
    MockServer server;
    TesseractOcr ocr;
    TranslationService client;
    HoverController controller(&ocr, &client);
    QString diagnostic;
    connect(&controller, &HoverController::statusChanged, this, [&](const QString &status) { diagnostic += status + '\n'; });
    connect(&ocr, &TesseractOcr::started, this, [] {
        QGuiApplication::primaryScreen()->grabWindow(0).save(QCoreApplication::applicationDirPath() + "/hover-fixture.png");
        auto *screen = QGuiApplication::primaryScreen();
        const QRect crop = QRect(QCursor::pos() - QPoint(350,80), QSize(700,160)).intersected(screen->geometry());
        screen->grabWindow(0, crop.x(), crop.y(), crop.width(), crop.height()).save(QCoreApplication::applicationDirPath() + "/hover-crop.png");
    });
    connect(&ocr, &TesseractOcr::linesRecognized, this, [&](const QVector<OcrLine> &lines) {
        diagnostic += QString("OCR lines: %1\n").arg(lines.size());
        for (const auto &line : lines)
            diagnostic += QString("OCR: %1, x=%2 y=%3 w=%4 h=%5 confidence=%6\n")
                .arg(line.text).arg(line.bounds.x()).arg(line.bounds.y())
                .arg(line.bounds.width()).arg(line.bounds.height()).arg(line.confidence);
    });
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QSignalSpy popup(&controller, &HoverController::popupShown);
    QCursor::setPos(surface.englishPoint());
    QVERIFY2(popup.wait(6000), qPrintable(diagnostic));
    QVERIFY(popup.last().at(0).toString().contains("Hello world"));
    QCOMPARE(popup.last().at(1).toString(), QString("你好，世界"));
    QVERIFY(controller.popup()->isVisible());
    QCOMPARE(QApplication::focusWidget(), focus);
    QCOMPARE(surface.edit->selectedText(), selection);
    QCOMPARE(QApplication::clipboard()->text(), QString("clipboard sentinel"));
    QCOMPARE(clipboard.count(), 0);
    QTest::qWait(50);
    QVERIFY(QGuiApplication::primaryScreen()->grabWindow(0).save(QCoreApplication::applicationDirPath() + "/hover-popup.png"));
    QCursor::setPos(10,10);
    QTRY_VERIFY(!controller.popup()->isVisible());
    QCursor::setPos(surface.englishPoint());
    QTRY_COMPARE_WITH_TIMEOUT(popup.count(), 2, 6000);
    QCOMPARE(server.requests.size(), 1); // The same text uses the bounded cache.
    settings.target = "en";
    QVERIFY(controller.configure(settings));
    QCursor::setPos(surface.chinesePoint());
    QTRY_COMPARE_WITH_TIMEOUT(popup.count(), 3, 6000);
    auto recognized = popup.last().at(0).toString(); recognized.remove(' ');
    QVERIFY(recognized.contains("你好世界"));
    QCOMPARE(popup.last().at(1).toString(), QString("Hello world"));
    QCOMPARE(server.requests.size(), 2);
}

void HoverTest::movementDropsSlowTranslation()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    MockServer server; server.delayMs = 500;
    TesseractOcr ocr; TranslationService client; HoverController controller(&ocr, &client);
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QSignalSpy request(&server, &MockServer::received);
    QSignalSpy popup(&controller, &HoverController::popupShown);
    QCursor::setPos(surface.englishPoint());
    QVERIFY(request.wait(6000));
    QCursor::setPos(10,10);
    QTest::qWait(800);
    QCOMPARE(popup.count(), 0);
    QVERIFY(!controller.popup()->isVisible());
}

void HoverTest::pauseDropsSlowTranslation()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    MockServer server; server.delayMs = 500;
    TesseractOcr ocr; TranslationService client; HoverController controller(&ocr, &client);
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QSignalSpy request(&server, &MockServer::received);
    QSignalSpy popup(&controller, &HoverController::popupShown);
    QCursor::setPos(surface.englishPoint());
    QVERIFY(request.wait(6000));
    settings.enabled = false;
    QVERIFY(controller.configure(settings));
    QTest::qWait(800);
    QCOMPARE(popup.count(), 0);
    QVERIFY(!controller.enabled());
}

void HoverTest::ignoresSettingsWindow()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    MockServer server;
    TesseractOcr ocr; TranslationService client; HoverController controller(&ocr, &client);
    controller.setIgnoredWidgets({&surface});
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QCursor::setPos(surface.englishPoint());
    QTest::qWait(600);
    QCOMPARE(server.requests.size(), 0);
    QVERIFY(!controller.popup()->isVisible());
}

void HoverTest::sameTargetMakesNoRequest()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    MockServer server;
    TesseractOcr ocr; TranslationService client; HoverController controller(&ocr, &client);
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.target = "en"; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QSignalSpy read(&ocr, &TesseractOcr::linesRecognized);
    QString recognized;
    connect(&ocr, &TesseractOcr::linesRecognized, this, [&](const QVector<OcrLine> &lines) {
        for (const auto &line : lines) recognized += line.text;
    });
    QCursor::setPos(surface.englishPoint());
    QVERIFY(read.wait(6000));
    QVERIFY(recognized.contains("Hello world"));
    QCOMPARE(server.requests.size(), 0);
    QVERIFY(!controller.popup()->isVisible());
}

void HoverTest::escapeDismissesWithoutReappearing()
{
    ReadingSurface surface; surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    MockServer server;
    TesseractOcr ocr; TranslationService client; HoverController controller(&ocr, &client);
    HoverSettings settings; settings.provider = "mozhi"; settings.enabled = true; settings.dwellMs = 100; settings.instance = server.url();
    QVERIFY(controller.configure(settings));
    QSignalSpy popup(&controller, &HoverController::popupShown);
    QCursor::setPos(surface.englishPoint());
    QVERIFY(popup.wait(6000));
    auto *connection = xcb_connect(nullptr, nullptr);
    QVERIFY(!xcb_connection_has_error(connection));
    X11Escape keyMapping;
    QVERIFY(keyMapping.keycode() != 0);
    // Native XTEST events exercise the real root-window Escape grab. The
    // application itself never injects input to extract text.
    xcb_test_fake_input(connection, XCB_KEY_PRESS, keyMapping.keycode(), XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
    xcb_test_fake_input(connection, XCB_KEY_RELEASE, keyMapping.keycode(), XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
    xcb_flush(connection);
    QTRY_VERIFY(!controller.popup()->isVisible());
    QTest::qWait(500);
    QCOMPARE(popup.count(), 1);
    xcb_disconnect(connection);
}

void HoverTest::commandLineUsesActualBinary()
{
    MockServer server;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("DISPLAY");
    environment.remove("WAYLAND_DISPLAY");
    environment.remove("QT_QPA_PLATFORM");
    process.setProcessEnvironment(environment);
    process.start(APP_BINARY, {"--translate", "Hello world", "--target", "zh-CN", "--instance", server.url()});
    QVERIFY(process.waitForStarted());
    QSignalSpy finished(&process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished));
    QVERIFY(finished.wait(6000));
    QCOMPARE(process.exitCode(), 0);
    QVERIFY(QString::fromUtf8(process.readAllStandardOutput()).contains("你好，世界"));
    QCOMPARE(server.requests.size(), 1);
}

void HoverTest::settingsWindowUsesActualBinary()
{
    QTemporaryDir directory;
    QProcess process;
    process.start(APP_BINARY, {"--paused", "--config", directory.filePath("settings.ini")});
    QVERIFY(process.waitForStarted());
    auto *connection = xcb_connect(nullptr, nullptr);
    QVERIFY(!xcb_connection_has_error(connection));
    const auto root = xcb_setup_roots_iterator(xcb_get_setup(connection)).data->root;
    auto findWindow = [&] {
        xcb_window_t found = XCB_WINDOW_NONE;
        auto *tree = xcb_query_tree_reply(connection, xcb_query_tree(connection, root), nullptr);
        if (!tree) return found;
        const auto *children = xcb_query_tree_children(tree);
        for (int i = 0; i < xcb_query_tree_children_length(tree); ++i) {
            auto *name = xcb_get_property_reply(connection, xcb_get_property(connection, false,
                children[i], XCB_ATOM_WM_NAME, XCB_GET_PROPERTY_TYPE_ANY, 0, 1024), nullptr);
            if (name && QByteArray(static_cast<const char *>(xcb_get_property_value(name)),
                xcb_get_property_value_length(name)) == "Hover Translate") found = children[i];
            std::free(name);
        }
        std::free(tree);
        return found;
    };
    xcb_window_t window = XCB_WINDOW_NONE;
    QTRY_VERIFY_WITH_TIMEOUT((window = findWindow()) != XCB_WINDOW_NONE, 5000);
    QTest::qWait(100);
    QVERIFY(QGuiApplication::primaryScreen()->grabWindow(window).save(QCoreApplication::applicationDirPath() + "/settings-window.png"));
    QCOMPARE(process.state(), QProcess::Running);
    auto atom = [&](const QByteArray &name) {
        auto *reply = xcb_intern_atom_reply(connection, xcb_intern_atom(connection, false, name.size(), name.constData()), nullptr);
        const xcb_atom_t value = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
        std::free(reply);
        return value;
    };
    xcb_client_message_event_t close{};
    close.response_type = XCB_CLIENT_MESSAGE;
    close.format = 32;
    close.window = window;
    close.type = atom("WM_PROTOCOLS");
    close.data.data32[0] = atom("WM_DELETE_WINDOW");
    close.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(connection, false, window, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&close));
    xcb_flush(connection);
    QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::NotRunning, 3000);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    xcb_disconnect(connection);
}

QTEST_MAIN(HoverTest)
#include "test_hover.moc"
