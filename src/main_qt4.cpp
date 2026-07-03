#include "AppIcon.h"
#include "AppLogger.h"
#include "MainWindow.h"
#include "TrayController.h"
#include "UiText.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
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
#ifdef Q_OS_WIN
        Sleep(50);
#else
        QThread::msleep(50);
#endif
    }
    return false;
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const int dpi = QApplication::desktop() ? QApplication::desktop()->logicalDpiX() : 96;
    if (dpi > 96) {
        QFont font = app.font();
        const int pointSize = font.pointSize() > 0 ? font.pointSize() : 9;
        font.setPointSize(qBound(9, (pointSize * dpi + 48) / 96, 20));
        app.setFont(font);
    }
    QApplication::setApplicationName("WStart");
    QApplication::setOrganizationName("WStart");
    QApplication::setWindowIcon(AppIcon::launcherIcon());
    QApplication::setQuitOnLastWindowClosed(false);
    AppLogger::install();
    AppLogger::writeLine(QString::fromLatin1("INFO"), QString::fromLatin1("WStart starting version=%1").arg(QString::fromLatin1(HKM_APP_VERSION)));
    const bool startupMinimized = app.arguments().contains(QString::fromLatin1("--startup-minimized"));

    const QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockDirectory.isEmpty()) {
        QDir().mkpath(lockDirectory);
    }

    QLockFile lockFile(QDir(lockDirectory.isEmpty() ? QDir::tempPath() : lockDirectory).filePath("WStart.lock"));
    if (!lockFile.tryLock(0)) {
        if (!startupMinimized && notifyExistingInstance()) {
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
    TrayController tray(&window, &window);

    QObject::connect(&server, SIGNAL(newConnection()), &window, SLOT(showSettings()));

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (!trayAvailable) {
        QMessageBox::warning(0, "WStart", UiText::text("zh-CN", UiText::Key::SystemTrayUnavailable));
    }

    if (startupMinimized && trayAvailable) {
        window.showStartupMinimized();
    } else {
        window.show();
    }

    return app.exec();
}
