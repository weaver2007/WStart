#pragma once

#include <QString>

class CredentialStore {
public:
    static QString githubToken();
    static bool saveGithubToken(const QString& token, QString* error = nullptr);
    static bool clearGithubToken(QString* error = nullptr);
};
