// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gnomehover.h"
#include <QBuffer>
#include <QDBusConnection>
#include <QDBusContext>
#include <QFile>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

const QString service = "io.github.LincolnGothic.HoverTranslate.Gnome";
const QString path = "/io/github/LincolnGothic/HoverTranslate/Gnome";
class GnomeFixture : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.LincolnGothic.HoverTranslate.Gnome")
public:
    uint token = 0;
    int configured = 0, options = 0;
    bool temporary = true, legacy = false, active = false;
public slots:
    void Configure(bool value, int) { active=value; ++configured; emit Invalidated(++token); }
    void ConfigureOptions(bool, bool value) {
        ++options;
        if (legacy) { sendErrorReply(QDBusError::UnknownMethod,"ConfigureOptions is unavailable"); return; }
        temporary=value;
    }
    void Selection(uint, const QList<double> &) {}
    void Result(uint token, const QString &source, const QString &text, bool error) {
        emit resultReceived(token, source, text, error);
    }
signals:
    void Capture(uint token, const QByteArray &png, double x, double y);
    void CaptureMode(uint token, const QByteArray &png, double x, double y, const QString &mode);
    void Invalidated(uint token);
    void Problem(uint token, const QString &message);
    void resultReceived(uint token, const QString &source, const QString &text, bool error);
};

class GnomeTest : public QObject {
    Q_OBJECT
    GnomeFixture fixture;
    QTemporaryDir directory;
    HoverSettings settings;
    QByteArray image(int scale = 1) {
        QImage image(800 * scale, 180 * scale, QImage::Format_RGB32); image.fill(Qt::white);
        QPainter painter(&image); QFont font("Noto Sans CJK SC"); font.setPixelSize(24 * scale);
        painter.setFont(font); painter.setPen(Qt::black);
        painter.drawText(20 * scale, 72 * scale, "Hello world"); painter.end();
        QByteArray data; QBuffer buffer(&data); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
        return data;
    }
private slots:
    void initTestCase() {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(service));
        QVERIFY(bus.registerObject(path, &fixture, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QFile dictionary(directory.filePath("cedict.u8")); QVERIFY(dictionary.open(QIODevice::WriteOnly));
        dictionary.write(QString("# fixture authored for this test\n你好世界 你好世界 [ni3 hao3 shi4 jie4] /Hello world/\n").toUtf8());
        settings.dictionaryPath = dictionary.fileName(); settings.pythonPath = "python3";
        settings.textMode = "line"; settings.enabled = true; settings.target = "zh-CN";
    }
    void captureToDictionaryAndHiDpi() {
        for (int scale : {1,2}) {
            GnomeHover hover; QVERIFY(GnomeHover::available());
            const auto configured = fixture.configured, options=fixture.options; QVERIFY(hover.configure(settings));
            QTRY_VERIFY(fixture.configured > configured); QTRY_VERIFY(fixture.options>options);
            QSignalSpy results(&fixture,&GnomeFixture::resultReceived);
            emit fixture.Capture(fixture.token, image(scale), 100.0/800, 62.0/180);
            QTRY_COMPARE_WITH_TIMEOUT(results.count(),1,8000);
            QCOMPARE(results[0][0].toUInt(),fixture.token);
            QCOMPARE(results[0][1].toString(),QString("Hello world"));
            QVERIFY(results[0][2].toString().contains("你好世界"));
            QVERIFY(!results[0][3].toBool());
            auto paused = settings; paused.enabled = false; QVERIFY(hover.configure(paused));
            QTest::qWait(30);
        }
    }
    void invalidatedWorkCannotShowPopup() {
        GnomeHover hover; QVERIFY(hover.configure(settings)); QTest::qWait(50);
        QSignalSpy results(&fixture,&GnomeFixture::resultReceived);
        emit fixture.Capture(fixture.token,image(),100.0/800,62.0/180);
        emit fixture.Invalidated(++fixture.token);
        QTest::qWait(500);
        QCOMPARE(results.count(),0);
    }
    void rejectsStaleAndMalformedCaptures() {
        GnomeHover hover; QVERIFY(hover.configure(settings)); QTest::qWait(50);
        QSignalSpy results(&fixture,&GnomeFixture::resultReceived);
        emit fixture.Capture(fixture.token-1,image(),100.0/800,62.0/180);
        emit fixture.Capture(fixture.token,"invalid png",0.5,0.5);
        emit fixture.Capture(fixture.token,image(),1.5,0.5);
        QTest::qWait(350); QCOMPARE(results.count(),0);
    }
    void disableCancelsPendingWork() {
        GnomeHover hover; QVERIFY(hover.configure(settings)); QTest::qWait(50);
        QSignalSpy results(&fixture,&GnomeFixture::resultReceived);
        emit fixture.Capture(fixture.token,image(),100.0/800,62.0/180);
        auto paused = settings; paused.enabled = false; QVERIFY(hover.configure(paused));
        QTest::qWait(350); QCOMPARE(results.count(),0); QVERIFY(!hover.enabled());
    }
    void extensionReconnectResumesOnlyEnabledHover() {
        GnomeHover hover; QVERIFY(hover.configure(settings)); QTest::qWait(50);
        auto bus = QDBusConnection::sessionBus(); bus.unregisterService(service);
        QTRY_VERIFY(!hover.enabled()); QVERIFY(bus.registerService(service));
        QTRY_VERIFY(hover.enabled()); QTest::qWait(50);
        auto paused = settings; paused.enabled = false; QVERIFY(hover.configure(paused)); QTest::qWait(50);
        bus.unregisterService(service); QTest::qWait(50); QVERIFY(bus.registerService(service));
        QTest::qWait(100); QVERIFY(!hover.enabled());
    }
    void disabledTemporaryModeIgnoresOverrides() {
        GnomeHover hover; auto selected=settings; selected.temporaryModes=false;
        const auto options=fixture.options; QVERIFY(hover.configure(selected));
        QTRY_VERIFY(fixture.options>options); QVERIFY(!fixture.temporary);
        QSignalSpy results(&fixture,&GnomeFixture::resultReceived);
        emit fixture.CaptureMode(fixture.token,image(),100.0/800,62.0/180,"word");
        QTRY_COMPARE_WITH_TIMEOUT(results.count(),1,8000);
        QCOMPARE(results[0][1].toString(),QString("Hello world"));
    }
    void outdatedExtensionIsPaused() {
        fixture.legacy=true; const auto options=fixture.options;
        GnomeHover hover; QVERIFY(hover.configure(settings));
        QTRY_VERIFY(fixture.options>options); QTRY_VERIFY(!hover.enabled()); QTRY_VERIFY(!fixture.active);
        fixture.legacy=false;
    }
    void cleanupTestCase() {
        QDBusConnection::sessionBus().unregisterObject(path);
        QDBusConnection::sessionBus().unregisterService(service);
    }
};
QTEST_MAIN(GnomeTest)
#include "test_gnome.moc"
