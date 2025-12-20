#ifndef TOPOGRAPHIC_FEATURES_H
#define TOPOGRAPHIC_FEATURES_H

#include "categories/category_manager.h"
#include <QList>

class CanvasWidget;

/**
 * @brief Topographic Surveying Features
 * 
 * Available tools:
 * - Station Setup / Backsight
 * - Levelling Tool
 * - All Modify tools
 * 
 * Future additions:
 * - Contour Generation
 * - DTM Creation
 * - Spot Level Labels
 * - Feature Coding
 * - Break Lines (Stream/Ridge)
 */
namespace TopographicFeatures {

    void setupLayers(CanvasWidget* canvas);
    QList<CategoryManager::Feature> getAvailableFeatures();
    bool isFeatureAvailable(CategoryManager::Feature feature);
}

#endif // TOPOGRAPHIC_FEATURES_H
