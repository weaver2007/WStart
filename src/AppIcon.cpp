#include "AppIcon.h"

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

QPixmap paintLauncherPixmap(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = size;
    QLinearGradient background(0, 0, s, s);
    background.setColorAt(0.0, QColor("#19d2c2"));
    background.setColorAt(0.52, QColor("#1095ed"));
    background.setColorAt(1.0, QColor("#1557d8"));

    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(s * 0.05, s * 0.05, s * 0.90, s * 0.90), s * 0.20, s * 0.20);

    const QColor ink("#063a5b");
    const QColor tile("#e9fbff");
    const qreal tileSize = s * 0.15;
    const qreal tileRadius = s * 0.04;
    const qreal left = s * 0.18;
    const qreal top = s * 0.19;
    const qreal gap = s * 0.055;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(tile.red(), tile.green(), tile.blue(), 220));
    painter.drawRoundedRect(QRectF(left, top, tileSize, tileSize), tileRadius, tileRadius);
    painter.drawRoundedRect(QRectF(left + tileSize + gap, top, tileSize, tileSize), tileRadius, tileRadius);
    painter.drawRoundedRect(QRectF(left, top + tileSize + gap, tileSize, tileSize), tileRadius, tileRadius);
    painter.drawRoundedRect(QRectF(left + tileSize + gap, top + tileSize + gap, tileSize, tileSize), tileRadius,
                            tileRadius);

    QPainterPath wPath;
    wPath.moveTo(QPointF(s * 0.19, s * 0.38));
    wPath.lineTo(QPointF(s * 0.34, s * 0.78));
    wPath.lineTo(QPointF(s * 0.50, s * 0.46));
    wPath.lineTo(QPointF(s * 0.65, s * 0.78));
    wPath.lineTo(QPointF(s * 0.82, s * 0.34));

    painter.setPen(QPen(ink, qMax<qreal>(2.0, s * 0.15), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(wPath);
    painter.setPen(QPen(QColor("#ffffff"), qMax<qreal>(1.5, s * 0.095), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(wPath);

    const QPointF badgeCenter(s * 0.75, s * 0.27);
    const qreal badgeRadius = s * 0.13;
    painter.setPen(QPen(ink, qMax<qreal>(1.2, s * 0.035)));
    painter.setBrush(QColor("#ffffff"));
    painter.drawEllipse(badgeCenter, badgeRadius, badgeRadius);

    QPainterPath playPath;
    playPath.moveTo(QPointF(s * 0.72, s * 0.20));
    playPath.lineTo(QPointF(s * 0.72, s * 0.34));
    playPath.lineTo(QPointF(s * 0.84, s * 0.27));
    playPath.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#13c88f"));
    painter.drawPath(playPath);

    return pixmap;
}

} // namespace

namespace AppIcon {

QIcon launcherIcon() {
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
    for (int i = 0; i < int(sizeof(sizes) / sizeof(sizes[0])); ++i) {
        icon.addPixmap(paintLauncherPixmap(sizes[i]));
    }
    return icon;
}

} // namespace AppIcon
