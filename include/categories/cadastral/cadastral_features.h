#ifndef CADASTRAL_FEATURES_H
#define CADASTRAL_FEATURES_H

#include "categories/category_manager.h"
#include <QList>

class CanvasWidget;

/**
 * @brief Cadastral Surveying Features
 * 
 * Available tools:
 * - Station Setup / Backsight
 * - Stakeout from Station
 * - Offset Polyline
 * - Partition Tool  
 * - GAMA Network Adjustment
 * - All Modify tools
 * 
 * Future additions:
 * - Area Calculation
 * - Coordinate Schedule Export
 * - Diagram Generation
 * - Subdivision Tool
 */
namespace CadastralFeatures {

    void setupLayers(CanvasWidget* canvas);
    QList<CategoryManager::Feature> getAvailableFeatures();
    bool isFeatureAvailable(CategoryManager::Feature feature);
}

#endif // CADASTRAL_FEATURES_H
