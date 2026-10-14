// SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cloudtranslation.h"
#include "translationservice.h"
#include "settings.h"
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>
#include <cstring>
#ifdef Q_OS_WIN
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincred.h>

namespace {
QString credentialTarget(const QString &directory)
{
    const auto path = QDir::cleanPath(QFileInfo(directory).absoluteFilePath()).toCaseFolded();
    return "HoverTranslate/Azure/" + QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
}
class TestCloudDirectory : public QTemporaryDir {
public:
    ~TestCloudDirectory() {
        if (!isValid()) return;
        // Only this unique fixture's target; never a user's saved credential.
        const auto target = credentialTarget(filePath("cloud")).toStdWString();
        CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
    }
};
}
#else
using TestCloudDirectory = QTemporaryDir;
#endif

class MockReply : public QNetworkReply {
public:
    MockReply(const QNetworkRequest &request, QByteArray body, int code, int delay,
              NetworkError error, QObject *parent) : QNetworkReply(parent), data(std::move(body))
    {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, code);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(delay, this, [this, error] {
            if (done) return;
            done = true;
            if (error != NoError) setError(error, "fixture-secret");
            setFinished(true); emit readyRead(); emit finished();
        });
    }
    void abort() override {
        if (done) return;
        done = true; setError(OperationCanceledError, "cancelled");
        setFinished(true); emit finished();
    }
    qint64 bytesAvailable() const override { return data.size() + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 maximum) override {
        const auto count = qMin(maximum, qint64(data.size()));
        if (!count) return -1;
        std::memcpy(buffer, data.constData(), size_t(count)); data.remove(0, count); return count;
    }
private:
    QByteArray data;
    bool done = false;
};

class MockNetwork : public QNetworkAccessManager {
public:
    QByteArray body = R"([{"translations":[{"text":"你好","to":"zh-Hans"}]}])";
    int code = 200, delay = 0, requests = 0;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QNetworkRequest sent;
    QByteArray payload;
protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request,
                                 QIODevice *outgoing) override {
        Q_ASSERT(operation == PostOperation);
        ++requests; sent = request; payload = outgoing->readAll();
        return new MockReply(request, body, code, delay, error, this);
    }
};

class AzureTest : public QObject {
    Q_OBJECT
private slots:
    void settingsMigrationAndSecrets();
    void credentialsRoundtripAndIsolation();
    void invalidCredentials();
    void languageMappingAndParsing();
    void errors_data();
    void errors();
    void missingCredentialsAndPrivatePermissions();
    void requestAndUsage();
    void cancellationTimeoutAndStaleReply();
    void usageRollover();
    void providerCacheSeparation();
};

void AzureTest::settingsMigrationAndSecrets()
{
    TestCloudDirectory directory; SettingsStore settings(directory.filePath("settings.ini"));
    QCOMPARE(settings.load().provider, QString("offline"));
    { QSettings legacy(settings.fileName(), QSettings::IniFormat);
      legacy.setValue("translation/provider", "mozhi"); }
    QCOMPARE(settings.load().provider, QString("mozhi"));
    auto value = settings.load(); value.provider = "azure";
    QVERIFY(settings.save(value)); QCOMPARE(settings.load().provider, QString("azure"));
    CloudStore store(directory.filePath("cloud")); QString error;
    QVERIFY2(store.saveCredentials("fixture-secret", "westus", &error), qPrintable(error));
    QFile ini(settings.fileName()); QVERIFY(ini.open(QIODevice::ReadOnly));
    QVERIFY(!ini.readAll().contains("fixture-secret"));
    value.provider = "invalid"; QVERIFY(!settings.save(value));
}

void AzureTest::credentialsRoundtripAndIsolation()
{
    TestCloudDirectory directory, otherDirectory;
    const auto cloudPath = directory.filePath("cloud");
    CloudStore store(cloudPath), other(otherDirectory.filePath("cloud"));
    QString error;
    QVERIFY(store.credentials(&error).isEmpty()); QVERIFY(error.isEmpty());
    QVERIFY2(store.saveCredentials(" fixture-secret ", "westus", &error), qPrintable(error));
    const auto loaded = CloudStore(cloudPath).credentials(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loaded.value("key").toString(), QString("fixture-secret"));
    QCOMPARE(loaded.value("region").toString(), QString("westus"));
    QVERIFY(other.credentials(&error).isEmpty()); QVERIFY(error.isEmpty());
    QVERIFY(other.saveCredentials("other-fixture", "eastus"));
    QCOMPARE(store.credentials().value("key").toString(), QString("fixture-secret"));
    QCOMPARE(other.credentials().value("key").toString(), QString("other-fixture"));
#ifdef Q_OS_WIN
    QVERIFY(!QFile::exists(directory.filePath("cloud/azure-credentials.json")));
    QCOMPARE(CloudStore(cloudPath + "/.").credentials(), loaded);
    const auto target = credentialTarget(cloudPath).toStdWString();
    PCREDENTIALW credential = nullptr;
    QVERIFY(CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential));
    const auto persist = credential->Persist;
    const auto blob = QByteArray(reinterpret_cast<const char *>(credential->CredentialBlob),
                                 int(credential->CredentialBlobSize));
    CredFree(credential);
    QCOMPARE(persist, DWORD(CRED_PERSIST_LOCAL_MACHINE));
    QCOMPARE(QJsonDocument::fromJson(blob).object(), loaded);
#endif
}

void AzureTest::invalidCredentials()
{
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QString error;
    for (const auto &key : {QString(), QString(" \t "), QString("key\r\nInjected: value"), QString(2049, 'x')}) {
        QVERIFY(!store.saveCredentials(key, "westus", &error)); QVERIFY(!error.isEmpty());
        QVERIFY(store.credentials().isEmpty());
    }
    for (const auto &region : {QString("West US"), QString("westus\r\n"), QString("westus\n"), QString("west_us")}) {
        QVERIFY(!store.saveCredentials("fixture-secret", region, &error)); QVERIFY(!error.isEmpty());
        QVERIFY(store.credentials().isEmpty());
    }
#ifdef Q_OS_WIN
    QVERIFY2(store.saveCredentials(QString(2048, 'x'), "westus", &error), qPrintable(error));
    QCOMPARE(store.credentials().value("key").toString(), QString(2048, 'x'));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
    const auto target = credentialTarget(directory.filePath("cloud")).toStdWString();
    PCREDENTIALW credential = nullptr;
    QVERIFY(CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential));
    const auto invalid = QByteArray(R"({"key":"","region":"westus"})");
    credential->CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(invalid.constData()));
    credential->CredentialBlobSize = DWORD(invalid.size());
    const bool written = CredWriteW(credential, 0);
    CredFree(credential);
    QVERIFY(written);
    QVERIFY(store.credentials(&error).isEmpty()); QVERIFY(error.contains("Invalid"));
#endif
}

void AzureTest::languageMappingAndParsing()
{
    QCOMPARE(azureLanguage("en"), QString("en"));
    QCOMPARE(azureLanguage("zh-CN"), QString("zh-Hans"));
    QVERIFY(azureLanguage("ja").isEmpty());
    QString error;
    const auto body = QByteArray(R"([{"translations":[{"text":"你好","to":"zh-Hans"}]}])");
    QCOMPARE(parseAzureResponse(body, "zh-Hans", &error), QString("你好"));
    QVERIFY(error.isEmpty());
    for (const auto &invalid : {QByteArray("not json"), QByteArray("{}"),
                               QByteArray(R"([{"translations":[{"text":123}]}])"),
                               QByteArray(R"([{"translations":[{"text":"OK","to":"en"}]}])")}) {
        QVERIFY(parseAzureResponse(invalid, "zh-Hans", &error).isEmpty());
        QVERIFY(error.contains("invalid response"));
    }
    for (const auto &empty : {QByteArray("[]"), QByteArray(R"([{"translations":[]}])"),
                             QByteArray(R"([{"translations":[{"text":" "}]}])")}) {
        QVERIFY(parseAzureResponse(empty, "zh-Hans", &error).isEmpty());
        QVERIFY(error.contains("empty translation"));
    }
}

void AzureTest::errors_data()
{
    QTest::addColumn<int>("status"); QTest::addColumn<int>("networkError");
    QTest::addColumn<QString>("message");
    for (auto item : QList<QPair<int, QString>>{{400,"language"},{401,"credentials"},
             {403,"quota"},{408,"timed out"},{429,"rate limit"},{500,"unavailable"},{302,"redirect"}})
        QTest::newRow(qPrintable(QString::number(item.first))) << item.first << 0 << item.second;
    QTest::newRow("network") << 0 << int(QNetworkReply::ConnectionRefusedError) << QString("network");
}

void AzureTest::errors()
{
    QFETCH(int, status); QFETCH(int, networkError); QFETCH(QString, message);
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
    MockNetwork network; network.code = status;
    network.error = QNetworkReply::NetworkError(networkError);
    TranslationService service(nullptr, &network, store);
    QSignalSpy failed(&service, &TranslationService::failed);
    HoverSettings settings; settings.provider = "azure";
    service.translate(42, "Hello", "en", "zh-CN", settings);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(failed[0][0].toULongLong(), quint64(42));
    QVERIFY2(failed[0][1].toString().contains(message), qPrintable(failed[0][1].toString()));
    QVERIFY(!failed[0][1].toString().contains("fixture-secret"));
    QCOMPARE(network.requests, 1);
}

void AzureTest::missingCredentialsAndPrivatePermissions()
{
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    MockNetwork network; TranslationService service(nullptr, &network, store);
    QSignalSpy failed(&service, &TranslationService::failed);
    HoverSettings settings; settings.provider = "azure";
    service.translate(1, "Hello", "en", "zh-CN", settings);
    QCOMPARE(failed.count(), 1); QCOMPARE(network.requests, 0);
    QVERIFY(failed[0][1].toString().contains("credentials"));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
#ifndef Q_OS_WIN
    const auto path = directory.filePath("cloud/azure-credentials.json");
    const auto permissions = QFile::permissions(path);
    QVERIFY(!(permissions & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther)));
    QVERIFY(QFile::setPermissions(path, permissions | QFile::ReadOther));
    QString error; QVERIFY(store.credentials(&error).isEmpty()); QVERIFY(error.contains("permissions"));
    service.translate(2, "Hello", "en", "zh-CN", settings);
    QCOMPARE(network.requests, 0);
#endif
}

void AzureTest::requestAndUsage()
{
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
    MockNetwork network; TranslationService service(nullptr, &network, store);
    QSignalSpy translated(&service, &TranslationService::translated);
    HoverSettings settings; settings.provider = "azure"; settings.useDictionary = true;
    service.translate(9, "Hello😀", "en", "zh-CN", settings);
    QTRY_COMPARE(translated.count(), 1);
    QCOMPARE(translated[0][1].toString(), QString("你好"));
    QCOMPARE(QUrlQuery(network.sent.url()).queryItemValue("to"), QString("zh-Hans"));
    QCOMPARE(network.sent.url().scheme(), QString("https"));
    QCOMPARE(network.sent.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             int(QNetworkRequest::ManualRedirectPolicy));
    QCOMPARE(network.sent.rawHeader("Ocp-Apim-Subscription-Key"), QByteArray("fixture-secret"));
    QCOMPARE(QJsonDocument::fromJson(network.payload).array()[0].toObject().value("Text").toString(), QString("Hello😀"));
    QCOMPARE(store.usage().value("requests").toInt(), 1);
    QCOMPARE(store.usage().value("characters").toInt(), 6);
    QFile usage(directory.filePath("cloud/cloud-usage.json")); QVERIFY(usage.open(QIODevice::ReadOnly));
    const auto raw = usage.readAll();
    for (const auto &forbidden : {"Hello", "fixture-secret", "westus", "translations"}) QVERIFY(!raw.contains(forbidden));
    QSignalSpy failed(&service, &TranslationService::failed);
    service.translate(10, "Hello", "ja", "zh-CN", settings);
    QCOMPARE(failed.count(), 1); QCOMPARE(network.requests, 1);
    service.translate(11, "Hello", "en", "en", settings);
    QCOMPARE(translated.count(), 2); QCOMPARE(network.requests, 1);
}

void AzureTest::cancellationTimeoutAndStaleReply()
{
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
    MockNetwork network; network.delay = 100;
    TranslationService service(nullptr, &network, store); service.setTimeoutMs(20);
    QSignalSpy failed(&service, &TranslationService::failed);
    QSignalSpy translated(&service, &TranslationService::translated);
    HoverSettings settings; settings.provider = "azure";
    service.translate(1, "Hello", "en", "zh-CN", settings);
    QTRY_COMPARE(failed.count(), 1); QCOMPARE(failed[0][0].toULongLong(), quint64(1));
    service.translate(2, "Old", "en", "zh-CN", settings); service.cancel();
    network.delay = 0;
    service.translate(3, "New", "en", "zh-CN", settings);
    QTRY_COMPARE(translated.count(), 1);
    QCOMPARE(translated[0][0].toULongLong(), quint64(3));
    QTest::qWait(120); QCOMPARE(translated.count(), 1); QCOMPARE(failed.count(), 1);
    network.delay = 100;
    service.translate(4, "Old", "en", "zh-CN", settings);
    network.delay = 0; service.translate(5, "New", "en", "zh-CN", settings);
    QTRY_COMPARE(translated.count(), 2); QCOMPARE(translated[1][0].toULongLong(), quint64(5));
    QTest::qWait(120); QCOMPARE(translated.count(), 2);
}

void AzureTest::usageRollover()
{
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QVERIFY(store.record(7, nullptr, QDate(2026, 10, 31)));
    QVERIFY(store.record(3, nullptr, QDate(2026, 10, 31)));
    QCOMPARE(store.usage(QDate(2026, 10, 31)).value("requests").toInt(), 2);
    QCOMPARE(store.usage(QDate(2026, 10, 31)).value("characters").toInt(), 10);
    QCOMPARE(store.usage(QDate(2026, 11, 1)).value("requests").toInt(), 0);
    QVERIFY(store.record(5, nullptr, QDate(2026, 11, 1)));
    QCOMPARE(store.usage(QDate(2026, 11, 1)).value("characters").toInt(), 5);
}

void AzureTest::providerCacheSeparation()
{
    HoverSettings settings;
    const auto offline = translationCacheKey(settings, "Hello", "en");
    settings.provider = "azure";
    const auto azure = translationCacheKey(settings, "Hello", "en");
    QVERIFY(azure != offline);
    settings.provider = "mozhi";
    const auto mozhi = translationCacheKey(settings, "Hello", "en");
    QVERIFY(mozhi != azure); QVERIFY(mozhi != offline);
    TestCloudDirectory directory; CloudStore store(directory.filePath("cloud"));
    QVERIFY(store.saveCredentials("fixture-secret", "westus"));
    settings.provider = "azure";
    QCOMPARE(translationCacheKey(settings, "Hello", "en"), azure);
    QVERIFY(!azure.contains("fixture-secret"));
}

QTEST_GUILESS_MAIN(AzureTest)
#include "test_azure.moc"
