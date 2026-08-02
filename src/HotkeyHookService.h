#pragma once

#include "HotkeyState.h"
#include "HotkeyTypes.h"

#include <QMutex>
#include <QObject>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
class HotkeyHookThread;
#endif

class HotkeyHookService : public QObject {
    Q_OBJECT

public:
    explicit HotkeyHookService(QObject* parent = nullptr);
    ~HotkeyHookService() override;

    bool start(QString* error = nullptr);
    void stop();
    bool isRunning() const;
    void setPaused(bool paused);
    bool isPaused() const;
    void setRules(const QVector<HotkeyRule>& rules);

signals:
    void hotkeyTriggered(const HotkeyRule& rule);
    void hookError(const QString& message);
    void queuedHotkeyTriggered(const HotkeyRule& rule);
    void diagnosticQueued(const QString& message);

private slots:
    void writeQueuedDiagnostic(const QString& message);

private:
#ifdef Q_OS_WIN
    friend class HotkeyHookThread;

    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam);
    LRESULT handleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* event);
    HotkeyModifiers currentModifiers(int eventKey, bool isKeyDown, bool isKeyUp);
    bool matchingRule(int virtualKey, HotkeyModifiers modifiers, HotkeyRule* match) const;
    bool isTrackedSuppressedKey(int virtualKey) const;
    bool isModifierKey(int virtualKey) const;
    void updatePressedModifierState(int virtualKey, bool pressed);
    void resetKeyboardState(const QString& reason);
    void synchronizePhysicalModifierState();
    void recoverStaleWinKeys(quint32 currentTime);
    void releaseForwardedWinKeys(const QString& reason, quint32 currentTime);
    void releaseStaleSystemWinKeysAtStartup();
    void synchronizeWinStateFlags();
    void queueDiagnostic(const QString& message);

    HHOOK m_hook = nullptr;
    HotkeyHookThread* m_thread = nullptr;
    volatile LONG m_running = 0;
    volatile LONG m_paused = 0;
    static HotkeyHookService* s_instance;
#endif
    QVector<HotkeyRule> m_rules;
    mutable QMutex m_rulesMutex;
#ifdef Q_OS_WIN
    HotkeyState m_state;
    WinKeyState m_winState;
    bool m_leftWinPhysicallyDown = false;
    bool m_rightWinPhysicallyDown = false;
#endif
#ifndef Q_OS_WIN
    bool m_paused = false;
#endif
};
