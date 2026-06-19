#pragma once

#include "ActionRunner.h"
#include "HotkeyHookService.h"
#include "LauncherWindowInterface.h"
#include "RuleStore.h"

#include <QMainWindow>

class QListWidget;
class QListWidgetItem;
class QLabel;

class MainWindowQt4 : public QMainWindow, public LauncherWindowInterface {
    Q_OBJECT

public:
    explicit MainWindowQt4(QWidget *parent = 0);
    QString language() const;
    bool hotkeysEnabled() const;

public slots:
    void showSettings();
    void setHotkeysPaused(bool paused);
    void applyHotkeysEnabled(bool enabled);

signals:
    void languageChanged(const QString &language);
    void hotkeysEnabledChanged(bool enabled);

private slots:
    void runCurrentItem(QListWidgetItem *item);
    void onHotkeyTriggered(const HotkeyRule &rule);
    void setStatus(const QString &message);

private:
    void buildUi();
    void loadDocument();
    void refreshList();
    int ruleIndexById(const QString &ruleId) const;

    RuleStore m_store;
    ActionRunner m_runner;
    HotkeyHookService m_hookService;
    LauncherDocument m_document;
    QListWidget *m_list;
    QLabel *m_status;
    bool m_hotkeysEnabled;
};
