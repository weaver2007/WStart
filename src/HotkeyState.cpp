#include "HotkeyState.h"

void HotkeyState::reset() {
    m_pressedModifiers = ModifierNone;
    m_pressedModifierKeys.clear();
    m_suppressedKeys.clear();
    m_suppressedTriggerIds.clear();
    m_activeTriggers.clear();
}

void HotkeyState::setModifierKey(int key, HotkeyModifier modifier, bool pressed) {
    if (key == 0 || modifier == ModifierNone) {
        return;
    }
    if (pressed) {
        m_pressedModifierKeys.insert(key, modifier);
    } else {
        m_pressedModifierKeys.remove(key);
    }

    m_pressedModifiers = ModifierNone;
    for (QHash<int, HotkeyModifier>::const_iterator it = m_pressedModifierKeys.constBegin();
         it != m_pressedModifierKeys.constEnd(); ++it) {
        m_pressedModifiers |= it.value();
    }
}

HotkeyModifiers HotkeyState::pressedModifiers() const {
    return m_pressedModifiers;
}

bool HotkeyState::isSuppressed(int key) const {
    return m_suppressedKeys.contains(key);
}

bool HotkeyState::isTriggerActive(const QString& triggerId) const {
    return m_activeTriggers.contains(triggerId);
}

bool HotkeyState::claimTrigger(int key, const QString& triggerId) {
    if (key == 0 || triggerId.isEmpty() || m_activeTriggers.contains(triggerId)) {
        return false;
    }
    m_activeTriggers.insert(triggerId);
    m_suppressedKeys.insert(key);
    m_suppressedTriggerIds.insert(key, triggerId);
    return true;
}

QString HotkeyState::releaseSuppressedKey(int key) {
    const QString triggerId = m_suppressedTriggerIds.take(key);
    m_suppressedKeys.remove(key);
    if (!triggerId.isEmpty()) {
        m_activeTriggers.remove(triggerId);
    }
    return triggerId;
}

bool HotkeyState::findMatchingRule(const QVector<HotkeyRule>& rules, int key, HotkeyModifiers modifiers,
                                   HotkeyRule* match) {
    for (const HotkeyRule& rule : rules) {
        if (rule.enabled && rule.hotkey.isValid() && rule.hotkey.key == key && rule.hotkey.modifiers == modifiers) {
            if (match) {
                *match = rule;
            }
            return true;
        }
    }
    return false;
}

void WinKeyState::reset() {
    m_keys.clear();
}

void WinKeyState::synchronizeKey(int key, bool down, quint32 eventTime) {
    if (!down) {
        m_keys.remove(key);
        return;
    }

    KeyState& state = m_keys[key];
    state.physicallyDown = true;
    state.forwardedDown = true;
    state.claimed = false;
    state.lastEventTime = eventTime;
}

WinKeyState::EventDisposition WinKeyState::handleKeyDown(int key, quint32 eventTime) {
    KeyState& state = m_keys[key];
    if (!state.physicallyDown) {
        // A new down starts a new lifecycle even if a late up from a previously
        // recovered key never arrived.
        state.physicallyDown = true;
        state.forwardedDown = true;
        state.claimed = false;
        state.lastEventTime = eventTime;
        return EventDisposition::Forward;
    }

    state.lastEventTime = eventTime;
    if (state.claimed) {
        return EventDisposition::Suppress;
    }

    state.forwardedDown = true;
    return EventDisposition::Forward;
}

WinKeyState::EventDisposition WinKeyState::handleKeyUp(int key, quint32 eventTime) {
    Q_UNUSED(eventTime)
    const QHash<int, KeyState>::iterator it = m_keys.find(key);
    if (it == m_keys.end()) {
        return EventDisposition::Forward;
    }

    const bool suppress = it->claimed && !it->forwardedDown;
    m_keys.erase(it);
    return suppress ? EventDisposition::Suppress : EventDisposition::Forward;
}

QVector<int> WinKeyState::claimPressedKeys(quint32 eventTime) {
    QVector<int> releaseKeys;
    for (QHash<int, KeyState>::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        KeyState& state = it.value();
        if (!state.physicallyDown) {
            continue;
        }
        state.claimed = true;
        state.lastEventTime = eventTime;
        if (state.forwardedDown) {
            state.forwardedDown = false;
            releaseKeys.push_back(it.key());
        }
    }
    return releaseKeys;
}

QVector<int> WinKeyState::claimedPressedKeys() const {
    QVector<int> keys;
    for (QHash<int, KeyState>::const_iterator it = m_keys.constBegin(); it != m_keys.constEnd(); ++it) {
        if (it->physicallyDown && it->claimed) {
            keys.push_back(it.key());
        }
    }
    return keys;
}

QVector<WinKeyRecoveryAction> WinKeyState::expireStaleKeys(quint32 currentTime, quint32 forwardedTimeout,
                                                           quint32 claimedTimeout) {
    QVector<WinKeyRecoveryAction> actions;
    for (QHash<int, KeyState>::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        KeyState& state = it.value();
        if (!state.physicallyDown) {
            continue;
        }

        const quint32 timeout = state.claimed ? claimedTimeout : forwardedTimeout;
        // Unsigned subtraction remains correct when GetTickCount wraps.
        if (timeout == 0 || static_cast<quint32>(currentTime - state.lastEventTime) < timeout) {
            continue;
        }

        WinKeyRecoveryAction action;
        action.key = it.key();
        action.releaseSystemKey = state.forwardedDown;
        actions.push_back(action);

        state.physicallyDown = false;
        state.forwardedDown = false;
        state.claimed = true;
        state.lastEventTime = currentTime;
    }
    return actions;
}

QVector<int> WinKeyState::claimForwardedKeys(quint32 eventTime) {
    QVector<int> releaseKeys;
    for (QHash<int, KeyState>::iterator it = m_keys.begin(); it != m_keys.end(); ++it) {
        KeyState& state = it.value();
        if (!state.forwardedDown) {
            continue;
        }
        state.forwardedDown = false;
        state.claimed = true;
        state.lastEventTime = eventTime;
        releaseKeys.push_back(it.key());
    }
    return releaseKeys;
}

void WinKeyState::markSystemReleaseFailed(int key, quint32 eventTime) {
    KeyState& state = m_keys[key];
    state.physicallyDown = true;
    state.forwardedDown = true;
    state.claimed = true;
    state.lastEventTime = eventTime;
}

bool WinKeyState::isPhysicallyDown(int key) const {
    const QHash<int, KeyState>::const_iterator it = m_keys.constFind(key);
    return it != m_keys.constEnd() && it->physicallyDown;
}

bool WinKeyState::isForwardedDown(int key) const {
    const QHash<int, KeyState>::const_iterator it = m_keys.constFind(key);
    return it != m_keys.constEnd() && it->forwardedDown;
}

bool WinKeyState::isClaimed(int key) const {
    const QHash<int, KeyState>::const_iterator it = m_keys.constFind(key);
    return it != m_keys.constEnd() && it->claimed;
}
