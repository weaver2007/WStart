#include "SelfUpdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

QString portableMarkerPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1("WStart.portable"));
}

#ifdef Q_OS_WIN
QString updaterPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1("WStartUpdater.exe"));
}
#endif

} // namespace

bool SelfUpdater::isPortableMode() {
    return QFileInfo::exists(portableMarkerPath());
}

bool SelfUpdater::startPortableUpdate(const QString& packagePath, QString* error) {
#ifdef Q_OS_WIN
    const QString helper = updaterPath();
    if (!QFileInfo::exists(helper)) {
        if (error) {
            *error = QString::fromUtf8("WStartUpdater.exe was not found.");
        }
        return false;
    }

    const QString tempDir =
        QDir::temp().filePath(QString("WStartUpdater-%1").arg(QUuid::createUuid().toString().remove('{').remove('}')));
    if (!QDir().mkpath(tempDir)) {
        if (error) {
            *error = QString::fromUtf8("Unable to create updater temporary directory.");
        }
        return false;
    }
    const QString tempHelper = QDir(tempDir).filePath(QString::fromLatin1("WStartUpdater.exe"));
    if (!QFile::copy(helper, tempHelper)) {
        if (error) {
            *error = QString::fromUtf8("Unable to copy WStartUpdater.exe to a temporary directory.");
        }
        return false;
    }

    const QString targetDir = QCoreApplication::applicationDirPath();
    const QString restartPath = QCoreApplication::applicationFilePath();
    QStringList args;
    args << QString::fromLatin1("--pid") << QString::number(GetCurrentProcessId()) << QString::fromLatin1("--package")
         << packagePath << QString::fromLatin1("--target-dir") << targetDir << QString::fromLatin1("--restart")
         << restartPath;

    if (!QProcess::startDetached(tempHelper, args, tempDir)) {
        if (error) {
            *error = QString::fromUtf8("Unable to start WStartUpdater.exe.");
        }
        return false;
    }
    QCoreApplication::quit();
    return true;
#else
    Q_UNUSED(packagePath);
    if (error) {
        *error = QString::fromUtf8("Portable self-update is not implemented on this platform yet.");
    }
    return false;
#endif
}
