#include "CredentialStore.h"

#include <QSettings>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace {
const char* GithubTokenKey = "githubToken";
const char* GithubCredentialName = "WStart/GitHubToken";

#ifdef Q_OS_WIN
QString windowsErrorMessage(DWORD errorCode) {
    return QString("Windows credential error %1.").arg(errorCode);
}
#endif

} // namespace

QString CredentialStore::githubToken() {
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    const std::wstring targetName = QString::fromLatin1(GithubCredentialName).toStdWString();
    if (!CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &credential) || !credential) {
        return QString();
    }

    QString token;
    if (credential->CredentialBlob && credential->CredentialBlobSize > 0) {
        token = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob),
                                  static_cast<int>(credential->CredentialBlobSize));
    }
    CredFree(credential);
    return token;
#else
    QSettings settings;
    return settings.value(QString::fromLatin1(GithubTokenKey)).toString();
#endif
}

bool CredentialStore::saveGithubToken(const QString& token, QString* error) {
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        return clearGithubToken(error);
    }

#ifdef Q_OS_WIN
    const QByteArray blob = trimmed.toUtf8();
    const std::wstring targetName = QString::fromLatin1(GithubCredentialName).toStdWString();
    const std::wstring comment = QString::fromLatin1("WStart private GitHub update token").toStdWString();

    CREDENTIALW credential;
    ZeroMemory(&credential, sizeof(credential));
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(targetName.c_str());
    credential.Comment = const_cast<LPWSTR>(comment.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(blob.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"WStart");

    if (!CredWriteW(&credential, 0)) {
        if (error) {
            *error = windowsErrorMessage(GetLastError());
        }
        return false;
    }
    return true;
#else
    QSettings settings;
    settings.setValue(QString::fromLatin1(GithubTokenKey), trimmed);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QString::fromLatin1("Unable to save token.");
        }
        return false;
    }
    return true;
#endif
}

bool CredentialStore::clearGithubToken(QString* error) {
#ifdef Q_OS_WIN
    const std::wstring targetName = QString::fromLatin1(GithubCredentialName).toStdWString();
    if (!CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD code = GetLastError();
        if (code == ERROR_NOT_FOUND) {
            return true;
        }
        if (error) {
            *error = windowsErrorMessage(code);
        }
        return false;
    }
    return true;
#else
    QSettings settings;
    settings.remove(QString::fromLatin1(GithubTokenKey));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QString::fromLatin1("Unable to clear token.");
        }
        return false;
    }
    return true;
#endif
}
