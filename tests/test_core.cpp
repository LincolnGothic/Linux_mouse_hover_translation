// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hoverpolicy.h"
#include "settings.h"
#include "tesseractocr.h"
#include "translationservice.h"
#include "mockserver.h"
#include <QFile>
#include <QPainter>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class CoreTest : public QObject {
    Q_OBJECT
private slots:
    void dwellAndMovement();
    void blockedAndDismissed();
    void lineSelection();
    void fixedTargetLanguage();
    void settingsPersist();
    void settingsRejectInvalidEndpoints();
    void corruptSettingsDisableCapture();
    void englishAndChineseOcr();
    void missingModelFails();
    void translationBothDirections();
    void translationTimeoutAndRecovery();
    void malformedResponseFails();
    void abortedRequestCannotCompleteNext();
};

void CoreTest::dwellAndMovement()
{
    HoverPolicy policy;
    QCOMPARE(policy.update({100,100}, false, 0), HoverPolicy::Invalidated);
    const auto generation = policy.generation();
    QCOMPARE(policy.update({102,100}, false, 599), HoverPolicy::Idle);
    QCOMPARE(policy.update({100,100}, false, 600), HoverPolicy::Capture);
    QCOMPARE(policy.update({100,100}, false, 1800), HoverPolicy::Idle);
    QCOMPARE(policy.update({120,100}, false, 1900), HoverPolicy::Invalidated);
    QVERIFY(policy.generation() > generation);
    QCOMPARE(policy.update({120,100}, false, 2499), HoverPolicy::Idle);
    QCOMPARE(policy.update({120,100}, false, 2500), HoverPolicy::Capture);
}

void CoreTest::blockedAndDismissed()
{
    HoverPolicy policy;
    policy.update({100,100}, true, 0);
    QCOMPARE(policy.update({100,100}, true, 1000), HoverPolicy::Idle);
    QCOMPARE(policy.update({100,100}, false, 1100), HoverPolicy::Invalidated);
    QCOMPARE(policy.update({100,100}, false, 1700), HoverPolicy::Capture);
    const auto generation = policy.generation();
    policy.dismiss();
    QVERIFY(policy.generation() > generation);
    QCOMPARE(policy.update({100,100}, false, 5000), HoverPolicy::Idle);
    QCOMPARE(policy.update({110,100}, false, 5100), HoverPolicy::Invalidated);
}

void CoreTest::lineSelection()
{
    const QVector<OcrLine> lines{
        {"wrong paragraph", {10,10,200,25}, 98},
        {" Hello  world\n", {10,50,200,25}, 95},
        {"low confidence", {10,90,200,25}, 20}
    };
    QCOMPARE(HoverPolicy::lineAt(lines, {80,60}), QString("Hello world"));
    QVERIFY(HoverPolicy::lineAt(lines, {80,100}).isEmpty());
    QVERIFY(HoverPolicy::lineAt(lines, {260,60}).isEmpty());
}

void CoreTest::fixedTargetLanguage()
{
    QCOMPARE(HoverPolicy::sourceLanguage("Hello world"), QString("en"));
    QCOMPARE(HoverPolicy::sourceLanguage("你好，世界"), QString("zh-CN"));
    QCOMPARE(HoverPolicy::sourceLanguage("你好 Linux"), QString("zh-CN"));
    QVERIFY(HoverPolicy::sourceLanguage("1234 !").isEmpty());
    QVERIFY(HoverPolicy::sourceLanguage("こんにちは").isEmpty());
}

void CoreTest::settingsPersist()
{
    QTemporaryDir directory;
    SettingsStore store(directory.filePath("sub/settings.ini"));
    HoverSettings settings;
    settings.enabled = true; settings.target = "en"; settings.dwellMs = 900;
    settings.instance = "https://example.com/mozhi";
    QVERIFY(store.save(settings));
    const auto loaded = SettingsStore(store.fileName()).load();
    QCOMPARE(loaded.target, settings.target);
    QCOMPARE(loaded.instance, settings.instance);
    QCOMPARE(loaded.dwellMs, 900);
    QVERIFY(loaded.enabled);
}

void CoreTest::settingsRejectInvalidEndpoints()
{
    for (const auto &url : {"http://example.com", "https://user:password@example.com",
                           "file:///tmp/server", "https://example.com?token=example", "https://example.com/#fragment"})
        QVERIFY2(!SettingsStore::validInstance(url), url);
    QVERIFY(SettingsStore::validInstance("https://example.com"));
    QVERIFY(SettingsStore::validInstance("http://127.0.0.1:8000"));
    QVERIFY(SettingsStore::validInstance("http://[::1]:8000"));
}

void CoreTest::corruptSettingsDisableCapture()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("settings.ini");
    { QSettings settings(path, QSettings::IniFormat);
      settings.setValue("hover/enabled", true);
      settings.setValue("translation/target", "ja");
      settings.setValue("translation/instance", "http://example.com");
      settings.setValue("hover/dwellMs", -100);
    }
    const auto loaded = SettingsStore(path).load();
    QVERIFY(!loaded.enabled);
    QCOMPARE(loaded.target, QString("zh-CN"));
    QVERIFY(SettingsStore::validInstance(loaded.instance));
    QVERIFY(loaded.dwellMs >= 100);
}

void CoreTest::englishAndChineseOcr()
{
    QImage image(800, 230, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::black);
    painter.setFont(QFont("Noto Sans CJK SC", 30));
    painter.drawText(QRect(40,20,700,80), Qt::AlignLeft | Qt::AlignVCenter, "Hello world");
    painter.drawText(QRect(40,115,700,80), Qt::AlignLeft | Qt::AlignVCenter, "你好世界");
    painter.end();
    TesseractOcr ocr;
    QVERIFY(ocr.init("eng+chi_sim", {}));
    QSignalSpy result(&ocr, &TesseractOcr::linesRecognized);
    QVector<OcrLine> lines;
    connect(&ocr, &TesseractOcr::linesRecognized, this, [&](const auto &recognized) { lines = recognized; });
    ocr.recognize(image, 96);
    QVERIFY(result.wait(8000));
    bool english = false, chinese = false;
    for (const auto &line : lines) {
        english |= line.text.contains("Hello world");
        auto text = line.text; text.remove(' ');
        chinese |= text.contains("你好世界");
        QVERIFY(line.bounds.intersects(image.rect()));
        QVERIFY(line.confidence >= 0);
    }
    QVERIFY(english);
    QVERIFY(chinese);
}

void CoreTest::missingModelFails()
{
    QTemporaryDir directory;
    const QString eng = qEnvironmentVariable("TESSDATA_PREFIX") + "/eng.traineddata";
    const QString fallback = "/usr/share/tesseract-ocr/5/tessdata/eng.traineddata";
    QVERIFY(QFile::copy(QFile::exists(eng) ? eng : fallback, directory.filePath("eng.traineddata")));
    TesseractOcr ocr;
    QVERIFY(!ocr.init("eng+chi_sim", directory.path().toUtf8()));
    QVERIFY(!ocr.isConfigured());
}

void CoreTest::translationBothDirections()
{
    MockServer server;
    TranslationService client;
    QSignalSpy success(&client, &TranslationService::translated);
    QSignalSpy failure(&client, &TranslationService::failed);
    client.translate(1, "Hello world", "en", "zh-CN", server.url());
    QVERIFY(success.wait(3000));
    QCOMPARE(success.last().at(1).toString(), QString("你好，世界"));
    client.translate(2, "你好世界", "zh-CN", "en", server.url());
    QVERIFY(success.wait(3000));
    QCOMPARE(success.last().at(1).toString(), QString("Hello world"));
    QCOMPARE(failure.count(), 0);
    QCOMPARE(server.requests.size(), 2);
    const QUrlQuery first(server.requests[0]), second(server.requests[1]);
    QCOMPARE(first.queryItemValue("engine"), QString("google"));
    QCOMPARE(first.queryItemValue("from"), QString("en"));
    QCOMPARE(first.queryItemValue("to"), QString("zh-CN"));
    QCOMPARE(second.queryItemValue("from"), QString("zh-CN"));
    QCOMPARE(second.queryItemValue("to"), QString("en"));
    QCOMPARE(second.queryItemValue("text", QUrl::FullyDecoded), QString("你好世界"));
}

void CoreTest::translationTimeoutAndRecovery()
{
    MockServer server; server.delayMs = 300;
    TranslationService client; client.setTimeoutMs(80);
    QSignalSpy failure(&client, &TranslationService::failed);
    QSignalSpy success(&client, &TranslationService::translated);
    client.translate(1, "Hello", "en", "zh-CN", server.url());
    QVERIFY(failure.wait(1000));
    QCOMPARE(success.count(), 0);
    server.delayMs = 0;
    client.translate(2, "Hello", "en", "zh-CN", server.url());
    QVERIFY(success.wait(1000));
    QCOMPARE(success.last().at(0).toULongLong(), 2ULL);
    QTest::qWait(350);
    QCOMPARE(success.count(), 1);
    QCOMPARE(failure.count(), 1);
}

void CoreTest::malformedResponseFails()
{
    MockServer server; server.malformed = true;
    TranslationService client;
    QSignalSpy failure(&client, &TranslationService::failed);
    QSignalSpy success(&client, &TranslationService::translated);
    client.translate(1, "Hello", "en", "zh-CN", server.url());
    QVERIFY(failure.wait(1000));
    QCOMPARE(success.count(), 0);
}

void CoreTest::abortedRequestCannotCompleteNext()
{
    MockServer server; server.delayMs = 200;
    TranslationService client;
    QSignalSpy request(&server, &MockServer::received);
    QSignalSpy success(&client, &TranslationService::translated);
    client.translate(1, "Hello", "en", "zh-CN", server.url());
    QVERIFY(request.wait(1000));
    server.delayMs = 0;
    client.translate(2, "你好", "zh-CN", "en", server.url());
    QVERIFY(success.wait(1000));
    QCOMPARE(success.last().at(0).toULongLong(), 2ULL);
    QTest::qWait(300);
    QCOMPARE(success.count(), 1);
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
