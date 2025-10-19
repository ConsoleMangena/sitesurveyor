#include "iconmanager.h"
#include <QIcon>
#include <QString>
#include <QFile>
#include <QPixmap>
#include <QPainter>

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

void IconManager::setMonochrome(bool enabled)
{
    s_monochrome = enabled;
}

void IconManager::setMonochromeColor(const QColor& color)
{
    s_monoColor = color;
}
