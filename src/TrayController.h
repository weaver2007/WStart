#pragma once

#include "UiText.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class LauncherWindowInterface;
class MainWindow;

class TrayController : public QObject {
    Q_OBJECT

public:
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    explicit TrayController(MainWindow *window, QObject *parent = nullptr);
#endif
    explicit TrayController(QMainWindow *window, LauncherWindowInterface *launcher, QObject *parent = nullptr);

public slots:
    void retranslateUi();
    void setHotkeysEnabled(bool enabled);

private:
    QString uiText(UiText::Key key) const;

    QMainWindow *m_window = nullptr;
    LauncherWindowInterface *m_launcher = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_hotkeysAction = nullptr;
    QAction *m_exitAction = nullptr;
};
