#ifndef CATEGORY_MANAGER_H
#define CATEGORY_MANAGER_H

#include <QStringList>
#include <QMap>
#include "app/startdialog.h"

class CanvasWidget;
class QAction;
class QMenu;

/**
 * @brief CategoryManager - Manages feature visibility based on survey category
 * 
 * Each survey discipline has different tools available:
 * - Engineering: Offset, Partition, Levelling, Station Setup, Stakeout
 * - Cadastral: Offset, Partition, Station Setup, Stakeout, GAMA
 * - Mining: Levelling, Station Setup, Volume (future), Sections (future)
 * - Topographic: Levelling, Contours (future), DTM (future)
 * - Geodetic: Station Setup, Stakeout, GAMA, Network Adjustment
 */
class CategoryManager
{
public:
    // Feature IDs for menu/toolbar actions
    enum Feature {
        // Common
        StationSetup,
        Backsight,
        Stakeout,
        
        // Engineering/Cadastral
        Offset,
        Partition,
        
        // Measurement
        Levelling,
        GamaAdjust,
        
        // Modify tools (all categories)
        Select,
        Explode,
        Break,
        Join,
        CloseOpen,
        Reverse,
        Erase,
        
        // Future features
        VolumeCalc,     // Mining
        Sections,       // Mining
        Contours,       // Topo
        AreaCalc,       // Cadastral
        
        // New Engineering Tools
        Resection,
        Intersection,
        PolarCalculation,   // Compute coords from bearing+distance
        JoinCalculation,    // Compute bearing+distance from 2 points
        TraverseCalculation, // Traverse with Bowditch/Transit adjustment
        
        FeatureCount
    };
    
    // Check if a feature is available for a category
    static bool isFeatureAvailable(SurveyCategory category, Feature feature);
    
    // Get list of available features for a category
    static QList<Feature> getAvailableFeatures(SurveyCategory category);
    
    // Setup default layers for a category
    static void setupDefaultLayers(CanvasWidget* canvas, SurveyCategory category);
    
    // Get category display name
    static QString getCategoryDisplayName(SurveyCategory category);
    
    // Get category icon name
    static QString getCategoryIcon(SurveyCategory category);
};

#endif // CATEGORY_MANAGER_H
