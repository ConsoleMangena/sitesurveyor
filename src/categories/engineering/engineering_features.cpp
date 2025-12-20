#include "categories/engineering/engineering_features.h"
#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

namespace EngineeringFeatures {

/**
 * Setup default layers for Engineering Surveying projects
 */
void setupLayers(CanvasWidget* canvas)
{
    if (!canvas) return;
    
    canvas->addLayer("Site Boundary", QColor(255, 255, 255));
    canvas->addLayer("Buildings", QColor(100, 149, 237));
    canvas->addLayer("Roads", QColor(128, 128, 128));
    canvas->addLayer("Services", QColor(255, 165, 0));
    canvas->addLayer("Setout Points", QColor(255, 0, 0));
    canvas->addLayer("Levels", QColor(0, 255, 0));
}

/**
 * Get list of available features for Engineering Surveying
 */
QList<CategoryManager::Feature> getAvailableFeatures()
{
    return {
        // Station setup
        CategoryManager::StationSetup,
        CategoryManager::Backsight,
        CategoryManager::Stakeout,
        
        // Engineering calculation tools
        CategoryManager::Offset,
        CategoryManager::Partition,
        CategoryManager::Levelling,
        CategoryManager::GamaAdjust,
        CategoryManager::Resection,
        CategoryManager::Intersection,
        CategoryManager::PolarCalculation,
        CategoryManager::JoinCalculation,
        
        // Modify tools
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
 * Check if a feature is available for Engineering Surveying
 */
bool isFeatureAvailable(CategoryManager::Feature feature)
{
    switch (feature) {
        case CategoryManager::StationSetup:
        case CategoryManager::Backsight:
        case CategoryManager::Stakeout:
        case CategoryManager::Offset:
        case CategoryManager::Partition:
        case CategoryManager::Levelling:
        case CategoryManager::GamaAdjust:
        case CategoryManager::Resection:
        case CategoryManager::Intersection:
        case CategoryManager::PolarCalculation:
        case CategoryManager::JoinCalculation:
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

} // namespace EngineeringFeatures
