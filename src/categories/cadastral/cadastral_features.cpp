#include "categories/cadastral/cadastral_features.h"
#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

namespace CadastralFeatures {

/**
 * Setup default layers for Cadastral Surveying projects
 */
void setupLayers(CanvasWidget* canvas)
{
    if (!canvas) return;
    
    canvas->addLayer("Boundary", QColor(255, 255, 255));
    canvas->addLayer("Beacons", QColor(255, 165, 0));
    canvas->addLayer("Pegs", QColor(255, 0, 0));
    canvas->addLayer("Offset", QColor(0, 255, 255));
    canvas->addLayer("Servitudes", QColor(255, 255, 0));
    canvas->addLayer("Annotation", QColor(200, 200, 200));
}

/**
 * Get list of available features for Cadastral Surveying
 */
QList<CategoryManager::Feature> getAvailableFeatures()
{
    return {
        CategoryManager::StationSetup,
        CategoryManager::Backsight,
        CategoryManager::Stakeout,
        CategoryManager::Offset,
        CategoryManager::Partition,
        CategoryManager::GamaAdjust,
        CategoryManager::AreaCalc,
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
 * Check if a feature is available for Cadastral Surveying
 */
bool isFeatureAvailable(CategoryManager::Feature feature)
{
    switch (feature) {
        case CategoryManager::StationSetup:
        case CategoryManager::Backsight:
        case CategoryManager::Stakeout:
        case CategoryManager::Offset:
        case CategoryManager::Partition:
        case CategoryManager::GamaAdjust:
        case CategoryManager::AreaCalc:
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

} // namespace CadastralFeatures
