#pragma once

#include "HotkeyTypes.h"

#include <QHash>
#include <QSet>
#include <QVector>

// Platform-independent state owned exclusively by the keyboard-hook thread.
// Keeping matching and suppression here makes key-order regressions testable
// without installing a global Windows hook.
class HotkeyState {
public:
    void reset();
    void setModifierKey(int key, HotkeyModifier modifier, bool pressed);
    HotkeyModifiers pressedModifiers() const;

    bool isSuppressed(int key) const;
    bool isTriggerActive(const QString& triggerId) const;
    bool claimTrigger(int key, const QString& triggerId);
    QString releaseSuppressedKey(int key);

    static bool findMatchingRule(const QVector<HotkeyRule>& rules, int key, HotkeyModifiers modifiers,
                                 HotkeyRule* match = nullptr);

private:
    HotkeyModifiers m_pressedModifiers = ModifierNone;
    QHash<int, HotkeyModifier> m_pressedModifierKeys;
    QSet<int> m_suppressedKeys;
    QHash<int, QString> m_suppressedTriggerIds;
    QSet<QString> m_activeTriggers;
};

struct WinKeyRecoveryAction {
    int key = 0;
    bool releaseSystemKey = false;
};

// Tracks the physical and forwarded lifetimes separately. A low-level hook can
// consume an event after Windows has already seen an earlier Win-down, so a
// single "pressed" flag is not enough to decide whether a compensating key-up
// is required.
class WinKeyState {
public:
    enum class EventDisposition { Forward, Suppress };

    void reset();
    void synchronizeKey(int key, bool down, quint32 eventTime);

    EventDisposition handleKeyDown(int key, quint32 eventTime);
    EventDisposition handleKeyUp(int key, quint32 eventTime);

    QVector<int> claimPressedKeys(quint32 eventTime);
    QVector<int> claimedPressedKeys() const;
    QVector<WinKeyRecoveryAction> expireStaleKeys(quint32 currentTime, quint32 forwardedTimeout,
                                                  quint32 claimedTimeout);
    QVector<int> claimForwardedKeys(quint32 eventTime);
    void markSystemReleaseFailed(int key, quint32 eventTime);

    bool isPhysicallyDown(int key) const;
    bool isForwardedDown(int key) const;
    bool isClaimed(int key) const;

private:
    struct KeyState {
        bool physicallyDown = false;
        bool forwardedDown = false;
        bool claimed = false;
        quint32 lastEventTime = 0;
    };

    QHash<int, KeyState> m_keys;
};
