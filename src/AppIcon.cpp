#include "AppIcon.h"

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

namespace {

QPixmap paintLauncherPixmap(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = size;
    QLinearGradient background(0, 0, s, s);
    background.setColorAt(0.0, QColor("#35d0be"));
    background.setColorAt(0.56, QColor("#2ba8d8"));
    background.setColorAt(1.0, QColor("#2378ee"));

    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(s * 0.04, s * 0.04, s * 0.92, s * 0.92), s * 0.18, s * 0.18);

    const QColor ink("#0a3857");
    const QRectF keyboard(s * 0.14, s * 0.30, s * 0.62, s * 0.39);
    painter.setBrush(QColor("#f8fcff"));
    painter.setPen(QPen(ink, qMax<qreal>(1.2, s * 0.035), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawRoundedRect(keyboard, s * 0.08, s * 0.08);

    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    const qreal keyHeight = qMax<qreal>(2.0, s * 0.045);
    painter.drawRoundedRect(QRectF(s * 0.24, s * 0.40, s * 0.13, keyHeight), keyHeight / 2, keyHeight / 2);
    painter.drawRoundedRect(QRectF(s * 0.41, s * 0.40, s * 0.13, keyHeight), keyHeight / 2, keyHeight / 2);
    painter.drawRoundedRect(QRectF(s * 0.58, s * 0.40, s * 0.11, keyHeight), keyHeight / 2, keyHeight / 2);
    painter.drawRoundedRect(QRectF(s * 0.24, s * 0.52, s * 0.23, keyHeight), keyHeight / 2, keyHeight / 2);
    painter.drawRoundedRect(QRectF(s * 0.51, s * 0.52, s * 0.17, keyHeight), keyHeight / 2, keyHeight / 2);

    QPolygonF bolt;
    bolt << QPointF(s * 0.73, s * 0.12)
         << QPointF(s * 0.47, s * 0.49)
         << QPointF(s * 0.63, s * 0.49)
         << QPointF(s * 0.49, s * 0.86)
         << QPointF(s * 0.88, s * 0.36)
         << QPointF(s * 0.69, s * 0.36);

    painter.setBrush(QColor("#ffe66d"));
    painter.setPen(QPen(ink, qMax<qreal>(1.2, s * 0.04), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolygon(bolt);

    QPolygonF boltHighlight;
    boltHighlight << QPointF(s * 0.70, s * 0.22)
                  << QPointF(s * 0.54, s * 0.45)
                  << QPointF(s * 0.68, s * 0.45)
                  << QPointF(s * 0.57, s * 0.72)
                  << QPointF(s * 0.80, s * 0.39)
                  << QPointF(s * 0.64, s * 0.39);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 244, 138, 190));
    painter.drawPolygon(boltHighlight);

    return pixmap;
}

} // namespace

namespace AppIcon {

QIcon launcherIcon()
{
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
    for (int i = 0; i < int(sizeof(sizes) / sizeof(sizes[0])); ++i) {
        icon.addPixmap(paintLauncherPixmap(sizes[i]));
    }
    return icon;
}

} // namespace AppIcon
