#include "categories/mining/mining_features.h"
#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

namespace MiningFeatures {

/**
 * Setup default layers for Mining Surveying projects
 */
void setupLayers(CanvasWidget* canvas)
{
    if (!canvas) return;
    
    canvas->addLayer("Ore Body", QColor(255, 215, 0));
    canvas->addLayer("Waste", QColor(128, 128, 128));
    canvas->addLayer("Development", QColor(100, 149, 237));
    canvas->addLayer("Ventilation", QColor(0, 255, 255));
    canvas->addLayer("Services", QColor(255, 165, 0));
    canvas->addLayer("Safety", QColor(255, 0, 0));
}

/**
 * Get list of available features for Mining Surveying
 */
QList<CategoryManager::Feature> getAvailableFeatures()
{
    return {
        CategoryManager::StationSetup,
        CategoryManager::Backsight,
        CategoryManager::Stakeout,
        CategoryManager::Levelling,
        CategoryManager::GamaAdjust,
        CategoryManager::VolumeCalc,
        CategoryManager::Sections,
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
 * Check if a feature is available for Mining Surveying
 */
bool isFeatureAvailable(CategoryManager::Feature feature)
{
    switch (feature) {
        case CategoryManager::StationSetup:
        case CategoryManager::Backsight:
        case CategoryManager::Stakeout:
        case CategoryManager::Levelling:
        case CategoryManager::GamaAdjust:
        case CategoryManager::VolumeCalc:
        case CategoryManager::Sections:
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

} // namespace MiningFeatures
