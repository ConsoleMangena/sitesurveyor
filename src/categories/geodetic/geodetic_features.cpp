#include "categories/geodetic/geodetic_features.h"
#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

namespace GeodeticFeatures {

/**
 * Setup default layers for Geodetic Surveying projects
 */
void setupLayers(CanvasWidget* canvas)
{
    if (!canvas) return;
    
    canvas->addLayer("Control Points", QColor(255, 0, 0));
    canvas->addLayer("Baselines", QColor(0, 255, 0));
    canvas->addLayer("Benchmarks", QColor(255, 255, 0));
    canvas->addLayer("Network", QColor(100, 149, 237));
}

/**
 * Get list of available features for Geodetic Surveying
 */
QList<CategoryManager::Feature> getAvailableFeatures()
{
    return {
        CategoryManager::StationSetup,
        CategoryManager::Backsight,
        CategoryManager::Stakeout,
        CategoryManager::GamaAdjust,
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
 * Check if a feature is available for Geodetic Surveying
 */
bool isFeatureAvailable(CategoryManager::Feature feature)
{
    switch (feature) {
        case CategoryManager::StationSetup:
        case CategoryManager::Backsight:
        case CategoryManager::Stakeout:
        case CategoryManager::GamaAdjust:
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

} // namespace GeodeticFeatures
