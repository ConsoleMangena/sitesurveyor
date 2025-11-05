#include "iconmanager.h"
#include <QIcon>
#include <QString>
#include <QFile>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QPen>

bool IconManager::s_monochrome = false;
QColor IconManager::s_monoColor = Qt::white;

static QString iconPathFor(const QString& name)
{
    const QString p1 = QString(":/icons/%1.svg").arg(name);
    if (QFile::exists(p1)) return p1;
    const QString p2 = QString(":/icons/icons/%1.svg").arg(name);
    if (QFile::exists(p2)) return p2;
    return QString();
}

static QIcon tintedFromIcon(const QIcon& base, const QColor& color)
{
    if (base.isNull()) return QIcon();
    QIcon out;
    const QList<int> sizes = {16, 18, 20, 24};
    for (int s : sizes) {
        QPixmap pm = base.pixmap(s, s);
        if (pm.isNull()) continue;
        QPixmap tinted(pm.size());
        tinted.fill(Qt::transparent);
        QPainter pt(&tinted);
        pt.fillRect(tinted.rect(), color);
        pt.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        pt.drawPixmap(0, 0, pm);
        pt.end();
        out.addPixmap(tinted);
    }
    return out;
}

QIcon IconManager::icon(const QString& name)
{
    const QString path = iconPathFor(name);
    if (path.isEmpty()) return QIcon();
    QIcon base(path);
    if (!s_monochrome) return base;
    return tintedFromIcon(base, s_monoColor);
}

QIcon IconManager::iconUnique(const QString& name, const QString& key, const QString& badgeText)
{
    QIcon base = icon(name);
    if (base.isNull()) return base;
    QIcon out;
    QList<int> sizes = {12, 14, 16, 18, 20, 24};
    uint h = qHash(key.isEmpty() ? name : key);
    int hue = int(h % 360);
    QColor badge = s_monochrome ? QColor(32,32,32) : QColor::fromHsv(hue, 200, 235);
    for (int s : sizes) {
        QPixmap pm = base.pixmap(s, s);
        if (pm.isNull()) continue;
        QPixmap composed = pm;
        QPainter p(&composed);
        p.setRenderHint(QPainter::Antialiasing, true);
        int d = qMax(6, s/2);
        QRect r(s - d, s - d, d - 1, d - 1);
        QPen outline(Qt::white);
        outline.setWidth(1);
        p.setPen(outline);
        p.setBrush(badge);
        p.drawEllipse(r);
        if (!badgeText.isEmpty() && !s_monochrome) {
            QFont f = p.font();
            int px = qMax(6, d - 2);
            f.setPixelSize(px/2 + (s >= 18 ? 1 : 0));
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::black);
            p.drawText(r, Qt::AlignCenter, badgeText.left(2).toUpper());
        }
        p.end();
        out.addPixmap(composed);
    }
    return out;
}

void IconManager::setMonochrome(bool enabled)
{
    s_monochrome = enabled;
}

void IconManager::setMonochromeColor(const QColor& color)
{
    s_monoColor = color;
}
