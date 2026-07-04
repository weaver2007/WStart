#include "AppLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QStandardPaths>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <QtGlobal>

#include <cstdlib>

namespace {
const qint64 kMaxLogFileSize = 2 * 1024 * 1024;
const int kMaxBackupCount = 3;
const int kMaxQueuedLines = 4096;

bool* installedFlag() {
    static bool installed = false;
    return &installed;
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

QString currentLogFilePath() {
    return QDir(logDirectory()).filePath(QString::fromLatin1("WStart.log"));
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

void appendLines(const QStringList& lines) {
    if (lines.isEmpty()) {
        return;
    }

    const QString directory = logDirectory();
    QDir().mkpath(directory);
    const QString path = currentLogFilePath();
    rotateLogIfNeeded(path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    for (const QString& line : lines) {
        file.write(line.toUtf8());
    }
    file.flush();
}

class LogWriterThread : public QThread {
public:
    void enqueue(const QString& line) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_queue.size() >= kMaxQueuedLines) {
                m_queue.dequeue();
            }
            m_queue.enqueue(line);
        }
        m_wait.wakeOne();
        if (!isRunning()) {
            start();
        }
    }

protected:
    void run() override {
        for (;;) {
            QStringList batch;
            {
                QMutexLocker locker(&m_mutex);
                if (m_queue.isEmpty()) {
                    m_wait.wait(&m_mutex, 1000);
                }
                while (!m_queue.isEmpty()) {
                    batch.append(m_queue.dequeue());
                    if (batch.size() >= 256) {
                        break;
                    }
                }
            }
            appendLines(batch);
        }
    }

private:
    QMutex m_mutex;
    QWaitCondition m_wait;
    QQueue<QString> m_queue;
};

LogWriterThread* writerThread() {
    static LogWriterThread* thread = new LogWriterThread;
    return thread;
}

void enqueueLine(const QString& level, const QString& message) {
    const QString timestamp = QDateTime::currentDateTime().toString(QString::fromLatin1("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString line = QString::fromLatin1("%1 [%2] %3\n").arg(timestamp, level, message);
    writerThread()->enqueue(line);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
void messageHandler(QtMsgType type, const char* message) {
    enqueueLine(levelName(type), QString::fromLocal8Bit(message ? message : ""));
    if (type == QtFatalMsg) {
        std::abort();
    }
}
#else
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    Q_UNUSED(context)
    enqueueLine(levelName(type), message);
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
    return currentLogFilePath();
}

void AppLogger::writeLine(const QString& level, const QString& message) {
    enqueueLine(level, message);
}