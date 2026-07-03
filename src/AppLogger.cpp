#include "AppLogger.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <cstdlib>

namespace {
const qint64 kMaxLogFileSize = 2 * 1024 * 1024;
const int kMaxBackupCount = 3;

bool* installedFlag() {
    static bool installed = false;
    return &installed;
}

QMutex* loggerMutex() {
    static QMutex* mutex = new QMutex;
    return mutex;
}

QString levelName(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
        return QString::fromLatin1("DEBUG");
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    case QtInfoMsg:
        return QString::fromLatin1("INFO");
#endif
    case QtWarningMsg:
        return QString::fromLatin1("WARN");
    case QtCriticalMsg:
        return QString::fromLatin1("ERROR");
    case QtFatalMsg:
        return QString::fromLatin1("FATAL");
    }
    return QString::fromLatin1("LOG");
}

QString logDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    return QDir(base).filePath(QString::fromLatin1("logs"));
}

void rotateLogIfNeeded(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() || info.size() <= kMaxLogFileSize) {
        return;
    }

    QFile::remove(QString::fromLatin1("%1.%2").arg(path).arg(kMaxBackupCount));
    for (int index = kMaxBackupCount - 1; index >= 1; --index) {
        const QString source = QString::fromLatin1("%1.%2").arg(path).arg(index);
        const QString target = QString::fromLatin1("%1.%2").arg(path).arg(index + 1);
        if (QFileInfo(source).exists()) {
            QFile::remove(target);
            QFile::rename(source, target);
        }
    }
    QFile::rename(path, QString::fromLatin1("%1.1").arg(path));
}

void appendLine(const QString& level, const QString& message) {
    QMutexLocker locker(loggerMutex());

    const QString directory = logDirectory();
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(QString::fromLatin1("WStart.log"));
    rotateLogIfNeeded(path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QString::fromLatin1("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString line = QString::fromLatin1("%1 [%2] %3\n").arg(timestamp, level, message);
    file.write(line.toUtf8());
    file.flush();
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
void messageHandler(QtMsgType type, const char* message) {
    appendLine(levelName(type), QString::fromLocal8Bit(message ? message : ""));
    if (type == QtFatalMsg) {
        std::abort();
    }
}
#else
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    Q_UNUSED(context)
    appendLine(levelName(type), message);
    if (type == QtFatalMsg) {
        std::abort();
    }
}
#endif
} // namespace

void AppLogger::install() {
    if (*installedFlag()) {
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    qInstallMsgHandler(messageHandler);
#else
    qInstallMessageHandler(messageHandler);
#endif
    *installedFlag() = true;
    writeLine(QString::fromLatin1("INFO"), QString::fromLatin1("logger installed path=%1").arg(logFilePath()));
}

bool AppLogger::isInstalled() {
    return *installedFlag();
}

QString AppLogger::logFilePath() {
    return QDir(logDirectory()).filePath(QString::fromLatin1("WStart.log"));
}

void AppLogger::writeLine(const QString& level, const QString& message) {
    appendLine(level, message);
}