// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// Runs only inside tools/test-gnome.sh's isolated real GNOME compositor.
#include "hovercontroller.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFile>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TargetWindow : public QWidget {
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.fillRect(rect(),Qt::white);
        QFont font("Noto Sans CJK SC"); font.setPixelSize(26);
        painter.setFont(font); painter.setPen(Qt::black);
        painter.drawText(80,350,"Hello world");
        painter.drawText(80,500,"你好世界");
    }
};

class LiveGnomeTest : public QObject {
    Q_OBJECT
    const QString service = "io.github.LincolnGothic.HoverTranslate.Gnome";
    const QString path = "/io/github/LincolnGothic/HoverTranslate/Gnome";
    const QString remote = "org.gnome.Mutter.RemoteDesktop";
    QString session;
    QDBusMessage call(const QString &service, const QString &path, const QString &interface,
        const QString &name, const QList<QVariant> &args = {}) {
        auto message = QDBusMessage::createMethodCall(service,path,interface,name);
        message.setArguments(args);
        return QDBusConnection::sessionBus().call(message,QDBus::Block,5000);
    }
    QDBusMessage input(const QString &method, const QList<QVariant> &args = {}) {
        return call(remote,session,remote+".Session",method,args);
    }
    QList<QVariant> status() { return call(service,path,service,"GetStatus").arguments(); }
private slots:
    void realPointerCaptureOcrTranslationAndPopup() {
        QVERIFY(GnomeHover::available());
        QDBusReply<QDBusObjectPath> created = call(remote,"/org/gnome/Mutter/RemoteDesktop",remote,"CreateSession");
        QVERIFY2(created.isValid(),qPrintable(created.error().message()));
        session = created.value().path();
        auto started = input("Start");
        QVERIFY2(started.type()!=QDBusMessage::ErrorMessage,qPrintable(started.errorMessage()));
        TargetWindow window; window.setWindowTitle("Native Wayland OCR target"); window.showFullScreen();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Close GNOME's startup overview with actual compositor input.
        QVERIFY(input("NotifyKeyboardKeysym",{uint(0xff1b),true}).type()!=QDBusMessage::ErrorMessage);
        QVERIFY(input("NotifyKeyboardKeysym",{uint(0xff1b),false}).type()!=QDBusMessage::ErrorMessage);
        QTest::qWait(800);
        QTemporaryDir directory;
        QFile dictionary(directory.filePath("cedict.u8")); QVERIFY(dictionary.open(QIODevice::WriteOnly));
        dictionary.write(QString("# fixture authored for this test\n你好世界 你好世界 [ni3 hao3 shi4 jie4] /Hello world/\n").toUtf8()); dictionary.close();
        HoverSettings settings; settings.enabled = true; settings.dwellMs = 200;
        settings.dictionaryPath = dictionary.fileName(); settings.pythonPath = "python3";
        TesseractOcr ocr; TranslationService translator;
        HoverController hover(&ocr,&translator); QSignalSpy popup(&hover,&HoverController::popupShown);
        QVERIFY(HoverController::platformProblem().isEmpty());
        QVERIFY(hover.configure(settings));
        QVERIFY(input("NotifyPointerMotionRelative",{-10000.0,-10000.0}).type()!=QDBusMessage::ErrorMessage);
        QTest::qWait(150);
        auto moved = input("NotifyPointerMotionRelative",{130.0,340.0});
        QVERIFY2(moved.type()!=QDBusMessage::ErrorMessage,qPrintable(moved.errorMessage()));
        QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,12000);
        QCOMPARE(popup.last()[0].toString(),QString("Hello world"));
        QVERIFY(popup.last()[1].toString().contains("你好世界"));
        QTRY_VERIFY_WITH_TIMEOUT(status().value(2).toBool(),3000);
        auto state = status(); QVERIFY(state[0].toBool()); QCOMPARE(state[3].toString(),QString("Hello world"));
        QVERIFY(state[4].toString().contains("你好世界"));
        QCOMPARE(state[5].toInt(),130); QCOMPARE(state[6].toInt(),340);
        const auto shown = popup.count();
        // Esc dismisses the shell popup without immediately rearming hover.
        input("NotifyKeyboardKeysym",{uint(0xff1b),true}); input("NotifyKeyboardKeysym",{uint(0xff1b),false});
        QTRY_VERIFY(!status().value(2).toBool()); QTest::qWait(500); QCOMPARE(popup.count(),shown);
        input("NotifyPointerMotionRelative",{0.0,100.0});
        QTRY_VERIFY(!status().value(2).toBool());
        if (qEnvironmentVariableIsSet("HOVER_GNOME_MODEL_PYTHON")) {
            settings.pythonPath = qEnvironmentVariable("HOVER_GNOME_MODEL_PYTHON");
            settings.packagesPath = qEnvironmentVariable("HOVER_GNOME_MODELS_DIR");
            settings.useDictionary = false;
            QVERIFY(hover.configure(settings)); popup.clear();
            input("NotifyPointerMotionRelative",{0.0,-100.0});
            QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,30000);
            QCOMPARE(popup.last()[0].toString(),QString("Hello world"));
            QVERIFY2(popup.last()[1].toString().contains("世界"),qPrintable(popup.last()[1].toString()));
            QTRY_VERIFY(status().value(2).toBool());
            QVERIFY(status().value(4).toString().contains("世界"));
            settings.target = "en"; QVERIFY(hover.configure(settings)); popup.clear();
            input("NotifyPointerMotionRelative",{0.0,150.0});
            QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,30000);
            auto translated = popup.last()[1].toString().toLower();
            QVERIFY2(translated.contains("hello") && translated.contains("world"),qPrintable(translated));
            QTRY_VERIFY(status().value(2).toBool());
        } else {
            qInfo("Real sentence models were not exercised in this GNOME run; dictionary fixture only.");
        }
        // Exercise the actual extension's disable/enable lifecycle and the
        // controller's reconnection, as occurs around a GNOME session change.
        auto disabled = call("org.gnome.Shell","/org/gnome/Shell","org.gnome.Shell.Extensions",
            "DisableExtension",{QString("hover-translate@lincolngothic.github.io")});
        QVERIFY2(disabled.type()!=QDBusMessage::ErrorMessage,qPrintable(disabled.errorMessage()));
        QTRY_VERIFY(!GnomeHover::available()); QTRY_VERIFY(!hover.enabled());
        auto resumed = call("org.gnome.Shell","/org/gnome/Shell","org.gnome.Shell.Extensions",
            "EnableExtension",{QString("hover-translate@lincolngothic.github.io")});
        QVERIFY2(resumed.type()!=QDBusMessage::ErrorMessage,qPrintable(resumed.errorMessage()));
        QTRY_VERIFY(GnomeHover::available()); QTRY_VERIFY(hover.enabled());
        QTRY_VERIFY(status().value(0).toBool());
        auto paused = settings; paused.enabled = false; QVERIFY(hover.configure(paused));
        QTRY_VERIFY(!status().value(0).toBool());
        input("Stop"); window.close();
    }
};
int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("HoverFixture");
    app.setDesktopFileName("io.github.LincolnGothic.HoverFixture");
    LiveGnomeTest test; return QTest::qExec(&test,argc,argv);
}
#include "test_gnome_live.moc"
