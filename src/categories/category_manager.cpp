#include "categories/category_manager.h"
#include "canvas/canvaswidget.h"
#include <QColor>

bool CategoryManager::isFeatureAvailable(SurveyCategory category, Feature feature)
{
    switch (feature) {
        // Core features - always available to ALL categories
        case StationSetup:
        case Backsight:
        case Stakeout:
        case Select:
        case Explode:
        case Break:
        case Join:
        case CloseOpen:
        case Reverse:
        case Erase:
        case GamaAdjust:
            return true;
            
        // Offset/Partition - Engineering and Cadastral only
        case Offset:
        case Partition:
            return (category == SurveyCategory::Engineering || 
                    category == SurveyCategory::Cadastral);
            
        // Levelling - Engineering only
        case Levelling:
            return (category == SurveyCategory::Engineering);
            
        // Future category-specific features
        case VolumeCalc:
        case Sections:
            return (category == SurveyCategory::Mining);
        case Contours:
            return (category == SurveyCategory::Topographic);
        case AreaCalc:
            return (category == SurveyCategory::Cadastral);
            
        // Resection/Intersection - Engineering and Cadastral
        case Resection:
        case Intersection:
            return (category == SurveyCategory::Engineering || 
                    category == SurveyCategory::Cadastral);
        
        // Traverse Calculation - Engineering, Cadastral, and Geodetic
        case TraverseCalculation:
            return (category == SurveyCategory::Engineering || 
                    category == SurveyCategory::Cadastral ||
                    category == SurveyCategory::Geodetic);
            
        default:
            return true;
    }
}


QList<CategoryManager::Feature> CategoryManager::getAvailableFeatures(SurveyCategory category)
{
    QList<Feature> features;
    for (int i = 0; i < FeatureCount; ++i) {
        Feature f = static_cast<Feature>(i);
        if (isFeatureAvailable(category, f)) {
            features.append(f);
        }
    }
    return features;
}

void CategoryManager::setupDefaultLayers(CanvasWidget* canvas, SurveyCategory category)
{
    if (!canvas) return;
    canvas->clearAll();
    
    switch (category) {
        case SurveyCategory::Engineering:
            canvas->addLayer("Site Boundary", QColor(255, 255, 255));
            canvas->addLayer("Buildings", QColor(100, 149, 237));
            canvas->addLayer("Roads", QColor(128, 128, 128));
            canvas->addLayer("Services", QColor(255, 165, 0));
            canvas->addLayer("Setout Points", QColor(255, 0, 0));
            canvas->addLayer("Levels", QColor(0, 255, 0));
            break;
            
        case SurveyCategory::Cadastral:
            canvas->addLayer("Boundary", QColor(255, 255, 255));
            canvas->addLayer("Beacons", QColor(255, 165, 0));
            canvas->addLayer("Pegs", QColor(255, 0, 0));
            canvas->addLayer("Offset", QColor(0, 255, 255));
            canvas->addLayer("Servitudes", QColor(255, 255, 0));
            canvas->addLayer("Annotation", QColor(200, 200, 200));
            break;
            
        case SurveyCategory::Mining:
            canvas->addLayer("Ore Body", QColor(255, 215, 0));
            canvas->addLayer("Waste", QColor(128, 128, 128));
            canvas->addLayer("Development", QColor(100, 149, 237));
            canvas->addLayer("Ventilation", QColor(0, 255, 255));
            canvas->addLayer("Services", QColor(255, 165, 0));
            canvas->addLayer("Safety", QColor(255, 0, 0));
            break;
            
        case SurveyCategory::Topographic:
            canvas->addLayer("Contours", QColor(139, 69, 19));
            canvas->addLayer("Spot Levels", QColor(0, 255, 0));
            canvas->addLayer("Buildings", QColor(255, 255, 255));
            canvas->addLayer("Vegetation", QColor(34, 139, 34));
            canvas->addLayer("Water", QColor(0, 191, 255));
            canvas->addLayer("Roads", QColor(128, 128, 128));
            break;
            
        case SurveyCategory::Geodetic:
            canvas->addLayer("Control Points", QColor(255, 0, 0));
            canvas->addLayer("Baselines", QColor(0, 255, 0));
            canvas->addLayer("Benchmarks", QColor(255, 255, 0));
            canvas->addLayer("Network", QColor(100, 149, 237));
            break;
    }
}

QString CategoryManager::getCategoryDisplayName(SurveyCategory category)
{
    switch (category) {
        case SurveyCategory::Engineering: return "Engineering Surveying";
        case SurveyCategory::Cadastral: return "Cadastral Surveying";
        case SurveyCategory::Mining: return "Mining Surveying";
        case SurveyCategory::Topographic: return "Topographic Surveying";
        case SurveyCategory::Geodetic: return "Geodetic Surveying";
    }
    return "Survey";
}

QString CategoryManager::getCategoryIcon(SurveyCategory category)
{
    switch (category) {
        case SurveyCategory::Engineering: return "🔧";
        case SurveyCategory::Cadastral: return "📐";
        case SurveyCategory::Mining: return "⛏️";
        case SurveyCategory::Topographic: return "🗺️";
        case SurveyCategory::Geodetic: return "📡";
    }
    return "📋";
}
