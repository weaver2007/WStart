#pragma once

#include "HotkeyTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
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

private:
#ifdef Q_OS_WIN
    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam);
    LRESULT handleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* event);
    HotkeyModifiers currentModifiers(int eventKey, bool isKeyDown);
    const HotkeyRule* matchingRule(int virtualKey, HotkeyModifiers modifiers) const;
    bool isTrackedSuppressedKey(int virtualKey) const;
    bool isModifierKey(int virtualKey) const;
    void updatePressedModifierState(int virtualKey, bool pressed);
    void trackSuppressedChord(const HotkeyCombination& hotkey);
    void clearSuppressedKey(int virtualKey);

    HHOOK m_hook = nullptr;
    static HotkeyHookService* s_instance;
#endif
    QVector<HotkeyRule> m_rules;
    QSet<QString> m_activeTriggers;
#ifdef Q_OS_WIN
    QSet<int> m_suppressedKeys;
    QHash<int, QString> m_suppressedTriggerIds;
    HotkeyModifiers m_pressedModifiers = ModifierNone;
#endif
    bool m_paused = false;
};
