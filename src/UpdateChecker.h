#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    QString currentVersion() const;
    QString manifestUrl() const;
    bool isChecking() const;

    void setCurrentVersion(const QString& version);
    void setManifestUrl(const QString& url);

public slots:
    void checkNow();
    void checkSilently();
    void checkNow(bool silent);

signals:
    void checkFinished(bool updateAvailable, QString latestVersion, QString downloadUrl, QString releaseNotes,
                       QString error, bool silent);

private slots:
    void onReplyFinished();

private:
    QString effectiveManifestUrl() const;
    QString inferredReleaseUrl(const QString& manifestUrl) const;

    QNetworkAccessManager m_network;
    QNetworkReply* m_reply = nullptr;
    QString m_currentVersion;
    QString m_manifestUrl;
    bool m_silent = false;
};
