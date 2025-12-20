#ifndef MINING_FEATURES_H
#define MINING_FEATURES_H

#include "categories/category_manager.h"
#include <QList>

class CanvasWidget;

/**
 * @brief Mining Surveying Features
 * 
 * Available tools:
 * - Station Setup / Backsight
 * - Stakeout from Station
 * - Levelling Tool
 * - GAMA Network Adjustment
 * - All Modify tools
 * 
 * Future additions:
 * - Volume Calculation (Cut/Fill, Stockpile)
 * - Section Generation
 * - Decline/Incline Gradient Tools
 * - Stope Survey
 * - Blast Pattern Design
 */
namespace MiningFeatures {

    void setupLayers(CanvasWidget* canvas);
    QList<CategoryManager::Feature> getAvailableFeatures();
    bool isFeatureAvailable(CategoryManager::Feature feature);
}

#endif // MINING_FEATURES_H
