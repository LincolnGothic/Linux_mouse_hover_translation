// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include "settingsdialog.h"
#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QNetworkProxyFactory>
#include <QSystemTrayIcon>
#include <QTextStream>

int main(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsSet("DISPLAY") && !qEnvironmentVariableIsSet("WAYLAND_DISPLAY")
        && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        bool commandLine = false;
        for (int i = 1; i < argc; ++i) {
            const QByteArray option(argv[i]);
            commandLine |= option == "--ocr" || option.startsWith("--ocr=")
                || option == "--translate" || option.startsWith("--translate=")
                || option == "--help" || option == "--help-all" || option == "-h"
                || option == "--version" || option == "-v";
        }
        if (commandLine) qputenv("QT_QPA_PLATFORM", "offscreen");
        else {
            QTextStream(stderr) << "The GUI requires a desktop display. Use --help for headless OCR and translation commands.\n";
            return 2;
        }
    }
    QApplication app(argc, argv);
    app.setApplicationName("HoverTranslate");
    app.setOrganizationName("LincolnGothic");
    app.setApplicationVersion("0.1.0");
    app.setWindowIcon(QIcon(":/icons/hover-translate.svg"));
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QCommandLineParser parser;
    parser.setApplicationDescription("Open-source X11 mouse-hover translation, English ↔ Simplified Chinese.\n"
        "GNU GPL version 3 or later; no warranty. See Settings → About & licenses.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "Use a specific settings file.", "file"},
        {"ocr", "Recognize an image locally and print line geometry as JSON.", "file"},
        {"translate", "Translate text through the configured Mozhi server.", "text"},
        {"target", "Target language: en or zh-CN.", "language"},
        {"instance", "Mozhi HTTPS server URL (HTTP allowed only on localhost).", "url"},
        {"paused", "Start with hover paused."},
        {"background", "Start in the tray when a tray is available."}
    });
    parser.process(app);
    SettingsStore store(parser.value("config"));
    auto settings = store.load();
    if (parser.isSet("target")) settings.target = parser.value("target");
    if (parser.isSet("instance")) settings.instance = parser.value("instance");
    if (parser.isSet("paused")) settings.enabled = false;
    QString error;
    if (!SettingsStore::validate(settings, &error)) {
        QTextStream(stderr) << error << '\n';
        return 2;
    }
    TesseractOcr ocr;
    TranslationService translator;
    if (parser.isSet("ocr")) {
        const QImage image(parser.value("ocr"));
        if (image.isNull() || !ocr.init("eng+chi_sim", settings.tessdataPath.toUtf8())) {
            QTextStream(stderr) << "Could not load the image or the eng+chi_sim OCR models.\n";
            return 2;
        }
        QObject::connect(&ocr, &TesseractOcr::linesRecognized, &app, [&app](const QVector<OcrLine> &lines) {
            QJsonArray output;
            for (const auto &line : lines)
                output.append(QJsonObject{{"text", line.text}, {"confidence", line.confidence},
                    {"x", line.bounds.x()}, {"y", line.bounds.y()}, {"width", line.bounds.width()}, {"height", line.bounds.height()}});
            QTextStream(stdout) << QJsonDocument(output).toJson(QJsonDocument::Compact) << '\n';
            app.exit(0);
        });
        QObject::connect(&ocr, &TesseractOcr::failed, &app, [&app](const QString &failure) {
            QTextStream(stderr) << failure << '\n'; app.exit(1);
        });
        QTimer::singleShot(0, &ocr, [&] { ocr.recognize(image, 96); });
        return app.exec();
    }
    if (parser.isSet("translate")) {
        const QString text = parser.value("translate");
        const QString source = HoverPolicy::sourceLanguage(text);
        if (source.isEmpty()) { QTextStream(stderr) << "Use English or Simplified Chinese text.\n"; return 2; }
        if (source == settings.target) { QTextStream(stdout) << text << '\n'; return 0; }
        QObject::connect(&translator, &TranslationService::translated, &app, [&app](quint64, const QString &result) {
            QTextStream(stdout) << result << '\n'; app.exit(0);
        });
        QObject::connect(&translator, &TranslationService::failed, &app, [&app](quint64, const QString &failure) {
            QTextStream(stderr) << failure << '\n'; app.exit(1);
        });
        QTimer::singleShot(0, &translator, [&] { translator.translate(1, text, source, settings.target, settings.instance); });
        return app.exec();
    }

    HoverController controller(&ocr, &translator);
    SettingsDialog dialog;
    dialog.setSettings(settings);
    controller.setIgnoredWidgets({&dialog});
    QObject::connect(&controller, &HoverController::statusChanged, &dialog, &SettingsDialog::setStatus);
    controller.configure(settings, &error);
    if (!error.isEmpty()) {
        settings.enabled = false;
        dialog.setSettings(settings);
        dialog.setStatus(error);
    }
    QSystemTrayIcon tray(app.windowIcon());
    QMenu menu;
    auto *pause = menu.addAction("Enable hover translation");
    pause->setCheckable(true);
    pause->setChecked(controller.enabled());
    auto *showSettings = menu.addAction("Settings…");
    menu.addSeparator();
    menu.addAction("Quit", &app, &QApplication::quit);
    tray.setContextMenu(&menu);
    tray.setToolTip("Hover Translate — English ↔ 简体中文");
    auto apply = [&](const HoverSettings &requested) {
        QString failure;
        if (!controller.configure(requested, &failure) || !store.save(requested, &failure)) {
            controller.configure(settings);
            dialog.setSettings(settings);
            pause->setChecked(controller.enabled());
            dialog.showProblem(failure);
            return;
        }
        settings = requested;
        dialog.setSettings(settings);
        pause->setChecked(controller.enabled());
    };
    QObject::connect(&dialog, &SettingsDialog::applyRequested, &app, apply);
    QObject::connect(pause, &QAction::triggered, &app, [&](bool enabled) {
        auto requested = settings; requested.enabled = enabled; apply(requested);
    });
    QObject::connect(showSettings, &QAction::triggered, &dialog, [&] { dialog.show(); dialog.raise(); dialog.activateWindow(); });
    QObject::connect(&tray, &QSystemTrayIcon::activated, &dialog, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            dialog.show(); dialog.raise(); dialog.activateWindow();
        }
    });
    // Server tests use their own translator and never cancel a hover request.
    TranslationService testClient;
    QObject::connect(&dialog, &SettingsDialog::testRequested, &app, [&](const HoverSettings &requested) {
        dialog.setStatus("Testing the server with “Hello”…");
        testClient.translate(1, "Hello", "en", "zh-CN", requested.instance);
    });
    QObject::connect(&testClient, &TranslationService::translated, &dialog, [&](quint64, const QString &result) {
        dialog.setStatus("Server responded: Hello → " + result);
    });
    QObject::connect(&testClient, &TranslationService::failed, &dialog, [&](quint64, const QString &failure) { dialog.setStatus(failure); });
    const bool haveTray = QSystemTrayIcon::isSystemTrayAvailable();
    if (haveTray) tray.show();
    app.setQuitOnLastWindowClosed(!haveTray);
    // Without a tray (e.g. a stock GNOME session), keep controls accessible.
    if (!parser.isSet("background") || !haveTray) dialog.show();
    return app.exec();
}
