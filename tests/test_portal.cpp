// SPDX-License-Identifier: GPL-3.0-or-later
#include "portalcapture.h"
#include "screenreader.h"
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFile>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>

class ScreenshotFixture : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Screenshot")
public:
    uint code = 0;
    QString uri;
    int delay = 40;
    QVariantMap received;
public slots:
    QDBusObjectPath Screenshot(const QString &, const QVariantMap &options, const QDBusMessage &message)
    {
        received = options;
        QString sender = message.service().mid(1); sender.replace('.', '_');
        const QString path = "/org/freedesktop/portal/desktop/request/" + sender + '/' + options.value("handle_token").toString();
        const uint result = code;
        const QString image = uri;
        QTimer::singleShot(delay, this, [path, result, image] {
            auto signal = QDBusMessage::createSignal(path, "org.freedesktop.portal.Request", "Response");
            signal << result << QVariantMap{{"uri", image}};
            QDBusConnection::sessionBus().send(signal);
        });
        return QDBusObjectPath(path);
    }
};

class PortalTest : public QObject {
    Q_OBJECT
    ScreenshotFixture fixture;
    QTemporaryDir directory;
private slots:
    void initTestCase()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY2(bus.isConnected(), "Run tests inside dbus-run-session.");
        QVERIFY(bus.registerService("org.freedesktop.portal.Desktop"));
        QVERIFY(bus.registerObject("/org/freedesktop/portal/desktop", &fixture, QDBusConnection::ExportAllSlots));
        QImage image(800,230,QImage::Format_RGB32); image.fill(Qt::white);
        QPainter painter(&image); painter.setPen(Qt::black); painter.setFont(QFont("Noto Sans CJK SC",30));
        painter.drawText(QRect(40,20,700,80), Qt::AlignLeft | Qt::AlignVCenter, "Wrong paragraph");
        painter.drawText(QRect(40,120,700,80), Qt::AlignLeft | Qt::AlignVCenter, "Hello");
        painter.end();
        QVERIFY(image.save(directory.filePath("screen.png")));
    }
    void init()
    {
        fixture.code = 0; fixture.delay = 40;
        fixture.uri = QUrl::fromLocalFile(directory.filePath("screen.png")).toString();
    }
    void actualDbusScreenshot()
    {
        PortalCapture capture;
        QSignalSpy success(&capture, &PortalCapture::captured);
        QSignalSpy failure(&capture, &PortalCapture::failed);
        capture.capture();
        QVERIFY(success.wait(3000));
        QCOMPARE(qvariant_cast<QImage>(success.last().at(0)).size(), QSize(800,230));
        QVERIFY(fixture.received.value("interactive").toBool());
        QVERIFY(!fixture.received.value("handle_token").toString().isEmpty());
        QCOMPARE(failure.count(),0);
    }
    void desktopCancellation()
    {
        fixture.code = 1;
        PortalCapture capture;
        QSignalSpy cancelled(&capture, &PortalCapture::cancelled);
        QSignalSpy success(&capture, &PortalCapture::captured);
        capture.capture();
        QVERIFY(cancelled.wait(3000));
        QCOMPARE(success.count(),0);
    }
    void rejectsRemoteScreenshot()
    {
        fixture.uri = "https://example.com/screen.png";
        PortalCapture capture;
        QSignalSpy failure(&capture, &PortalCapture::failed);
        capture.capture();
        QVERIFY(failure.wait(3000));
        QVERIFY(failure.last().at(0).toString().contains("invalid"));
    }
    void timeoutAndLateResponse()
    {
        fixture.delay = 250;
        PortalCapture capture; capture.setTimeoutMs(60);
        QSignalSpy failure(&capture, &PortalCapture::failed);
        QSignalSpy success(&capture, &PortalCapture::captured);
        capture.capture();
        QVERIFY(failure.wait(1000));
        QTest::qWait(300);
        QCOMPARE(success.count(),0);
    }
    void screenshotCropOcrAndOfflineDictionary()
    {
        QFile dictionary(directory.filePath("cedict.u8"));
        QVERIFY(dictionary.open(QIODevice::WriteOnly));
        dictionary.write(QString("# fixture authored for this test\n你好 你好 [ni3 hao3] /hello/hi/\n").toUtf8()); dictionary.close();
        HoverSettings settings; settings.dictionaryPath = dictionary.fileName(); settings.pythonPath = "python3";
        ScreenReader reader; reader.configure(settings); reader.show();
        QVERIFY(QTest::qWaitForWindowExposed(&reader));
        QApplication::clipboard()->setText("clipboard sentinel");
        QSignalSpy clipboard(QApplication::clipboard(), &QClipboard::dataChanged);
        QStringList clipboardValues;
        connect(QApplication::clipboard(), &QClipboard::dataChanged, &reader, [&] {
            clipboardValues.append(QApplication::clipboard()->text());
        });
        // Exercise user region selection on the real preview, not an OCR stub.
        QTimer select;
        select.setInterval(30);
        connect(&select, &QTimer::timeout, this, [&] {
            auto *selector = QApplication::activeModalWidget();
            if (!selector || !selector->windowTitle().startsWith("Drag around")) return;
            select.stop();
            const qreal scale = selector->width()/800.0;
            const QPoint first(int(20*scale),int(112*scale));
            const QPoint last(int(650*scale),int(210*scale));
            QTest::mousePress(selector,Qt::LeftButton,Qt::NoModifier,first);
            QTest::mouseMove(selector,last);
            QTest::mouseRelease(selector,Qt::LeftButton,Qt::NoModifier,last);
        });
        select.start(); reader.capture();
        auto *source = reader.findChild<QPlainTextEdit *>("sourceText");
        auto *result = reader.findChild<QPlainTextEdit *>("resultText");
        QTRY_VERIFY_WITH_TIMEOUT(result->toPlainText().contains("你好"),10000);
        QVERIFY(source->toPlainText().contains("Hello"));
        QVERIFY(!source->toPlainText().contains("Wrong"));
        QCOMPARE(QApplication::clipboard()->text(),QString("clipboard sentinel"));
        // Wayland can reannounce the same clipboard offer when focus changes.
        // Verify the contents at every notification, rather than assuming
        // notifications necessarily mean an application changed the clipboard.
        QCOMPARE(clipboard.count(),clipboardValues.size());
        for (const auto &value : clipboardValues) QCOMPARE(value,QString("clipboard sentinel"));
        QVERIFY(reader.grab().save(QCoreApplication::applicationDirPath() + "/reader-window.png"));
        reader.close();
    }
    void cleanupTestCase()
    {
        QDBusConnection::sessionBus().unregisterObject("/org/freedesktop/portal/desktop");
        QDBusConnection::sessionBus().unregisterService("org.freedesktop.portal.Desktop");
    }
};
QTEST_MAIN(PortalTest)
#include "test_portal.moc"
