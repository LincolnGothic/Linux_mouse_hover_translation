// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "settingsdialog.h"
#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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
};

void SettingsTest::smallWindow_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<int>("fontSize");
    QTest::newRow("small") << QSize(420, 440) << 11;
    QTest::newRow("large-font") << QSize(480, 500) << 16;
    QTest::newRow("standard") << QSize(640, 700) << 11;
}

void SettingsTest::smallWindow()
{
    QFETCH(QSize, size);
    QFETCH(int, fontSize);
    SettingsDialog dialog;
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
            QWidget *last = i == 0 ? dialog.findChild<QPushButton *>("gnomeSetupButton")
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

QTEST_MAIN(SettingsTest)
#include "test_settings.moc"
