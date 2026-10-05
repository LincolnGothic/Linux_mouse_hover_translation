// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hoverpolicy.h"
#include "settings.h"
#include "worddictionary.h"
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
    void wordAndSentenceSelection();
    void wrappedSentenceOcr();
    void separatedTextAndChineseWords();
    void fixedTargetLanguage();
    void settingsPersist();
    void settingsRejectInvalidEndpoints();
    void corruptSettingsDisableCapture();
    void englishAndChineseOcr();
    void smallScreenTextOcr_data();
    void smallScreenTextOcr();
    void missingModelFails();
    void translationBothDirections();
    void translationTimeoutAndRecovery();
    void malformedResponseFails();
    void abortedRequestCannotCompleteNext();
    void offlineDefaultAndPersistence();
    void offlineDictionaryBothDirectionsAndCancellation();
    void missingOfflinePythonFails();
    void offlineBusyRequestCancellation();
    void offlineTimeoutAndRecovery();
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

void CoreTest::wordAndSentenceSelection()
{
    QVector<OcrLine> lines{
        {"Previous sentence. This sentence", {10,10,300,20}, 95,
            {{"Previous",{10,10,70,20},95,0},{"sentence.",{85,10,85,20},95,9},{"This",{180,10,40,20},95,19},{"sentence",{225,10,85,20},95,24}}, 0, 0},
        {"wraps onto another line. Next sentence.", {10,40,300,20}, 95,
            {{"wraps",{10,40,45,20},95,0},{"onto",{60,40,40,20},95,6},{"another",{105,40,50,20},95,11},{"line.",{160,40,45,20},95,19},{"Next",{210,40,40,20},95,25},{"sentence.",{255,40,60,20},95,30}}, 0, 0},
        {"Different paragraph.", {10,70,200,20},95,{{"Different",{10,70,85,20},95,0}},0,1}
    };
    QCOMPARE(HoverPolicy::textAt(lines,{25,45},"word"),QString("wraps"));
    QCOMPARE(HoverPolicy::textAt(lines,{25,45},"line"),lines[1].text);
    QCOMPARE(HoverPolicy::textAt(lines,{25,45},"sentence"),QString("This sentence wraps onto another line."));
    QCOMPARE(HoverPolicy::textAt(lines,{220,45},"sentence"),QString("Next sentence."));
    QVERIFY(HoverPolicy::textAt(lines,{157,45},"word").isEmpty());
    QVERIFY(HoverPolicy::textAt(lines,{25,100},"sentence").isEmpty());
    auto altered = lines;
    altered[0].paragraph = 4;
    QCOMPARE(HoverPolicy::textAt(altered,{25,45},"sentence"),QString("wraps onto another line."));
    altered = lines; altered[1].bounds.translate(400,0); for (auto &word:altered[1].words) word.bounds.translate(400,0);
    QCOMPARE(HoverPolicy::textAt(altered,{425,45},"sentence"),QString("wraps onto another line."));
    altered = lines; altered[1].bounds.translate(0,90); for (auto &word:altered[1].words) word.bounds.translate(0,90);
    QCOMPARE(HoverPolicy::textAt(altered,{25,135},"sentence"),QString("wraps onto another line."));
    altered = lines; altered[0].confidence = 20;
    QCOMPARE(HoverPolicy::textAt(altered,{25,45},"sentence"),QString("wraps onto another line."));
    // Never expand an unpunctuated paragraph beyond three lines.
    QVector<OcrLine> paragraph;
    for (int i=0;i<6;++i) paragraph.append({"A fragment without punctuation",{10,10+i*30,300,20},95,
        {{"fragment",{30,10+i*30,70,20},95,2}},0,0});
    QCOMPARE(HoverPolicy::textAt(paragraph,{40,75},"sentence"),paragraph[2].text);
    paragraph[2].text = QString(301,'a');
    QVERIFY(HoverPolicy::textAt(paragraph,{40,75},"sentence").isEmpty());
    const QVector<OcrLine> wrapped{
        {"A hyphenated trans-",{10,10,230,20},95,{{"trans-",{180,10,60,20},95,13}},0,0},
        {"lation across three",{10,40,230,20},95,{{"lation",{10,40,60,20},95,0}},0,0},
        {"lines is complete.",{10,70,230,20},95,{{"lines",{10,70,50,20},95,0}},0,0}
    };
    QCOMPARE(HoverPolicy::textAt(wrapped,{20,45},"sentence"),QString("A hyphenated translation across three lines is complete."));
    const QVector<OcrLine> chinese{{"第一句。第二句！",{10,10,200,20},95,
        {{"第二句",{100,10,60,20},95,4}},0,0}};
    QCOMPARE(HoverPolicy::textAt(chinese,{120,15},"sentence"),QString("第二句！"));
    const QVector<OcrLine> punctuation{{"(world!)",{10,10,100,20},95,
        {{"(world!)",{10,10,100,20},95,0}},0,0}};
    QCOMPARE(HoverPolicy::textAt(punctuation,{20,15},"word"),QString("world"));
}

void CoreTest::wrappedSentenceOcr()
{
    QImage image(650,170,QImage::Format_RGB32); image.fill(Qt::white);
    QPainter painter(&image); QFont font("Noto Sans CJK SC"); font.setPixelSize(24);
    painter.setFont(font); painter.setPen(Qt::black);
    painter.drawText(20,50,"This sentence continues");
    painter.drawText(20,85,"on the next line."); painter.end();
    TesseractOcr ocr; QVERIFY(ocr.init("eng+chi_sim",{}));
    QSignalSpy recognized(&ocr,&TesseractOcr::linesRecognized);
    ocr.recognizeLayout(image,96,true); QVERIFY(recognized.wait(8000));
    const auto lines = qvariant_cast<QVector<OcrLine>>(recognized[0][0]);
    bool checked = false;
    for (const auto &line : lines) for (const auto &word : line.words) if (word.text == "continues") {
        QVERIFY(image.rect().contains(word.bounds));
        QCOMPARE(HoverPolicy::textAt(lines,word.bounds.center(),"word"),QString("continues"));
        QCOMPARE(HoverPolicy::textAt(lines,word.bounds.center(),"sentence"),QString("This sentence continues on the next line."));
        checked = true;
    }
    QVERIFY(checked);
}

void CoreTest::separatedTextAndChineseWords()
{
    for (const int scale:{1,2}) {
        QVector<OcrLine> table{{"Mode What it translates",{10,10,400,20},95,
            {{"Mode",{10,10,40,20},95,0},{"What",{260,10,40,20},95,5},
             {"it",{305,10,15,20},95,10},{"translates",{325,10,85,20},95,13}},0,0}};
        auto scaled=[scale](QRect r) { return QRect(r.topLeft()*scale,r.size()*scale); };
        table[0].bounds=scaled(table[0].bounds);
        for (auto &word:table[0].words) word.bounds=scaled(word.bounds);
        for (const auto &mode:{"line","sentence"}) {
            QCOMPARE(HoverPolicy::textAt(table,QPoint(20,15)*scale,mode),QString("Mode"));
            QCOMPARE(HoverPolicy::textAt(table,QPoint(270,15)*scale,mode),QString("What it translates"));
            QVERIFY(HoverPolicy::textAt(table,QPoint(150,15)*scale,mode).isEmpty());
        }
        const auto boxes=HoverPolicy::sourceBounds(table,QPoint(270,15)*scale,"What it translates");
        QVERIFY(!boxes.isEmpty()); for (const auto &box:boxes) QVERIFY(box.left()>=260*scale);
    }
    QTemporaryDir directory; QFile dictionary(directory.filePath("cedict.u8")); QVERIFY(dictionary.open(QIODevice::WriteOnly));
    dictionary.write(QString("# test-authored definitions\n你好 你好 [ni3 hao3] /hello/\n世界 世界 [shi4 jie4] /world/\n").toUtf8()); dictionary.close();
    WordDictionary words; words.load(dictionary.fileName());
    QVector<OcrLine> chinese{{"你好世界",{10,10,80,20},95,
        {{"你好世界",{10,10,80,20},95,0,
          {{"你",{10,10,20,20},95,0},{"好",{30,10,20,20},95,1},
           {"世",{50,10,20,20},95,2},{"界",{70,10,20,20},95,3}}}},0,0}};
    QCOMPARE(words.chineseAt(chinese,{35,15}),QString("你好"));
    QCOMPARE(words.chineseAt(chinese,{55,15}),QString("世界"));
    const auto boxes=HoverPolicy::sourceBounds(chinese,{35,15},"你好");
    QCOMPARE(boxes.size(),1); QCOMPARE(boxes[0],QRect(10,10,40,20));
    const QVector<OcrLine> separated{{"你 好",{10,10,210,20},95,
        {{"你",{10,10,20,20},95,0,{{"你",{10,10,20,20},95,0}}},
         {"好",{200,10,20,20},95,2,{{"好",{200,10,20,20},95,2}}}},0,0}};
    QVERIFY(words.chineseAt(separated,{15,15}).isEmpty());
    QVERIFY(words.chineseAt(separated,{205,15}).isEmpty());
    QCOMPARE(HoverSettings{}.textMode,QString("word"));
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
    settings.instance = "https://example.com/mozhi"; settings.textMode = "word";
    settings.highlightSource=false; settings.temporaryModes=false;
    QVERIFY(store.save(settings));
    const auto loaded = SettingsStore(store.fileName()).load();
    QCOMPARE(loaded.target, settings.target);
    QCOMPARE(loaded.instance, settings.instance);
    QCOMPARE(loaded.dwellMs, 900);
    QCOMPARE(loaded.textMode, QString("word"));
    QVERIFY(!loaded.highlightSource); QVERIFY(!loaded.temporaryModes);
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
    HoverSettings settings; settings.textMode = "paragraph";
    QVERIFY(!SettingsStore::validate(settings));
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
    QTemporaryDir directory; QFile dictionary(directory.filePath("cedict.u8")); QVERIFY(dictionary.open(QIODevice::WriteOnly));
    dictionary.write(QString("# authored fixture\n你好 你好 [ni3 hao3] /hello/\n世界 世界 [shi4 jie4] /world/\n").toUtf8()); dictionary.close();
    WordDictionary words; words.load(dictionary.fileName()); bool matched=false;
    for (const auto &line:lines) for (const auto &word:line.words) for (const auto &symbol:word.symbols) if (symbol.text=="好") {
        QCOMPARE(words.chineseAt(lines,symbol.bounds.center()),QString("你好")); matched=true;
    }
    QVERIFY(matched);
}

void CoreTest::smallScreenTextOcr_data()
{
    QTest::addColumn<int>("pixels"); QTest::addColumn<bool>("dark");
    QTest::newRow("small-light") << 10 << false;
    QTest::newRow("small-dark") << 10 << true;
    QTest::newRow("normal-dark") << 14 << true;
}

void CoreTest::smallScreenTextOcr()
{
    QFETCH(int,pixels); QFETCH(bool,dark);
    QImage image(280,35,QImage::Format_RGB32); image.fill(dark ? QColor("#202124") : Qt::white);
    QPainter painter(&image); QFont font("Noto Sans CJK SC"); font.setPixelSize(pixels);
    painter.setFont(font); painter.setPen(dark ? Qt::white : Qt::black);
    painter.drawText(2,22,"Hello world"); painter.end();
    TesseractOcr ocr; QVERIFY(ocr.init("eng+chi_sim",{}));
    QSignalSpy recognized(&ocr,&TesseractOcr::linesRecognized);
    ocr.recognize(image,96); QVERIFY(recognized.wait(8000));
    const auto lines = qvariant_cast<QVector<OcrLine>>(recognized[0][0]);
    bool found = false;
    for (const auto &line : lines) if (line.text.contains("Hello world")) {
        found = true; QVERIFY(line.confidence >= 35);
        // Upscaling and padding must not shift hover hit testing.
        QVERIFY(QRect(QPoint(),image.size()).contains(line.bounds));
        QCOMPARE(HoverPolicy::lineAt(lines,line.bounds.center(),35),QString("Hello world"));
    }
    QVERIFY(found);
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

void CoreTest::offlineDefaultAndPersistence()
{
    QTemporaryDir directory;
    SettingsStore store(directory.filePath("settings.ini"));
    HoverSettings settings;
    QCOMPARE(settings.provider, QString("offline"));
    settings.instance = "not a server";
    settings.pythonPath = "/custom/python";
    settings.packagesPath = "/custom/packages";
    settings.dictionaryPath = "/custom/cedict.u8";
    settings.useDictionary = false;
    QVERIFY(store.save(settings));
    const auto loaded = store.load();
    QCOMPARE(loaded.provider, settings.provider);
    QCOMPARE(loaded.pythonPath, settings.pythonPath);
    QCOMPARE(loaded.packagesPath, settings.packagesPath);
    QCOMPARE(loaded.dictionaryPath, settings.dictionaryPath);
    QVERIFY(!loaded.useDictionary);
    settings.provider = "mozhi";
    QVERIFY(!SettingsStore::validate(settings));
}

void CoreTest::offlineDictionaryBothDirectionsAndCancellation()
{
    QTemporaryDir directory;
    const QString path = directory.filePath("cedict.u8");
    QFile dictionary(path); QVERIFY(dictionary.open(QIODevice::WriteOnly));
    dictionary.write(QString("# fixture, created for this test\n你好 你好 [ni3 hao3] /hello/hi/\n世界 世界 [shi4 jie4] /world/\n").toUtf8()); dictionary.close();
    HoverSettings settings; settings.dictionaryPath = path; settings.pythonPath = "python3";
    MockServer server; settings.instance = server.url();
    TranslationService client;
    QSignalSpy success(&client, &TranslationService::translated);
    QSignalSpy failure(&client, &TranslationService::failed);
    client.translate(1, "Hello", "en", "zh-CN", settings);
    client.cancel();
    client.translate(2, "你 好", "zh-CN", "en", settings);
    QVERIFY(success.wait(5000));
    QCOMPARE(success.count(), 1);
    QCOMPARE(success.last().at(0).toULongLong(), 2ULL);
    QCOMPARE(success.last().at(1).toString(), QString("hello; hi"));
    client.translate(3, "Hello", "en", "zh-CN", settings);
    QVERIFY(success.wait(5000));
    QVERIFY(success.last().at(1).toString().contains("你好"));
    QCOMPARE(server.requests.size(), 0);
    QCOMPARE(failure.count(), 0);
}

void CoreTest::missingOfflinePythonFails()
{
    HoverSettings settings; settings.pythonPath = "/nonexistent/hover-python";
    TranslationService client;
    QSignalSpy failure(&client, &TranslationService::failed);
    QSignalSpy success(&client, &TranslationService::translated);
    client.translate(1, "Hello world", "en", "zh-CN", settings);
    QVERIFY(failure.wait(3000));
    QCOMPARE(success.count(), 0);
    QVERIFY(failure.last().at(1).toString().contains("offline", Qt::CaseInsensitive));
}

static HoverSettings delayedOfflineFixture(QTemporaryDir &directory)
{
    const QString path = directory.filePath("python-fixture");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write("#!/usr/bin/env python3\nimport json,sys,time\nfrom pathlib import Path\n"
        "marker=Path(sys.argv[sys.argv.index('--packages-dir')+1])/'received'\n"
        "print(json.dumps({'ready':True}),flush=True)\n"
        "for line in sys.stdin:\n"
        " r=json.loads(line)\n"
        " if r['text']=='Slow':\n"
        "  marker.write_text('received')\n"
        "  time.sleep(0.3)\n"
        " print(json.dumps({'id':r['id'],'translation':'result '+r['text']}),flush=True)\n");
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    HoverSettings settings; settings.pythonPath = path; settings.packagesPath = directory.path(); settings.useDictionary = false;
    return settings;
}

void CoreTest::offlineBusyRequestCancellation()
{
    QTemporaryDir directory;
    const auto settings = delayedOfflineFixture(directory);
    TranslationService client;
    QSignalSpy success(&client,&TranslationService::translated);
    QSignalSpy failure(&client,&TranslationService::failed);
    client.translate(1,"Slow","en","zh-CN",settings);
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(directory.filePath("received")),2000);
    client.translate(2,"Fast","en","zh-CN",settings);
    QVERIFY(success.wait(2000));
    QCOMPARE(success.count(),1);
    QCOMPARE(success.last().at(0).toULongLong(),2ULL);
    QCOMPARE(success.last().at(1).toString(),QString("result Fast"));
    QCOMPARE(failure.count(),0);
}

void CoreTest::offlineTimeoutAndRecovery()
{
    QTemporaryDir directory;
    const auto settings = delayedOfflineFixture(directory);
    TranslationService client; client.setOfflineTimeoutMs(80);
    QSignalSpy success(&client,&TranslationService::translated);
    QSignalSpy failure(&client,&TranslationService::failed);
    client.translate(1,"Slow","en","zh-CN",settings);
    QVERIFY(failure.wait(1500));
    QCOMPARE(success.count(),0);
    client.setOfflineTimeoutMs(2000);
    client.translate(2,"Fast","en","zh-CN",settings);
    QVERIFY(success.wait(3000));
    QCOMPARE(success.last().at(1).toString(),QString("result Fast"));
    QCOMPARE(failure.count(),1);
}

QTEST_MAIN(CoreTest)
#include "test_core.moc"
