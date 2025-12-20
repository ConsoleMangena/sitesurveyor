#include "categories/topographic/topographic_features.h"
#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

namespace TopographicFeatures {

/**
 * Setup default layers for Topographic Surveying projects
 */
void setupLayers(CanvasWidget* canvas)
{
    if (!canvas) return;
    
    canvas->addLayer("Contours", QColor(139, 69, 19));
    canvas->addLayer("Spot Levels", QColor(0, 255, 0));
    canvas->addLayer("Buildings", QColor(255, 255, 255));
    canvas->addLayer("Vegetation", QColor(34, 139, 34));
    canvas->addLayer("Water", QColor(0, 191, 255));
    canvas->addLayer("Roads", QColor(128, 128, 128));
}

/**
 * Get list of available features for Topographic Surveying
 */
QList<CategoryManager::Feature> getAvailableFeatures()
{
    return {
        CategoryManager::StationSetup,
        CategoryManager::Backsight,
        CategoryManager::Levelling,
        CategoryManager::Contours,
        CategoryManager::Select,
        CategoryManager::Explode,
        CategoryManager::Break,
        CategoryManager::Join,
        CategoryManager::CloseOpen,
        CategoryManager::Reverse,
        CategoryManager::Erase
    };
}

/**
 * Check if a feature is available for Topographic Surveying
 */
bool isFeatureAvailable(CategoryManager::Feature feature)
{
    switch (feature) {
        case CategoryManager::StationSetup:
        case CategoryManager::Backsight:
        case CategoryManager::Levelling:
        case CategoryManager::Contours:
        case CategoryManager::Select:
        case CategoryManager::Explode:
        case CategoryManager::Break:
        case CategoryManager::Join:
        case CategoryManager::CloseOpen:
        case CategoryManager::Reverse:
        case CategoryManager::Erase:
            return true;
        default:
            return false;
    }
}

} // namespace TopographicFeatures
