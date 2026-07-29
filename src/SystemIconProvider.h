#pragma once

#include "HotkeyTypes.h"

#include <QIcon>

namespace SystemIconProvider {

QIcon fileIcon(const QString& path);
QIcon systemToolIcon(const HotkeyRule& rule);

} // namespace SystemIconProvider
