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
