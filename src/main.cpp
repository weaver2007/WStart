#include "AppIcon.h"
#include "MainWindow.h"
#include "TrayController.h"
#include "UiText.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLocalServer>
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
QString singleInstanceServerName() {
    return "HotKeyManager-SingleInstance";
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
    QApplication::setApplicationName("HotKeyManager");
    QApplication::setOrganizationName("HotKeyManager");
    QApplication::setWindowIcon(AppIcon::launcherIcon());
    QApplication::setQuitOnLastWindowClosed(false);

    const QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockDirectory.isEmpty()) {
        QDir().mkpath(lockDirectory);
    }

    std::unique_ptr<QLockFile> lockFile(
        new QLockFile(QDir(lockDirectory.isEmpty() ? QDir::tempPath() : lockDirectory).filePath("HotKeyManager.lock")));
    if (!lockFile->tryLock(0)) {
        if (notifyExistingInstance()) {
            return 0;
        }
        return 0;
    }

    QLocalServer::removeServer(singleInstanceServerName());
    QLocalServer server;
    if (!server.listen(singleInstanceServerName())) {
        return 0;
    }

    MainWindow window;
    TrayController tray(&window);

    QObject::connect(&server, &QLocalServer::newConnection, &app, [&]() {
        while (server.hasPendingConnections()) {
            if (QLocalSocket* socket = server.nextPendingConnection()) {
                window.showSettings();
                socket->disconnectFromServer();
                socket->deleteLater();
            }
        }
    });

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(nullptr, "HotKeyManager", UiText::text("zh-CN", UiText::Key::SystemTrayUnavailable));
    }

    window.show();

    return app.exec();
}
