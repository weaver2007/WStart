#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QString>
#include <QVector>

class QNetworkReply;
class QFile;

struct UpdateAsset {
    QString platform;
    QString arch;
    QString type;
    QString url;
    QString sha256;
    QString fileName;

    bool isValid() const;
};

struct UpdateInfo {
    bool updateAvailable = false;
    QString latestVersion;
    QString releaseNotes;
    QString pageUrl;
    UpdateAsset asset;
};

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    QString currentVersion() const;
    QString manifestUrl() const;
    bool isChecking() const;
    bool isDownloading() const;
    UpdateInfo lastUpdateInfo() const;

    void setCurrentVersion(const QString& version);
    void setManifestUrl(const QString& url);
    void setGithubToken(const QString& token);
    void setPreferPortable(bool portable);

    static QString currentPlatformKey();
    static QString currentArchKey();
    static bool versionGreaterThan(const QString& left, const QString& right);
    static UpdateInfo parseManifest(const QByteArray& payload, const QString& manifestUrl, const QString& currentVersion,
                                    bool preferPortable, QString* error = nullptr);
    static UpdateAsset selectAsset(const QVector<UpdateAsset>& assets, bool preferPortable);
    static bool verifySha256(const QString& filePath, const QString& expectedSha256, QString* error = nullptr);

public slots:
    void checkNow();
    void checkSilently();
    void checkNow(bool silent);
    void downloadSelectedUpdate();
    void cancelDownload();

signals:
    void checkFinished(bool updateAvailable, QString latestVersion, QString downloadUrl, QString releaseNotes,
                       QString error, bool silent);
    void updateInfoReady(UpdateInfo info, QString error, bool silent);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(QString filePath, UpdateAsset asset, QString error);

private slots:
    void onCheckReplyFinished();
    void onDownloadReadyRead();
    void onDownloadReplyFinished();

private:
    QString effectiveManifestUrl() const;
    QString inferredReleaseUrl(const QString& manifestUrl) const;
    QNetworkRequest makeRequest(const QUrl& url) const;
    QString downloadFilePath(const UpdateAsset& asset) const;

    QNetworkAccessManager m_network;
    QNetworkReply* m_reply = nullptr;
    QNetworkReply* m_downloadReply = nullptr;
    QFile* m_downloadFile = nullptr;
    QString m_currentVersion;
    QString m_manifestUrl;
    QString m_githubToken;
    UpdateInfo m_lastUpdateInfo;
    bool m_silent = false;
    bool m_downloadCancelled = false;
    bool m_preferPortable = false;
};
