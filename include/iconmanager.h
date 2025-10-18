#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <QIcon>
#include <QString>

class IconManager {
public:
    static QIcon icon(const QString& name) {
        // Load from embedded Qt resource path
        const QString path = QString(":/icons/%1.svg").arg(name);
        QIcon ic(path);
        return ic;
    }
};

#endif // ICONMANAGER_H
