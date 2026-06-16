#pragma once

#include "MainWindow.h"

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(MainWindow *window, QObject *parent = nullptr);

public slots:
    void retranslateUi();
    void setHotkeysEnabled(bool enabled);

private:
    QString uiText(UiText::Key key) const;

    MainWindow *m_window = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_hotkeysAction = nullptr;
    QAction *m_exitAction = nullptr;
};
