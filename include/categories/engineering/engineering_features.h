#ifndef ENGINEERING_FEATURES_H
#define ENGINEERING_FEATURES_H

#include "categories/category_manager.h"
#include <QList>

class CanvasWidget;

/**
 * @brief Engineering Surveying Features
 * 
 * Available tools:
 * - Station Setup / Backsight
 * - Stakeout from Station
 * - Offset Polyline
 * - Partition Tool
 * - Levelling Tool
 * - GAMA Network Adjustment
 * - All Modify tools (Select, Explode, Break, Join, etc.)
 */
namespace EngineeringFeatures {

    // Setup default layers for this category
    void setupLayers(CanvasWidget* canvas);
    
    // Get list of available features
    QList<CategoryManager::Feature> getAvailableFeatures();
    
    // Check if specific feature is available
    bool isFeatureAvailable(CategoryManager::Feature feature);
}

#endif // ENGINEERING_FEATURES_H
