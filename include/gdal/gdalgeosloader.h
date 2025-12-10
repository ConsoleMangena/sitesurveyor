#ifndef GDALGEOSLOADER_H
#define GDALGEOSLOADER_H

#include <QString>
#include <QVector>
#include <QColor>

// Forward declarations
struct DxfData;
class GDALDataset;

/**
 * @brief GdalGeosLoader - Loads DXF files using GDAL and validates geometry with GEOS C API
 * 
 * This class replaces the old libdxfrw-based DxfReader with a robust GDAL + GEOS pipeline.
 * Key features:
 * - Uses GDAL's OGR DXF driver with DXF_INLINE_BLOCKS=TRUE to explode blocks
 * - Uses GEOS C API for geometry validation (isValid, makeValid)
 * - Outputs to existing DxfData structures for UI compatibility
 */
class GdalGeosLoader {
public:
    GdalGeosLoader();
    ~GdalGeosLoader();
    
    /**
     * @brief Load a DXF file and populate the target DxfData structure
     * @param filepath Path to the DXF file
     * @param targetData Output structure (will be cleared first)
     * @return true on success, false on error (check lastError())
     */
    bool loadDxf(const QString& filepath, DxfData& targetData);
    
    /**
     * @brief Get the last error message
     */
    QString lastError() const { return m_lastError; }
    
    /**
     * @brief Get statistics from the last load operation
     */
    int geometriesProcessed() const { return m_geometriesProcessed; }
    int geometriesRepaired() const { return m_geometriesRepaired; }
    int geometriesFailed() const { return m_geometriesFailed; }

private:
    // Process a single OGR feature
    void processFeature(void* feature, const QString& layerName, DxfData& targetData);
    
    // Extract coordinates from OGR geometry directly
    void extractOgrGeometry(void* ogrGeom, const QString& layer, 
                           const QColor& color, const QString& textValue,
                           double textHeight, double textAngle, DxfData& targetData);
    
    // Get color for a layer index
    QColor getLayerColor(int index);
    
    // Error handling
    QString m_lastError;
    
    // Statistics
    int m_geometriesProcessed{0};
    int m_geometriesRepaired{0};
    int m_geometriesFailed{0};
    
    // GEOS context handle (thread-local)
    void* m_geosContext{nullptr};
};

#endif // GDALGEOSLOADER_H
