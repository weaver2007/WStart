#include "AppIcon.h"
#include "AppLogger.h"
#include "MainWindow.h"
#include "SingleInstanceServer.h"
#include "TrayController.h"
#include "UiText.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QThread>
#include <QtPlugin>

#include <memory>

#if defined(HKM_IMPORT_QT5_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
#endif

namespace {
struct LoggerShutdownGuard {
    ~LoggerShutdownGuard() {
        AppLogger::shutdown();
    }
};

QString singleInstanceServerName() {
    return "WStart-SingleInstance";
}

bool notifyExistingInstance() {
    QLocalSocket socket;
    for (int attempt = 0; attempt < 10; ++attempt) {
        socket.connectToServer(singleInstanceServerName());
        if (socket.waitForConnected(100)) {
            socket.write("activate");
            socket.flush();
            socket.waitForBytesWritten(100);
            socket.disconnectFromServer();
            socket.waitForDisconnected(100);
            return true;
        }
        socket.abort();
        QThread::msleep(50);
    }
    return false;
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("WStart");
    QApplication::setOrganizationName("WStart");
    QApplication::setWindowIcon(AppIcon::launcherIcon());
    QApplication::setQuitOnLastWindowClosed(false);
    AppLogger::install();
    LoggerShutdownGuard loggerShutdownGuard;
    AppLogger::writeLine(QString::fromLatin1("INFO"),
                         QString::fromLatin1("WStart starting version=%1").arg(QString::fromLatin1(HKM_APP_VERSION)));
    const bool startupMinimized = app.arguments().contains(QString::fromLatin1("--startup-minimized"));

    const QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockDirectory.isEmpty()) {
        QDir().mkpath(lockDirectory);
    }

    std::unique_ptr<QLockFile> lockFile(
        new QLockFile(QDir(lockDirectory.isEmpty() ? QDir::tempPath() : lockDirectory).filePath("WStart.lock")));
    if (!lockFile->tryLock(0)) {
        if (!startupMinimized && notifyExistingInstance()) {
            return 0;
        }
        return 0;
    }

    QLocalServer::removeServer(singleInstanceServerName());
    SingleInstanceServer server;
    if (!server.listen(singleInstanceServerName())) {
        return 0;
    }

    MainWindow window;
    TrayController tray(&window);

    QObject::connect(&server, SIGNAL(activationRequested()), &window, SLOT(showSettings()));

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (!trayAvailable) {
        QMessageBox::warning(nullptr, "WStart", UiText::text("zh-CN", UiText::Key::SystemTrayUnavailable));
    }

    if (startupMinimized && trayAvailable) {
        window.showStartupMinimized();
    } else {
        window.show();
    }

    return app.exec();
}
