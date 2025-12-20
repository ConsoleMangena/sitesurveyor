#ifndef GEODETIC_FEATURES_H
#define GEODETIC_FEATURES_H

#include "categories/category_manager.h"
#include <QList>

class CanvasWidget;

/**
 * @brief Geodetic Surveying Features
 * 
 * Available tools:
 * - Station Setup / Backsight
 * - Stakeout from Station
 * - GAMA Network Adjustment
 * - All Modify tools
 * 
 * Future additions:
 * - Coordinate Transformations
 * - Geoid Model Integration
 * - GNSS Baseline Processing
 * - Session Planning
 */
namespace GeodeticFeatures {

    void setupLayers(CanvasWidget* canvas);
    QList<CategoryManager::Feature> getAvailableFeatures();
    bool isFeatureAvailable(CategoryManager::Feature feature);
}

#endif // GEODETIC_FEATURES_H
