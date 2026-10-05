// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hovercontroller.h"
#include "settingsdialog.h"
#include "screenreader.h"
#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QProcess>
#include <QDir>
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
    app.setApplicationVersion("0.4.1");
    app.setDesktopFileName("io.github.LincolnGothic.HoverTranslate");
    app.setWindowIcon(QIcon(":/icons/hover-translate.svg"));
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QCommandLineParser parser;
    parser.setApplicationDescription("Offline English ↔ Simplified Chinese translation. GNOME Wayland hover with the extension, X11 hover, and screen-region capture.\n"
        "GNU GPL version 3 or later; no warranty. See Settings → About & licenses.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{"c", "config"}, "Use a specific settings file.", "file"},
        {"ocr", "Recognize an image locally and print line geometry as JSON.", "file"},
        {"translate", "Translate text locally by default.", "text"},
        {"provider", "Translation provider: offline (default) or mozhi.", "provider"},
        {"python", "Offline Python executable (normally detected automatically).", "file"},
        {"models-dir", "Folder containing installed Argos packages.", "directory"},
        {"dictionary", "CC-CEDICT text file for word definitions.", "file"},
        {"no-dictionary", "Use sentence translation instead of dictionary lookup."},
        {"capture", "Open the desktop screenshot dialog and translate a selected region."},
        {"reader", "Open typed-text translation without automatic hover."},
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
    if (parser.isSet("instance") && !parser.isSet("provider")) settings.provider = "mozhi";
    if (parser.isSet("provider")) settings.provider = parser.value("provider");
    if (parser.isSet("python")) settings.pythonPath = parser.value("python");
    if (parser.isSet("models-dir")) settings.packagesPath = parser.value("models-dir");
    if (parser.isSet("dictionary")) settings.dictionaryPath = parser.value("dictionary");
    if (parser.isSet("no-dictionary")) settings.useDictionary = false;
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
        QTimer::singleShot(0, &translator, [&] { translator.translate(1, text, source, settings.target, settings); });
        return app.exec();
    }

    ScreenReader reader;
    reader.configure(settings);
    if (parser.isSet("capture") || parser.isSet("reader")) {
        reader.show();
        if (parser.isSet("capture")) QTimer::singleShot(0, &reader, &ScreenReader::capture);
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
    pause->setEnabled(HoverController::platformProblem().isEmpty());
    pause->setToolTip(HoverController::platformProblem());
    QObject::connect(&controller, &HoverController::availabilityChanged, &dialog, [&] {
        const auto problem = HoverController::platformProblem();
        dialog.setHoverAvailability(problem, controller.enabled());
        pause->setEnabled(problem.isEmpty()); pause->setToolTip(problem); pause->setChecked(controller.enabled());
    });
    auto *showSettings = menu.addAction("Settings…");
    auto *captureRegion = menu.addAction("Translate screen region…");
    auto *translateText = menu.addAction("Translate text…");
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
        reader.configure(settings);
        dialog.setSettings(settings);
        pause->setChecked(controller.enabled());
    };
    QObject::connect(&dialog, &SettingsDialog::applyRequested, &app, apply);
    QObject::connect(pause, &QAction::triggered, &app, [&](bool enabled) {
        auto requested = settings; requested.enabled = enabled; apply(requested);
    });
    QObject::connect(showSettings, &QAction::triggered, &dialog, [&] { dialog.show(); dialog.raise(); dialog.activateWindow(); });
    auto openReader = [&](const HoverSettings &requested, bool capture) {
        QString failure;
        if (!SettingsStore::validate(requested, &failure)) { dialog.showProblem(failure); return; }
        reader.configure(requested); reader.show();
        if (capture) { dialog.hide(); reader.capture(); }
    };
    QObject::connect(&dialog, &SettingsDialog::readerRequested, &reader, [&](const HoverSettings &requested) { openReader(requested, false); });
    QObject::connect(&dialog, &SettingsDialog::captureRequested, &reader, [&](const HoverSettings &requested) { openReader(requested, true); });
    QObject::connect(captureRegion, &QAction::triggered, &reader, [&] { openReader(settings, true); });
    QObject::connect(translateText, &QAction::triggered, &reader, [&] { openReader(settings, false); });
    QObject::connect(&tray, &QSystemTrayIcon::activated, &dialog, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            dialog.show(); dialog.raise(); dialog.activateWindow();
        }
    });
    // Settings tests use their own translator and never cancel a hover request.
    TranslationService testClient;
    QObject::connect(&dialog, &SettingsDialog::testRequested, &app, [&](const HoverSettings &requested) {
        const QString sample = requested.target == "en" ? QString("你好世界") : QString("Hello world");
        const QString source = requested.target == "en" ? QString("zh-CN") : QString("en");
        dialog.setStatus("Testing translation with “" + sample + "”…");
        testClient.translate(1, sample, source, requested.target, requested);
    });
    QObject::connect(&testClient, &TranslationService::translated, &dialog, [&](quint64, const QString &result) {
        dialog.setStatus("Translation test result: " + result);
        testClient.resetOffline();
    });
    QObject::connect(&testClient, &TranslationService::failed, &dialog, [&](quint64, const QString &failure) { dialog.setStatus(failure); testClient.resetOffline(); });
    QProcess installer;
    QString setupDetail;
    QObject::connect(&dialog, &SettingsDialog::setupRequested, &app, [&] {
        if (installer.state() != QProcess::NotRunning) { dialog.setStatus("Offline setup is already running."); return; }
        const QString helper = offlineAsset("setup_offline.py");
        if (helper.isEmpty()) { dialog.showProblem("The offline setup helper is missing. Reinstall Hover Translate."); return; }
        QDir().mkpath(offlineDataDirectory());
        installer.setProcessChannelMode(QProcess::MergedChannels);
        setupDetail.clear();
        dialog.setStatus("Downloading the offline runtime and English / Chinese models. This first setup may take several minutes.");
        installer.start("python3", {"-u", helper, "--data-dir", offlineDataDirectory()});
    });
    QObject::connect(&installer, &QProcess::readyReadStandardOutput, &dialog, [&] {
        setupDetail = (setupDetail + QString::fromUtf8(installer.readAllStandardOutput())).right(8000);
        const auto lines = setupDetail.trimmed().split('\n');
        if (!lines.isEmpty()) dialog.setStatus(lines.last().left(500));
    });
    QObject::connect(&installer, &QProcess::errorOccurred, &dialog, [&](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) dialog.setStatus("Could not start setup. Install python3 and python3-venv.");
    });
    QObject::connect(&installer, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &dialog, [&](int code, QProcess::ExitStatus status) {
        dialog.setStatus(status == QProcess::NormalExit && code == 0
            ? "Offline models installed. Use Test translation to check the selected direction."
            : "Offline setup failed: " + setupDetail.trimmed().split('\n').last().left(700)
                + "\nRun hover-translate-offline-setup in a terminal for full details.");
    });
    QProcess gnomeInstaller;
    QString gnomeDetail;
    QObject::connect(&dialog, &SettingsDialog::gnomeSetupRequested, &app, [&] {
        if (gnomeInstaller.state() != QProcess::NotRunning) return;
        const auto helper = offlineAsset("setup_gnome.py");
        if (helper.isEmpty()) { dialog.showProblem("GNOME setup helper is missing. Reinstall Hover Translate."); return; }
        gnomeDetail.clear(); gnomeInstaller.setProcessChannelMode(QProcess::MergedChannels);
        dialog.setStatus("Setting up the GNOME hover extension for your account…");
        gnomeInstaller.start("python3", {"-u", helper});
    });
    QObject::connect(&gnomeInstaller, &QProcess::readyReadStandardOutput, &dialog, [&] {
        gnomeDetail = (gnomeDetail + QString::fromUtf8(gnomeInstaller.readAllStandardOutput())).right(8000);
    });
    QObject::connect(&gnomeInstaller, &QProcess::errorOccurred, &dialog, [&](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) dialog.setStatus("Could not start GNOME setup. Install python3.");
    });
    QObject::connect(&gnomeInstaller, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &dialog,
        [&](int, QProcess::ExitStatus) { dialog.setStatus(gnomeDetail.trimmed()); });
    const bool haveTray = QSystemTrayIcon::isSystemTrayAvailable();
    if (haveTray) tray.show();
    app.setQuitOnLastWindowClosed(!haveTray);
    // Without a tray (e.g. a stock GNOME session), keep controls accessible.
    if (!parser.isSet("background") || !haveTray) dialog.show();
    return app.exec();
}
