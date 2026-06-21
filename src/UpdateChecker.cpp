#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

#if defined(Q_OS_WIN) && !defined(CALG_SHA_256)
#define CALG_SHA_256 (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_SHA_256)
#endif

QVector<int> parseVersionParts(const QString& version) {
    QVector<int> parts;
    const QString cleaned = version.trimmed().startsWith('v', Qt::CaseInsensitive) ? version.trimmed().mid(1)
                                                                                   : version.trimmed();
    const QStringList tokens = cleaned.split('.');
    for (const QString& token : tokens) {
        QString digits;
        for (int i = 0; i < token.size(); ++i) {
            if (!token.at(i).isDigit()) {
                break;
            }
            digits.append(token.at(i));
        }
        parts.push_back(digits.isEmpty() ? 0 : digits.toInt());
    }
    return parts;
}

QString inferredReleaseUrlFromManifest(const QString& manifestUrl) {
    QUrl url(manifestUrl);
    if (url.host().compare("raw.githubusercontent.com", Qt::CaseInsensitive) == 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QStringList pathParts = url.path().split('/', Qt::SkipEmptyParts);
#else
        const QStringList pathParts = url.path().split('/', QString::SkipEmptyParts);
#endif
        if (pathParts.size() >= 3) {
            return QString("https://github.com/%1/%2/releases/latest").arg(pathParts.at(0), pathParts.at(1));
        }
    }
    if (url.host().compare("api.github.com", Qt::CaseInsensitive) == 0 &&
        url.path().startsWith("/repos/", Qt::CaseInsensitive)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const QStringList pathParts = url.path().split('/', Qt::SkipEmptyParts);
#else
        const QStringList pathParts = url.path().split('/', QString::SkipEmptyParts);
#endif
        if (pathParts.size() >= 3) {
            return QString("https://github.com/%1/%2/releases/latest").arg(pathParts.at(1), pathParts.at(2));
        }
    }
    return QString();
}

QString normalizedPlatform(QString value) {
    value = value.trimmed().toLower();
    if (value == "win" || value == "windows") {
        return "windows";
    }
    if (value == "darwin" || value == "osx" || value == "mac" || value == "macos") {
        return "macos";
    }
    if (value == "linux") {
        return "linux";
    }
    return value;
}

QString normalizedArch(QString value) {
    value = value.trimmed().toLower();
    if (value == "amd64" || value == "x86_64" || value == "x64") {
        return "x64";
    }
    if (value == "aarch64" || value == "arm64") {
        return "arm64";
    }
    if (value == "i386" || value == "i686" || value == "x86") {
        return "x86";
    }
    return value;
}

QByteArray maybeDecodeGithubContents(const QByteArray& payload) {
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return payload;
    }
    const QJsonObject root = document.object();
    if (!root.contains("content") || !root.contains("encoding")) {
        return payload;
    }
    if (root.value("encoding").toString().compare("base64", Qt::CaseInsensitive) != 0) {
        return payload;
    }
    const QByteArray encoded = root.value("content").toString().remove('\n').remove('\r').toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(encoded);
    return decoded.isEmpty() ? payload : decoded;
}

QString fileNameFromUrl(const QString& urlText) {
    const QUrl url(urlText);
    const QString path = url.path();
    const QString name = QFileInfo(path).fileName();
    return name.isEmpty() ? QString::fromLatin1("WStart-update.bin") : name;
}

UpdateAsset assetFromJson(const QJsonObject& object) {
    UpdateAsset asset;
    asset.platform = normalizedPlatform(object.value("platform").toString());
    asset.arch = normalizedArch(object.value("arch").toString());
    asset.type = object.value("type").toString(object.value("packageType").toString()).trimmed().toLower();
    asset.url = object.value("apiUrl")
                    .toString(object.value("url").toString(object.value("downloadUrl").toString()))
                    .trimmed();
    asset.sha256 = object.value("sha256").toString().trimmed();
    asset.fileName = object.value("fileName").toString().trimmed();
    if (asset.fileName.isEmpty() && !asset.url.isEmpty()) {
        asset.fileName = fileNameFromUrl(asset.url);
    }
    return asset;
}

int assetTypeScore(const UpdateAsset& asset, bool preferPortable) {
    if (preferPortable) {
        if (asset.type == "portable") {
            return 300;
        }
        if (asset.type == "installer" || asset.type == "package") {
            return 100;
        }
    } else {
        if (asset.type == "installer" || asset.type == "package") {
            return 300;
        }
        if (asset.type == "portable") {
            return 100;
        }
    }
    return 0;
}

bool isGithubUrl(const QUrl& url) {
    return url.host().compare("api.github.com", Qt::CaseInsensitive) == 0 ||
           url.host().compare("github.com", Qt::CaseInsensitive) == 0 ||
           url.host().compare("objects.githubusercontent.com", Qt::CaseInsensitive) == 0 ||
           url.host().compare("raw.githubusercontent.com", Qt::CaseInsensitive) == 0;
}

QUrl redirectedUrl(QNetworkReply* reply) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    const QVariant value = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
#else
    const QVariant value = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
#endif
    if (!value.isValid()) {
        return QUrl();
    }
    const QUrl redirect = value.toUrl();
    return reply->url().resolved(redirect);
}

} // namespace

bool UpdateAsset::isValid() const {
    return !url.trimmed().isEmpty();
}

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    m_currentVersion = QString::fromLatin1(HKM_APP_VERSION);
#ifdef HKM_UPDATE_MANIFEST_URL
    m_manifestUrl = QString::fromLatin1(HKM_UPDATE_MANIFEST_URL);
#endif
}

QString UpdateChecker::currentVersion() const {
    return m_currentVersion;
}

QString UpdateChecker::manifestUrl() const {
    return m_manifestUrl;
}

bool UpdateChecker::isChecking() const {
    return m_reply != nullptr;
}

bool UpdateChecker::isDownloading() const {
    return m_downloadReply != nullptr;
}

UpdateInfo UpdateChecker::lastUpdateInfo() const {
    return m_lastUpdateInfo;
}

void UpdateChecker::setCurrentVersion(const QString& version) {
    m_currentVersion = version.trimmed();
}

void UpdateChecker::setManifestUrl(const QString& url) {
    m_manifestUrl = url.trimmed();
}

void UpdateChecker::setGithubToken(const QString& token) {
    m_githubToken = token.trimmed();
}

void UpdateChecker::setPreferPortable(bool portable) {
    m_preferPortable = portable;
}

QString UpdateChecker::currentPlatformKey() {
#ifdef Q_OS_WIN
    return "windows";
#elif defined(Q_OS_MAC)
    return "macos";
#elif defined(Q_OS_LINUX)
    return "linux";
#else
    return QString();
#endif
}

QString UpdateChecker::currentArchKey() {
#if defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_AMD64)
    return "x64";
#elif defined(Q_PROCESSOR_ARM_64)
    return "arm64";
#elif defined(Q_PROCESSOR_X86_32)
    return "x86";
#else
    return normalizedArch(QString::fromLatin1(QT_ARCH));
#endif
}

bool UpdateChecker::versionGreaterThan(const QString& left, const QString& right) {
    QVector<int> leftParts = parseVersionParts(left);
    QVector<int> rightParts = parseVersionParts(right);
    const int partCount = qMax(leftParts.size(), rightParts.size());
    leftParts.resize(partCount);
    rightParts.resize(partCount);
    for (int i = 0; i < partCount; ++i) {
        if (leftParts.at(i) != rightParts.at(i)) {
            return leftParts.at(i) > rightParts.at(i);
        }
    }
    return false;
}

UpdateInfo UpdateChecker::parseManifest(const QByteArray& payload, const QString& manifestUrl,
                                        const QString& currentVersion, bool preferPortable, QString* error) {
    UpdateInfo info;
    const QJsonDocument document = QJsonDocument::fromJson(maybeDecodeGithubContents(payload));
    if (!document.isObject()) {
        if (error) {
            *error = QString::fromUtf8("Invalid update manifest.");
        }
        return info;
    }

    const QJsonObject root = document.object();
    info.latestVersion = root.value("version").toString().trimmed();
    info.pageUrl = root.value("pageUrl").toString(inferredReleaseUrlFromManifest(manifestUrl)).trimmed();
    info.releaseNotes = root.value("releaseNotes").toString().trimmed();
    if (info.releaseNotes.isEmpty()) {
        info.releaseNotes = root.value("notes").toString().trimmed();
    }

    if (info.latestVersion.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("Update manifest does not contain a version.");
        }
        return info;
    }

    QVector<UpdateAsset> assets;
    const QJsonArray assetArray = root.value("assets").toArray();
    for (QJsonArray::const_iterator it = assetArray.constBegin(); it != assetArray.constEnd(); ++it) {
        const UpdateAsset asset = assetFromJson(it->toObject());
        if (asset.isValid()) {
            assets.push_back(asset);
        }
    }

    info.asset = selectAsset(assets, preferPortable);
    if (!info.asset.isValid()) {
        QString downloadUrl = root.value("downloadUrl").toString().trimmed();
        if (downloadUrl.isEmpty()) {
            downloadUrl = info.pageUrl;
        }
        if (!downloadUrl.isEmpty()) {
            info.asset.url = downloadUrl;
            info.asset.sha256 = root.value("sha256").toString().trimmed();
            info.asset.fileName = fileNameFromUrl(downloadUrl);
            info.asset.platform = currentPlatformKey();
            info.asset.arch = currentArchKey();
            info.asset.type = preferPortable ? QString::fromLatin1("portable") : QString::fromLatin1("installer");
        }
    }

    info.updateAvailable = versionGreaterThan(info.latestVersion, currentVersion);
    return info;
}

UpdateAsset UpdateChecker::selectAsset(const QVector<UpdateAsset>& assets, bool preferPortable) {
    UpdateAsset best;
    int bestScore = -1;
    const QString platform = currentPlatformKey();
    const QString arch = currentArchKey();
    for (const UpdateAsset& asset : assets) {
        int score = 0;
        const QString assetPlatform = normalizedPlatform(asset.platform);
        const QString assetArch = normalizedArch(asset.arch);
        if (!assetPlatform.isEmpty() && !platform.isEmpty() && assetPlatform != platform) {
            continue;
        }
        score += assetPlatform == platform ? 1000 : 10;
        if (!assetArch.isEmpty() && !arch.isEmpty() && assetArch != arch) {
            continue;
        }
        score += assetArch == arch ? 500 : 5;
        score += assetTypeScore(asset, preferPortable);
        if (score > bestScore) {
            best = asset;
            bestScore = score;
        }
    }
    return best;
}

bool UpdateChecker::verifySha256(const QString& filePath, const QString& expectedSha256, QString* error) {
    const QString expected = expectedSha256.trimmed().toLower();
    if (expected.isEmpty()) {
        return true;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(1024 * 1024));
    }
    const QString actual = QString::fromLatin1(hash.result().toHex());
#elif defined(Q_OS_WIN)
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    QByteArray digest;
    if (CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        while (!file.atEnd()) {
            const QByteArray block = file.read(1024 * 1024);
            if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(block.constData()), static_cast<DWORD>(block.size()),
                               0)) {
                break;
            }
        }
        DWORD hashSize = 0;
        DWORD hashSizeLength = sizeof(hashSize);
        if (CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hashSize), &hashSizeLength, 0) &&
            hashSize > 0) {
            digest.resize(static_cast<int>(hashSize));
            DWORD digestLength = hashSize;
            if (!CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(digest.data()), &digestLength, 0)) {
                digest.clear();
            }
        }
    }
    if (hash) {
        CryptDestroyHash(hash);
    }
    if (provider) {
        CryptReleaseContext(provider, 0);
    }
    const QString actual = QString::fromLatin1(digest.toHex());
    if (actual.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("SHA256 is not available.");
        }
        return false;
    }
#else
    if (error) {
        *error = QString::fromUtf8("SHA256 is not available on this Qt version.");
    }
    return false;
#endif
    if (actual != expected) {
        if (error) {
            *error = QString::fromUtf8("SHA256 mismatch.");
        }
        return false;
    }
    return true;
}

void UpdateChecker::checkNow() {
    checkNow(false);
}

void UpdateChecker::checkSilently() {
    checkNow(true);
}

void UpdateChecker::checkNow(bool silent) {
    if (m_reply) {
        return;
    }

    const QString urlText = effectiveManifestUrl();
    const QUrl url(urlText);
    if (!url.isValid() || url.scheme().isEmpty()) {
        const QString error = QString::fromUtf8("Update manifest URL is empty.");
        emit checkFinished(false, QString(), QString(), QString(), error, silent);
        emit updateInfoReady(UpdateInfo(), error, silent);
        return;
    }

    m_silent = silent;
    m_reply = m_network.get(makeRequest(url));
    connect(m_reply, SIGNAL(finished()), this, SLOT(onCheckReplyFinished()));
}

void UpdateChecker::downloadSelectedUpdate() {
    if (m_downloadReply || !m_lastUpdateInfo.asset.isValid()) {
        return;
    }
    const QUrl url(m_lastUpdateInfo.asset.url);
    if (!url.isValid() || url.scheme().isEmpty()) {
        emit downloadFinished(QString(), m_lastUpdateInfo.asset, QString::fromUtf8("Update download URL is invalid."));
        return;
    }

    const QString filePath = downloadFilePath(m_lastUpdateInfo.asset);
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    delete m_downloadFile;
    m_downloadFile = new QFile(filePath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = m_downloadFile->errorString();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        emit downloadFinished(QString(), m_lastUpdateInfo.asset, error);
        return;
    }

    m_downloadCancelled = false;
    m_downloadReply = m_network.get(makeRequest(url));
    connect(m_downloadReply, SIGNAL(readyRead()), this, SLOT(onDownloadReadyRead()));
    connect(m_downloadReply, SIGNAL(downloadProgress(qint64, qint64)), this, SIGNAL(downloadProgress(qint64, qint64)));
    connect(m_downloadReply, SIGNAL(finished()), this, SLOT(onDownloadReplyFinished()));
}

void UpdateChecker::cancelDownload() {
    m_downloadCancelled = true;
    if (m_downloadReply) {
        m_downloadReply->abort();
    }
}

void UpdateChecker::onCheckReplyFinished() {
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }

    const bool silent = m_silent;
    const QString manifestUrlText = reply->url().toString();
    QString error;
    QByteArray payload;
    if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
    } else {
        payload = reply->readAll();
    }
    reply->deleteLater();

    if (!error.isEmpty()) {
        emit checkFinished(false, QString(), QString(), QString(), error, silent);
        emit updateInfoReady(UpdateInfo(), error, silent);
        return;
    }

    m_lastUpdateInfo = parseManifest(payload, manifestUrlText, m_currentVersion, m_preferPortable, &error);
    if (!error.isEmpty()) {
        emit checkFinished(false, QString(), QString(), QString(), error, silent);
        emit updateInfoReady(UpdateInfo(), error, silent);
        return;
    }

    emit checkFinished(m_lastUpdateInfo.updateAvailable, m_lastUpdateInfo.latestVersion, m_lastUpdateInfo.asset.url,
                       m_lastUpdateInfo.releaseNotes, QString(), silent);
    emit updateInfoReady(m_lastUpdateInfo, QString(), silent);
}

void UpdateChecker::onDownloadReadyRead() {
    if (m_downloadReply && m_downloadFile) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void UpdateChecker::onDownloadReplyFinished() {
    QNetworkReply* reply = m_downloadReply;
    QFile* file = m_downloadFile;
    m_downloadReply = nullptr;
    m_downloadFile = nullptr;
    if (!reply) {
        return;
    }
    const QUrl redirect = reply ? redirectedUrl(reply) : QUrl();
    if (redirect.isValid() && !m_downloadCancelled) {
        if (file) {
            file->close();
            file->remove();
            delete file;
        }
        reply->deleteLater();
        m_downloadReply = m_network.get(makeRequest(redirect));
        m_downloadFile = new QFile(downloadFilePath(m_lastUpdateInfo.asset), this);
        if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QString error = m_downloadFile->errorString();
            delete m_downloadFile;
            m_downloadFile = nullptr;
            emit downloadFinished(QString(), m_lastUpdateInfo.asset, error);
            return;
        }
        connect(m_downloadReply, SIGNAL(readyRead()), this, SLOT(onDownloadReadyRead()));
        connect(m_downloadReply, SIGNAL(downloadProgress(qint64, qint64)), this,
                SIGNAL(downloadProgress(qint64, qint64)));
        connect(m_downloadReply, SIGNAL(finished()), this, SLOT(onDownloadReplyFinished()));
        return;
    }

    if (file) {
        file->write(reply->readAll());
        file->close();
    }

    QString error;
    const QString filePath = file ? file->fileName() : QString();
    if (m_downloadCancelled) {
        error = QString::fromUtf8("Download cancelled.");
    } else if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
    }
    reply->deleteLater();
    delete file;

    if (!error.isEmpty()) {
        QFile::remove(filePath);
        emit downloadFinished(QString(), m_lastUpdateInfo.asset, error);
        return;
    }

    if (!verifySha256(filePath, m_lastUpdateInfo.asset.sha256, &error)) {
        QFile::remove(filePath);
        emit downloadFinished(QString(), m_lastUpdateInfo.asset, error);
        return;
    }

    emit downloadFinished(filePath, m_lastUpdateInfo.asset, QString());
}

QString UpdateChecker::effectiveManifestUrl() const {
    return m_manifestUrl.trimmed();
}

QString UpdateChecker::inferredReleaseUrl(const QString& manifestUrl) const {
    return inferredReleaseUrlFromManifest(manifestUrl);
}

QNetworkRequest UpdateChecker::makeRequest(const QUrl& url) const {
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    request.setHeader(QNetworkRequest::UserAgentHeader, QString("WStart/%1").arg(m_currentVersion));
#endif
    request.setRawHeader("Accept", "application/vnd.github+json, application/octet-stream, application/json");
    if (!m_githubToken.isEmpty() && isGithubUrl(url)) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_githubToken.toUtf8());
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    }
    return request;
}

QString UpdateChecker::downloadFilePath(const UpdateAsset& asset) const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    const QString fileName = asset.fileName.isEmpty() ? fileNameFromUrl(asset.url) : asset.fileName;
    return QDir(base).filePath(QString("updates/%1/%2").arg(m_lastUpdateInfo.latestVersion, fileName));
}
