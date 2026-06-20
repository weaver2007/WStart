#include "UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace {

QVector<int> parseVersionParts(const QString& version) {
    QVector<int> parts;
    const QStringList tokens = version.split('.');
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

bool versionGreaterThan(const QString& left, const QString& right) {
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

} // namespace

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

void UpdateChecker::setCurrentVersion(const QString& version) {
    m_currentVersion = version.trimmed();
}

void UpdateChecker::setManifestUrl(const QString& url) {
    m_manifestUrl = url.trimmed();
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
        emit checkFinished(false, QString(), QString(), QString(), QString::fromUtf8("Update manifest URL is empty."),
                           silent);
        return;
    }

    m_silent = silent;
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    request.setHeader(QNetworkRequest::UserAgentHeader, QString("HStart/%1").arg(m_currentVersion));
#endif
    m_reply = m_network.get(request);
    connect(m_reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
}

void UpdateChecker::onReplyFinished() {
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
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        emit checkFinished(false, QString(), QString(), QString(), QString::fromUtf8("Invalid update manifest."),
                           silent);
        return;
    }

    const QJsonObject root = document.object();
    const QString latestVersion = root.value("version").toString().trimmed();
    QString downloadUrl = root.value("downloadUrl").toString().trimmed();
    if (downloadUrl.isEmpty()) {
        downloadUrl = root.value("pageUrl").toString(inferredReleaseUrl(manifestUrlText)).trimmed();
    }
    QString releaseNotes = root.value("releaseNotes").toString().trimmed();
    if (releaseNotes.isEmpty()) {
        releaseNotes = root.value("notes").toString().trimmed();
    }

    if (latestVersion.isEmpty()) {
        emit checkFinished(false, QString(), QString(), QString(),
                           QString::fromUtf8("Update manifest does not contain a version."), silent);
        return;
    }

    emit checkFinished(versionGreaterThan(latestVersion, m_currentVersion), latestVersion, downloadUrl, releaseNotes,
                       QString(), silent);
}

QString UpdateChecker::effectiveManifestUrl() const {
    return m_manifestUrl.trimmed();
}

QString UpdateChecker::inferredReleaseUrl(const QString& manifestUrl) const {
    QUrl url(manifestUrl);
    if (url.host().compare("raw.githubusercontent.com", Qt::CaseInsensitive) != 0) {
        return QString();
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QStringList pathParts = url.path().split('/', Qt::SkipEmptyParts);
#else
    const QStringList pathParts = url.path().split('/', QString::SkipEmptyParts);
#endif
    if (pathParts.size() < 3) {
        return QString();
    }
    return QString("https://github.com/%1/%2/releases/latest").arg(pathParts.at(0), pathParts.at(1));
}
