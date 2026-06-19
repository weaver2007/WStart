#pragma once

#include <QString>

class LauncherWindowInterface {
public:
    virtual ~LauncherWindowInterface() {}
    virtual QString language() const = 0;
    virtual bool hotkeysEnabled() const = 0;
};
