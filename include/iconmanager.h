#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <QIcon>
#include <QString>
#include <QColor>

class IconManager {
public:
    // Retrieve icon by logical name from resources. In monochrome mode, icons are tinted to the monochrome color.
    static QIcon icon(const QString& name);

    // Enable/disable monochrome icons (useful for dark mode to force white icons)
    static void setMonochrome(bool enabled);
    static void setMonochromeColor(const QColor& color);

private:
    static bool s_monochrome;
    static QColor s_monoColor;
};

#endif // ICONMANAGER_H
