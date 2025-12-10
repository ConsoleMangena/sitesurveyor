#ifndef GDALREADER_H
#define GDALREADER_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QRectF>
#include <QImage>

// Forward declaration of GDAL types
class GDALDataset;

// Vector geometry types
struct GdalPoint {
    QPointF position;
    QString layer;
    QColor color;
};

struct GdalLineString {
    QVector<QPointF> points;
    QString layer;
    QColor color;
};

struct GdalPolygon {
    QVector<QVector<QPointF>> rings;  // First ring is exterior, rest are holes
    QString layer;
    QColor color;
    QColor fillColor;
};

struct GdalText {
    QString text;
    QPointF position;
    double height;
    double angle;
    QString layer;
    QColor color;
};

// Raster data
struct GdalRaster {
    QImage image;
    QRectF bounds;  // World coordinates
    QString layer;
};

// Layer info
struct GdalLayer {
    QString name;
    QString type;  // "vector" or "raster"
    int featureCount;
    QRectF extent;
    bool visible;
};

// Collected GDAL data
struct GdalData {
    QVector<GdalPoint> points;
    QVector<GdalLineString> lineStrings;
    QVector<GdalPolygon> polygons;
    QVector<GdalText> texts;
    QVector<GdalRaster> rasters;
    QVector<GdalLayer> layers;
    QRectF extent;
    QString crs;  // Coordinate Reference System (e.g., "EPSG:4326")
    
    void clear() {
        points.clear();
        lineStrings.clear();
        polygons.clear();
        texts.clear();
        rasters.clear();
        layers.clear();
        extent = QRectF();
        crs.clear();
    }
    
    bool isEmpty() const {
        return points.isEmpty() && lineStrings.isEmpty() && 
               polygons.isEmpty() && rasters.isEmpty();
    }
};

// GDAL Reader class
class GdalReader {
public:
    GdalReader();
    ~GdalReader();
    
    // Initialize GDAL (call once at startup)
    static void initialize();
    static void cleanup();
    
    // Read a file (auto-detects vector vs raster)
    bool readFile(const QString& fileName);
    
    // Get the loaded data
    const GdalData& data() const { return m_data; }
    
    // Get supported formats
    static QStringList supportedVectorFormats();
    static QStringList supportedRasterFormats();
    static QString fileFilter();
    
    // Get last error message
    QString lastError() const { return m_lastError; }

private:
    bool readVectorData(GDALDataset* dataset);
    bool readRasterData(GDALDataset* dataset);
    QColor getLayerColor(int index);
    
    GdalData m_data;
    QString m_lastError;
};

#endif // GDALREADER_H
