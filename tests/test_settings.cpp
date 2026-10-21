// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSpinBox>
#include <QStyleOptionSlider>
#include <QTabWidget>
#include <QWheelEvent>
#include <QtTest>

class SettingsTest : public QObject {
    Q_OBJECT
private slots:
    void smallWindow_data();
    void smallWindow();
    void retainedSettingsAndActions();
    void providerSpecificFields();
};

void SettingsTest::smallWindow_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("fontSize");
    QTest::newRow("small") << QSize(420, 440) << 11;
    QTest::newRow("large-font") << QSize(480, 500) << 16;
    QTest::newRow("standard") << QSize(640, 700) << 11;
    QTest::newRow("dark") << QSize(640, 700) << 11;
}

void SettingsTest::smallWindow()
{
    QFETCH(QSize, size);
    QFETCH(int, fontSize);
    const auto originalPalette = qApp->palette();
    const auto restorePalette = qScopeGuard([originalPalette] { qApp->setPalette(originalPalette); });
    if (QByteArray(QTest::currentDataTag()) == "dark") {
        QPalette dark = originalPalette;
        dark.setColor(QPalette::Window, QColor("#162024"));
        dark.setColor(QPalette::Base, QColor("#1d2a2e"));
        dark.setColor(QPalette::Button, QColor("#1d2a2e"));
        for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
            dark.setColor(role, QColor("#e3eeeb"));
        qApp->setPalette(dark);
    }
    SettingsDialog dialog;
#ifdef Q_OS_WIN
    QVERIFY(dialog.findChild<QPushButton *>("gnomeSetupButton")->isHidden());
#endif
    QFont font = dialog.font(); font.setPointSize(fontSize); dialog.setFont(font);
    dialog.setSettings(HoverSettings{});
    dialog.setStatus("Hover translation is paused. Choose your settings, then click Apply.");
    dialog.resize(size);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    QCOMPARE(dialog.size(), size);
    auto *tabs = dialog.findChild<QTabWidget *>("settingsTabs");
    QVERIFY(tabs);
    const auto footer = dialog.findChild<QPushButton *>("applyButton")->geometry();
    const QString screenshots = qEnvironmentVariable("HOVER_SETTINGS_SCREENSHOTS");
    if (!screenshots.isEmpty()) QVERIFY(QDir().mkpath(screenshots));
    bool exercisedWheel = false;
    for (int i = 0; i < tabs->count(); ++i) {
        tabs->setCurrentIndex(i);
        if (i == 1 && QByteArray(QTest::currentDataTag()) == "dark") {
            auto *provider = dialog.findChild<QComboBox *>("providerComboBox");
            provider->setCurrentIndex(provider->findData("google"));
        }
        QTest::qWait(30);
        auto *scroll = qobject_cast<QScrollArea *>(tabs->currentWidget());
        QVERIFY(scroll);
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        QVERIFY(!scroll->horizontalScrollBar()->isVisible());
        QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
        for (const auto &name : {"applyButton", "closeButton", "regionButton", "textButton"}) {
            auto *button = dialog.findChild<QPushButton *>(name);
            QVERIFY(button && button->isVisible());
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
        }
        QCOMPARE(dialog.findChild<QPushButton *>("applyButton")->geometry(), footer);
        auto *bar = scroll->verticalScrollBar();
        if (bar->maximum() > 0) {
            const QPointF point = scroll->viewport()->rect().center();
            QWheelEvent wheel(point, scroll->viewport()->mapToGlobal(point.toPoint()),
                              {}, {0, -120}, Qt::NoButton, Qt::NoModifier,
                              Qt::NoScrollPhase, false);
            QApplication::sendEvent(scroll->viewport(), &wheel);
            QTRY_VERIFY(bar->value() > 0);
            exercisedWheel = true;
            bar->setValue(0);
            QStyleOptionSlider option;
            option.initFrom(bar); option.orientation = Qt::Vertical;
            option.minimum = bar->minimum(); option.maximum = bar->maximum();
            option.pageStep = bar->pageStep(); option.singleStep = bar->singleStep();
            option.sliderPosition = option.sliderValue = 0;
            const auto thumb = bar->style()->subControlRect(
                QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, bar);
            QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, thumb.center());
            QTest::mouseMove(bar, QPoint(thumb.center().x(), bar->height() - 25), 30);
            QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier,
                                QPoint(thumb.center().x(), bar->height() - 25));
            QTRY_VERIFY(bar->value() > 0);
            bar->setValue(bar->maximum());
            QTest::qWait(20);
#ifdef Q_OS_WIN
            QWidget *last = i == 0 ? dialog.findChild<QWidget *>("windowsHoverLabel")
#else
            QWidget *last = i == 0 ? dialog.findChild<QPushButton *>("gnomeSetupButton")
#endif
                : i == 1 ? dialog.findChild<QPushButton *>("testTranslationButton")
                : dialog.findChild<QWidget *>("dictionaryLineEdit");
            QVERIFY(last);
            QVERIFY(scroll->viewport()->rect().contains(
                QRect(last->mapTo(scroll->viewport(), QPoint()), last->size())));
            bar->setValue(0);
        }
        if (!screenshots.isEmpty())
            QVERIFY(dialog.grab().save(screenshots + QString("/%1-%2.png")
                .arg(QString::fromLatin1(QTest::currentDataTag())).arg(i)));
    }
    if (size.height() < 600) QVERIFY(exercisedWheel);
}

void SettingsTest::retainedSettingsAndActions()
{
    SettingsDialog dialog;
    HoverSettings initial;
    initial.target = "en"; initial.textMode = "sentence"; initial.dwellMs = 200;
    initial.pythonPath = "/opt/offline/python"; initial.packagesPath = "/opt/offline/models";
    initial.dictionaryPath = "/opt/offline/cedict.txt"; initial.tessdataPath = "/opt/ocr";
    initial.highlightSource = false; initial.temporaryModes = false;
    dialog.setSettings(initial);
    QCOMPARE(dialog.settings().textMode, initial.textMode);
    QCOMPARE(dialog.settings().pythonPath, initial.pythonPath);
    QCOMPARE(dialog.settings().packagesPath, initial.packagesPath);
    QCOMPARE(dialog.settings().dictionaryPath, initial.dictionaryPath);
    QCOMPARE(dialog.settings().tessdataPath, initial.tessdataPath);
    QVERIFY(!dialog.settings().highlightSource);
    QVERIFY(!dialog.settings().temporaryModes);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    auto *tabs = dialog.findChild<QTabWidget *>("settingsTabs");
    auto *mode = dialog.findChild<QComboBox *>("textModeComboBox");
    mode->setCurrentIndex(mode->findData("word"));
    int applied = 0;
    connect(&dialog, &SettingsDialog::applyRequested, this, [&](const HoverSettings &settings) {
        ++applied; QCOMPARE(settings.textMode, QString("word"));
        QCOMPARE(settings.dwellMs, 200); QCOMPARE(settings.dictionaryPath, initial.dictionaryPath);
    });
    QTest::mouseClick(dialog.findChild<QPushButton *>("applyButton"), Qt::LeftButton);
    QCOMPARE(applied, 1);
    QSignalSpy capture(&dialog, &SettingsDialog::captureRequested);
    QSignalSpy reader(&dialog, &SettingsDialog::readerRequested);
    QTest::mouseClick(dialog.findChild<QPushButton *>("regionButton"), Qt::LeftButton);
    QTest::mouseClick(dialog.findChild<QPushButton *>("textButton"), Qt::LeftButton);
    QCOMPARE(capture.count(), 1); QCOMPARE(reader.count(), 1);
    tabs->setCurrentIndex(1);
    auto *provider = dialog.findChild<QComboBox *>("providerComboBox");
    auto *server = dialog.findChild<QLineEdit *>("serverLineEdit");
    QVERIFY(!server->isEnabled());
    provider->setCurrentIndex(provider->findData("mozhi"));
    QVERIFY(server->isEnabled());
    provider->setCurrentIndex(provider->findData("offline"));
    QVERIFY(!server->isEnabled());
    QSignalSpy test(&dialog, &SettingsDialog::testRequested);
    QTest::mouseClick(dialog.findChild<QPushButton *>("testTranslationButton"), Qt::LeftButton);
    QCOMPARE(test.count(), 1);
    QTest::mouseClick(dialog.findChild<QPushButton *>("closeButton"), Qt::LeftButton);
    QVERIFY(!dialog.isVisible());
}

void SettingsTest::providerSpecificFields()
{
    SettingsDialog dialog; dialog.setSettings(HoverSettings{});
    auto *provider = dialog.findChild<QComboBox *>("providerComboBox");
    auto *server = dialog.findChild<QLineEdit *>("serverLineEdit");
    auto *credentials = dialog.findChild<QPushButton *>("azureCredentialsButton");
    auto *dictionary = dialog.findChild<QLineEdit *>("dictionaryLineEdit");
    auto *setup = dialog.findChild<QPushButton *>("offlineSetupButton");
    QVERIFY(credentials);
    QVERIFY(server->isHidden()); QVERIFY(credentials->isHidden());
    QVERIFY(!dictionary->isHidden()); QVERIFY(!setup->isHidden());
    provider->setCurrentIndex(provider->findData("azure"));
    QCOMPARE(dialog.settings().provider, QString("azure"));
    QVERIFY(server->isHidden()); QVERIFY(!credentials->isHidden());
    QVERIFY(dictionary->isHidden()); QVERIFY(setup->isHidden());
    provider->setCurrentIndex(provider->findData("mozhi"));
    QVERIFY(!server->isHidden()); QVERIFY(credentials->isHidden());
    QVERIFY(dictionary->isHidden()); QVERIFY(setup->isHidden());
    provider->setCurrentIndex(provider->findData("google"));
    QCOMPARE(dialog.settings().provider, QString("google"));
    QVERIFY(server->isHidden()); QVERIFY(credentials->isHidden());
    QVERIFY(dictionary->isHidden()); QVERIFY(setup->isHidden());
    const auto screenshots = qEnvironmentVariable("HOVER_SETTINGS_SCREENSHOTS");
    if (!screenshots.isEmpty()) {
        dialog.findChild<QTabWidget *>("settingsTabs")->setCurrentIndex(1);
        dialog.setStatus("Google Translate is ready. Screenshots stay on this computer.");
        dialog.resize(640, 700); dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(dialog.grab().save(screenshots + "/google-translation.png"));
    }
}

QTEST_MAIN(SettingsTest)
#include "test_settings.moc"
