#include "AppIcon.h"
#include "MainWindow.h"
#include "TrayController.h"
#include "UiText.h"

#include <QApplication>
#include <QDir>
#include <QDesktopWidget>
#include <QFont>
#include <QIcon>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QStandardPaths>

#include <windows.h>

namespace {
QString singleInstanceServerName()
{
    return "HotKeyManager-SingleInstance";
}

bool notifyExistingInstance()
{
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
        Sleep(50);
    }
    return false;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const int dpi = QApplication::desktop() ? QApplication::desktop()->logicalDpiX() : 96;
    if (dpi > 96) {
        QFont font = app.font();
        const int pointSize = font.pointSize() > 0 ? font.pointSize() : 9;
        font.setPointSize(qBound(9, (pointSize * dpi + 48) / 96, 20));
        app.setFont(font);
    }
    QApplication::setApplicationName("HotKeyManager");
    QApplication::setOrganizationName("HotKeyManager");
    QApplication::setWindowIcon(AppIcon::launcherIcon());
    QApplication::setQuitOnLastWindowClosed(false);

    const QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!lockDirectory.isEmpty()) {
        QDir().mkpath(lockDirectory);
    }

    QLockFile lockFile(QDir(lockDirectory.isEmpty() ? QDir::tempPath() : lockDirectory).filePath("HotKeyManager.lock"));
    if (!lockFile.tryLock(0)) {
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
    TrayController tray(&window, &window);

    QObject::connect(&server, SIGNAL(newConnection()), &window, SLOT(showSettings()));

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(0, "HotKeyManager", UiText::text("zh-CN", UiText::Key::SystemTrayUnavailable));
    }

    window.show();

    return app.exec();
}
