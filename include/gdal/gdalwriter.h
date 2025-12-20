#ifndef GDALWRITER_H
#define GDALWRITER_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>

// Forward declarations
struct CanvasPolyline;
struct CanvasPeg;

/**
 * @brief GdalWriter - Export survey data to GIS formats using GDAL/OGR
 */
class GdalWriter {
public:
    GdalWriter();
    ~GdalWriter();
    
    /**
     * @brief Export polylines to Shapefile format
     * @param polylines Vector of polylines to export
     * @param filePath Output file path (.shp)
     * @param crs Coordinate Reference System (e.g., "EPSG:4326")
     * @return true if successful
     */
    bool exportToShapefile(const QVector<CanvasPolyline>& polylines, 
                          const QString& filePath,
                          const QString& crs = "");
    
    /**
     * @brief Export pegs to Shapefile point format
     * @param pegs Vector of pegs to export
     * @param filePath Output file path (.shp)
     * @param crs Coordinate Reference System
     * @return true if successful
     */
    bool exportPegsToShapefile(const QVector<CanvasPeg>& pegs,
                               const QString& filePath,
                               const QString& crs = "");
    
    /**
     * @brief Export polylines to GeoJSON format
     * @param polylines Vector of polylines to export
     * @param filePath Output file path (.geojson)
     * @param crs Coordinate Reference System
     * @return true if successful
     */
    bool exportToGeoJSON(const QVector<CanvasPolyline>& polylines,
                        const QString& filePath,
                        const QString& crs = "");
    
    /**
     * @brief Export pegs to GeoJSON point format
     */
    bool exportPegsToGeoJSON(const QVector<CanvasPeg>& pegs,
                             const QString& filePath,
                             const QString& crs = "");
    
    /**
     * @brief Export to KML for Google Earth
     * @param polylines Vector of polylines to export
     * @param pegs Vector of pegs to export
     * @param filePath Output file path (.kml)
     * @return true if successful
     */
    bool exportToKML(const QVector<CanvasPolyline>& polylines,
                    const QVector<CanvasPeg>& pegs,
                    const QString& filePath);
    
    /**
     * @brief Get last error message
     */
    QString lastError() const { return m_lastError; }
    
private:
    QString m_lastError;
};

#endif // GDALWRITER_H
