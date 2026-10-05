// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// Runs only inside tools/test-gnome.sh's isolated real GNOME compositor.
#include "hovercontroller.h"
#include <QDBusConnection>
#include <QDBusArgument>
#include <QClipboard>
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
        painter.drawText(80,650,"This sentence continues");
        painter.drawText(80,685,"on the next line.");
        painter.drawText(80,220,"Mode"); painter.drawText(400,220,"What it translates");
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
    QPointF globalPoint() {
        const auto state=status(); return QPointF(state.value(5).toDouble(),state.value(6).toDouble());
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
        dictionary.write(QString("# fixture authored for this test\n你好世界 你好世界 [ni3 hao3 shi4 jie4] /Hello world/\n你好 你好 [ni3 hao3] /Hello/\n模式 模式 [mo2 shi4] /mode/\n").toUtf8()); dictionary.close();
        HoverSettings settings; settings.textMode = "line"; settings.enabled = true; settings.dwellMs = 200;
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
        const auto initial=popup.count();
        input("NotifyKeyboardKeysym",{uint(0xffe1),true});
        QTRY_VERIFY_WITH_TIMEOUT(popup.count()>initial,12000);
        QCOMPARE(popup.last()[0].toString(),QString("Hello"));
        const auto shifted=popup.count();
        input("NotifyKeyboardKeysym",{uint(0xffe1),false});
        QTRY_VERIFY_WITH_TIMEOUT(popup.count()>shifted,12000);
        QCOMPARE(popup.last()[0].toString(),QString("Hello world"));
        QTRY_VERIFY(status().value(2).toBool());
        auto interaction = call(service,path,service,"InteractionState").arguments();
        QVERIFY(interaction.size()==5); QVERIFY(interaction[2].toUInt()>0);
        const auto buttons=qdbus_cast<QList<double>>(interaction[1]); QCOMPARE(buttons.size(),12);
        auto click=[&](int button) {
            const auto current=globalPoint();
            const auto currentButtons=qdbus_cast<QList<double>>(call(service,path,service,"InteractionState").arguments()[1]);
            QCOMPARE(currentButtons.size(),12);
            input("NotifyPointerMotionRelative",{currentButtons[button*4]+currentButtons[button*4+2]/2-current.x(),
                currentButtons[button*4+1]+currentButtons[button*4+3]/2-current.y()});
            QTest::qWait(100);
            const auto hit=call(service,path,service,"InteractionState").arguments().value(4).toString();
            QCOMPARE(hit,button==0?QString("copy"):button==1?QString("pin"):QString("close"));
            QVERIFY(input("NotifyPointerButton",{int(272),true}).type()!=QDBusMessage::ErrorMessage);
            QVERIFY(input("NotifyPointerButton",{int(272),false}).type()!=QDBusMessage::ErrorMessage);
        };
        click(1); QTRY_VERIFY(call(service,path,service,"InteractionState").arguments()[0].toBool());
        const auto pinnedPoint=globalPoint();
        input("NotifyPointerMotionRelative",{20.0-pinnedPoint.x(),20.0-pinnedPoint.y()}); QTest::qWait(500);
        QVERIFY(status().value(2).toBool());
        click(0);
        QTRY_VERIFY(call(service,path,service,"InteractionState").arguments().value(3).toBool());
        // Wayland offers clipboard data to the focused native client. Clicking
        // back into the target exercises the same focus change as pasting into
        // another application after using a shell popup.
        const auto current=globalPoint(); input("NotifyPointerMotionRelative",{800.0-current.x(),800.0-current.y()});
        input("NotifyPointerButton",{int(272),true}); input("NotifyPointerButton",{int(272),false});
        QTRY_VERIFY2(QApplication::clipboard()->text().contains("你好世界"),qPrintable(QString("Clipboard=%1; popup=%2").arg(QApplication::clipboard()->text(),status().value(4).toString())));
        // Move back to the original hover point while pinned; Esc below must
        // close the pinned result without an immediate recapture.
        const auto pastePoint=globalPoint();
        input("NotifyPointerMotionRelative",{130.0-pastePoint.x(),340.0-pastePoint.y()}); QTest::qWait(100);
        const auto shown = popup.count();
        // Esc dismisses the shell popup without immediately rearming hover.
        input("NotifyKeyboardKeysym",{uint(0xff1b),true}); input("NotifyKeyboardKeysym",{uint(0xff1b),false});
        QTRY_VERIFY(!status().value(2).toBool()); QTest::qWait(500); QCOMPARE(popup.count(),shown);
        input("NotifyPointerMotionRelative",{0.0,100.0});
        QTRY_VERIFY(!status().value(2).toBool());
        // Changing modes uses the actual compositor crop, OCR word geometry
        // and backend settings through the version 4 extension protocol.
        // The screenshot's two distant headings stay separate in Line mode.
        QVERIFY(hover.configure(settings)); popup.clear();
        input("NotifyPointerMotionRelative",{0.0,-230.0});
        QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,12000);
        QCOMPARE(popup.last()[0].toString(),QString("Mode"));
        input("NotifyPointerMotionRelative",{180.0,0.0}); QTRY_VERIFY(!status().value(2).toBool());
        QTest::qWait(500); QCOMPARE(popup.count(),1);
        input("NotifyPointerMotionRelative",{-180.0,230.0});
        settings.textMode = "word"; QVERIFY(hover.configure(settings)); popup.clear();
        input("NotifyPointerMotionRelative",{0.0,-100.0});
        QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,12000);
        QCOMPARE(popup.last()[0].toString(),QString("Hello"));
        QVERIFY(popup.last()[1].toString().contains("你好"));
        if (qEnvironmentVariableIsSet("HOVER_GNOME_MODEL_PYTHON")) {
            settings.pythonPath = qEnvironmentVariable("HOVER_GNOME_MODEL_PYTHON");
            settings.packagesPath = qEnvironmentVariable("HOVER_GNOME_MODELS_DIR");
            settings.useDictionary = false;
            settings.textMode = "word"; QVERIFY(hover.configure(settings)); popup.clear();
            input("NotifyKeyboardKeysym",{uint(0xffe1),true}); input("NotifyKeyboardKeysym",{uint(0xffe3),true});
            input("NotifyPointerMotionRelative",{0.0,300.0});
            QTRY_VERIFY_WITH_TIMEOUT(popup.count()>0,30000);
            QCOMPARE(popup.last()[0].toString(),QString("This sentence continues on the next line."));
            QCOMPARE(HoverPolicy::sourceLanguage(popup.last()[1].toString()),QString("zh-CN"));
            input("NotifyKeyboardKeysym",{uint(0xffe3),false}); input("NotifyKeyboardKeysym",{uint(0xffe1),false});
            settings.textMode = "line";
            QVERIFY(hover.configure(settings)); popup.clear();
            input("NotifyPointerMotionRelative",{0.0,-300.0});
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
