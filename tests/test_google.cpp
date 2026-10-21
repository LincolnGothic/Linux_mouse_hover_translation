// SPDX-FileCopyrightText: 2026 Hover Translate contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "googletranslation.h"
#include "translationservice.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
#include <cstring>

class GoogleReply : public QNetworkReply {
public:
    GoogleReply(const QNetworkRequest &request, QByteArray body, int status, int delay,
                NetworkError error, QObject *parent) : QNetworkReply(parent), m_data(std::move(body))
    {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(delay, this, [this, error] {
            if (isFinished()) return;
            if (error != NoError) setError(error, "private fixture details");
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {
        if (isFinished()) return;
        setFinished(true); setError(OperationCanceledError, "cancelled"); emit finished();
    }
    qint64 bytesAvailable() const override { return m_data.size() + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 maximum) override {
        const auto count = qMin(maximum, qint64(m_data.size()));
        if (!count) return -1;
        std::memcpy(buffer, m_data.constData(), size_t(count)); m_data.remove(0, count); return count;
    }
private:
    QByteArray m_data;
};

class GoogleNetwork : public QNetworkAccessManager {
public:
    QByteArray body = QString::fromUtf8(R"([[["你好","Hello",null,null]],null,"en"])").toUtf8();
    int status = 200, delay = 0;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QList<QNetworkRequest> requests;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoing) override {
        Q_ASSERT(operation == GetOperation && !outgoing);
        requests.append(request);
        return new GoogleReply(request, body, status, delay, error, this);
    }
};

class GoogleTest : public QObject {
    Q_OBJECT
private slots:
    void parsingAndEncoding();
    void longUnicodeChunks();
    void settingsRequestAndUsage();
    void malformedAndBlocked_data();
    void malformedAndBlocked();
    void cancellationTimeoutAndRecovery();
    void retryAndCancelBackoff();
    void sequentialChunks();
};

void GoogleTest::parsingAndEncoding()
{
    QString error;
    QCOMPARE(parseGoogleTranslation(QString::fromUtf8(R"([[["你好","Hello"],["世界","world"]],null,"en"])").toUtf8(), &error), QString("你好世界"));
    QVERIFY(error.isEmpty());
    for (const auto &body : {QByteArray(), QByteArray("[]"), QByteArray("null"), QByteArray("[[]]"),
         QByteArray("[[[]]]"), QByteArray("[[[null]]]"), QByteArray("[[[\" \"]]]"),
         QByteArray("[[[\"ok\"],[]]]"), QByteArray("<html>blocked private fixture</html>")}) {
        QVERIFY(parseGoogleTranslation(body, &error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(!error.contains("private fixture"));
    }
    QVERIFY(parseGoogleTranslation(QByteArray(2 * 1024 * 1024 + 1, 'x'), &error).isEmpty());
    const QString text = QString::fromUtf8("C++ & 50%\n你好 😀 # ? =");
    const auto url = googleTranslationUrl(text, "en", "zh-CN");
    QCOMPARE(url.host(), QString("translate.googleapis.com"));
    QCOMPARE(url.scheme(), QString("https"));
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue("q", QUrl::FullyDecoded), text);
    QVERIFY(url.toEncoded().contains("%2B%2B"));
    QCOMPARE(query.queryItemValue("client"), QString("gtx"));
    QCOMPARE(query.queryItemValue("sl"), QString("en"));
    QCOMPARE(query.queryItemValue("tl"), QString("zh-CN"));
}

void GoogleTest::longUnicodeChunks()
{
    const QString text = QString::fromUtf8("你好世界😀 line + &?%\n").repeated(170);
    const auto chunks = googleTranslationChunks(text, "zh-CN", "en");
    QVERIFY(chunks.size() > 2); QCOMPARE(chunks.join(""), text);
    for (const auto &chunk : chunks) {
        QVERIFY(googleTranslationUrl(chunk, "zh-CN", "en").toEncoded().size() <= 1800);
        QVERIFY(!chunk.front().isLowSurrogate()); QVERIFY(!chunk.back().isHighSurrogate());
    }
}

void GoogleTest::settingsRequestAndUsage()
{
    QTemporaryDir directory;
    SettingsStore store(directory.filePath("settings.ini"));
    HoverSettings settings; settings.provider = "google";
    QVERIFY(store.save(settings)); QCOMPARE(store.load().provider, QString("google"));
    CloudStore cloud(directory.filePath("cloud")); GoogleNetwork network;
    TranslationService client(nullptr, &network, cloud);
    QSignalSpy translated(&client, &TranslationService::translated), failed(&client, &TranslationService::failed);
    client.translate(7, "Hello + world", "en", "zh-CN", settings);
    QTRY_COMPARE(translated.count(), 1); QCOMPARE(failed.count(), 0);
    QCOMPARE(translated.first().at(0).toULongLong(), quint64(7));
    QCOMPARE(translated.first().at(1).toString(), QString("你好"));
    QCOMPARE(network.requests.size(), 1);
    const auto request = network.requests.first();
    QVERIFY(!request.hasRawHeader("Ocp-Apim-Subscription-Key"));
    QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
    QCOMPARE(cloud.usage().value("requests").toInteger(), qint64(1));
    QCOMPARE(cloud.usage().value("characters").toInteger(), qint64(13));
    auto other = settings; other.provider = "offline";
    QVERIFY(translationCacheKey(other, "Hello", "en") != translationCacheKey(settings, "Hello", "en"));
    client.translate(8, "Hello", "en", "en", settings);
    QCOMPARE(translated.count(), 2); QCOMPARE(network.requests.size(), 1);
    client.translate(9, "Hello", "ja", "en", settings);
    QCOMPARE(failed.count(), 1); QCOMPARE(network.requests.size(), 1);
}

void GoogleTest::malformedAndBlocked_data()
{
    QTest::addColumn<int>("status"); QTest::addColumn<QByteArray>("body"); QTest::addColumn<QString>("message");
    QTest::newRow("empty") << 200 << QByteArray("[]") << "invalid";
    QTest::newRow("html") << 200 << QByteArray("<html>private fixture details</html>") << "invalid";
    QTest::newRow("blocked") << 403 << QByteArray("private fixture details") << "blocked";
    QTest::newRow("rate-limit") << 429 << QByteArray("private fixture details") << "rate limit";
    QTest::newRow("redirect") << 302 << QByteArray() << "unavailable";
}

void GoogleTest::malformedAndBlocked()
{
    QFETCH(int, status); QFETCH(QByteArray, body); QFETCH(QString, message);
    GoogleNetwork network; network.status = status; network.body = body;
    GoogleTranslation client(&network); QSignalSpy failed(&client, &GoogleTranslation::failed);
    QSignalSpy translated(&client, &GoogleTranslation::translated);
    client.start("Hello", "en", "zh-CN", 1000);
    QTRY_COMPARE(failed.count(), 1); QCOMPARE(translated.count(), 0);
    QVERIFY(failed.first().first().toString().contains(message));
    QVERIFY(!failed.first().first().toString().contains("private fixture"));
    QCOMPARE(network.requests.size(), 1);
}

void GoogleTest::cancellationTimeoutAndRecovery()
{
    QTemporaryDir directory; GoogleNetwork network; network.delay = 150;
    TranslationService client(nullptr, &network, CloudStore(directory.filePath("cloud")));
    HoverSettings settings; settings.provider = "google";
    QSignalSpy translated(&client, &TranslationService::translated), failed(&client, &TranslationService::failed);
    client.translate(1, "Hello", "en", "zh-CN", settings); client.cancel();
    QTest::qWait(180); QCOMPARE(translated.count(), 0); QCOMPARE(failed.count(), 0);
    client.setTimeoutMs(20); client.translate(2, "Hello", "en", "zh-CN", settings);
    QTRY_COMPARE(failed.count(), 1); QCOMPARE(failed.first().at(0).toULongLong(), quint64(2));
    client.setTimeoutMs(1000); network.delay = 0;
    client.translate(3, "Hello", "en", "zh-CN", settings);
    QTRY_COMPARE(translated.count(), 1);
    QCOMPARE(translated.first().at(0).toULongLong(), quint64(3));
    QTest::qWait(180); QCOMPARE(translated.count(), 1); QCOMPARE(failed.count(), 1);
}

void GoogleTest::retryAndCancelBackoff()
{
    GoogleNetwork network; network.status = 503;
    GoogleTranslation client(&network);
    QSignalSpy translated(&client, &GoogleTranslation::translated), failed(&client, &GoogleTranslation::failed);
    client.start("Hello", "en", "zh-CN", 1000);
    QTest::qWait(50); QCOMPARE(network.requests.size(), 1);
    network.status = 200;
    QTRY_COMPARE(translated.count(), 1); QCOMPARE(network.requests.size(), 2);
    network.status = 503; client.start("Hello", "en", "zh-CN", 1000);
    QTest::qWait(50); client.cancel(); QTest::qWait(600);
    QCOMPARE(network.requests.size(), 3); QCOMPARE(failed.count(), 0);
    client.start("Hello", "en", "zh-CN", 1000);
    QTRY_COMPARE(failed.count(), 1); QCOMPARE(network.requests.size(), 5);
}

void GoogleTest::sequentialChunks()
{
    GoogleNetwork network;
    GoogleTranslation client(&network);
    QSignalSpy translated(&client, &GoogleTranslation::translated);
    QSignalSpy submitted(&client, &GoogleTranslation::requestSubmitted);
    const QString text = QString::fromUtf8("你好世界 ").repeated(300);
    const auto chunks = googleTranslationChunks(text, "zh-CN", "en");
    client.start(text, "zh-CN", "en", 1000);
    QTRY_COMPARE(translated.count(), 1); QCOMPARE(network.requests.size(), chunks.size());
    QString expected;
    for (const auto &chunk : chunks) expected += QString("你好") + (chunk.endsWith(' ') ? " " : "");
    QCOMPARE(translated.first().first().toString(), expected);
    qint64 count = 0; for (const auto &call : submitted) count += call.first().toLongLong();
    QCOMPARE(count, text.toUcs4().size());
}

QTEST_MAIN(GoogleTest)
#include "test_google.moc"
