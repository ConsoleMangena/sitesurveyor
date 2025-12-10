#include "gdal/gdalreader.h"
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <QFileInfo>
#include <QDebug>

// Color palette for layers
static const QColor s_layerColors[] = {
    QColor(255, 0, 0),      // Red
    QColor(0, 255, 0),      // Green
    QColor(0, 0, 255),      // Blue
    QColor(255, 255, 0),    // Yellow
    QColor(255, 0, 255),    // Magenta
    QColor(0, 255, 255),    // Cyan
    QColor(255, 128, 0),    // Orange
    QColor(128, 0, 255),    // Purple
    QColor(0, 128, 255),    // Light Blue
    QColor(255, 0, 128),    // Pink
};
static const int s_numColors = sizeof(s_layerColors) / sizeof(s_layerColors[0]);

GdalReader::GdalReader() {}

GdalReader::~GdalReader() {}

void GdalReader::initialize()
{
    GDALAllRegister();
}

void GdalReader::cleanup()
{
    // GDAL cleanup is automatic in recent versions
}

bool GdalReader::readFile(const QString& fileName)
{
    m_data.clear();
    m_lastError.clear();
    
    // Open dataset
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(fileName.toUtf8().constData(), 
                   GDAL_OF_READONLY | GDAL_OF_VECTOR | GDAL_OF_RASTER,
                   nullptr, nullptr, nullptr));
    
    if (!dataset) {
        m_lastError = QString("Failed to open file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    bool success = false;
    
    // Check for vector data (layers)
    int layerCount = dataset->GetLayerCount();
    if (layerCount > 0) {
        success = readVectorData(dataset);
    }
    
    // Check for raster data (bands)
    int bandCount = dataset->GetRasterCount();
    if (bandCount > 0) {
        success = readRasterData(dataset) || success;
    }
    
    GDALClose(dataset);
    
    if (!success && m_lastError.isEmpty()) {
        m_lastError = "No vector or raster data found in file";
    }
    
    return success;
}

bool GdalReader::readVectorData(GDALDataset* dataset)
{
    int layerCount = dataset->GetLayerCount();
    if (layerCount == 0) return false;
    
    for (int i = 0; i < layerCount; ++i) {
        OGRLayer* layer = dataset->GetLayer(i);
        if (!layer) continue;
        
        QString layerName = QString::fromUtf8(layer->GetName());
        QColor layerColor = getLayerColor(i);
        
        // Get layer extent
        OGREnvelope envelope;
        if (layer->GetExtent(&envelope) == OGRERR_NONE) {
            QRectF layerExtent(envelope.MinX, envelope.MinY,
                              envelope.MaxX - envelope.MinX,
                              envelope.MaxY - envelope.MinY);
            if (m_data.extent.isNull()) {
                m_data.extent = layerExtent;
            } else {
                m_data.extent = m_data.extent.united(layerExtent);
            }
        }
        
        // Get CRS
        OGRSpatialReference* srs = layer->GetSpatialRef();
        if (srs && m_data.crs.isEmpty()) {
            const char* authName = srs->GetAuthorityName(nullptr);
            const char* authCode = srs->GetAuthorityCode(nullptr);
            if (authName && authCode) {
                m_data.crs = QString("%1:%2").arg(authName).arg(authCode);
            }
        }
        
        // Add layer info
        GdalLayer layerInfo;
        layerInfo.name = layerName;
        layerInfo.type = "vector";
        layerInfo.featureCount = static_cast<int>(layer->GetFeatureCount());
        layerInfo.extent = QRectF(envelope.MinX, envelope.MinY,
                                  envelope.MaxX - envelope.MinX,
                                  envelope.MaxY - envelope.MinY);
        layerInfo.visible = true;
        m_data.layers.append(layerInfo);
        
        // Read features
        layer->ResetReading();
        OGRFeature* feature;
        while ((feature = layer->GetNextFeature()) != nullptr) {
            OGRGeometry* geometry = feature->GetGeometryRef();
            if (!geometry) {
                OGRFeature::DestroyFeature(feature);
                continue;
            }
            
            OGRwkbGeometryType geomType = wkbFlatten(geometry->getGeometryType());
            
            switch (geomType) {
                case wkbPoint:
                case wkbPoint25D: {
                    OGRPoint* point = static_cast<OGRPoint*>(geometry);
                    GdalPoint gp;
                    gp.position = QPointF(point->getX(), point->getY());
                    gp.layer = layerName;
                    gp.color = layerColor;
                    m_data.points.append(gp);
                    break;
                }
                
                case wkbMultiPoint:
                case wkbMultiPoint25D: {
                    OGRMultiPoint* multiPoint = static_cast<OGRMultiPoint*>(geometry);
                    for (int j = 0; j < multiPoint->getNumGeometries(); ++j) {
                        OGRPoint* point = static_cast<OGRPoint*>(multiPoint->getGeometryRef(j));
                        GdalPoint gp;
                        gp.position = QPointF(point->getX(), point->getY());
                        gp.layer = layerName;
                        gp.color = layerColor;
                        m_data.points.append(gp);
                    }
                    break;
                }
                
                case wkbLineString:
                case wkbLineString25D: {
                    OGRLineString* lineString = static_cast<OGRLineString*>(geometry);
                    GdalLineString gls;
                    gls.layer = layerName;
                    gls.color = layerColor;
                    for (int j = 0; j < lineString->getNumPoints(); ++j) {
                        gls.points.append(QPointF(lineString->getX(j), lineString->getY(j)));
                    }
                    if (gls.points.size() >= 2) {
                        m_data.lineStrings.append(gls);
                    }
                    break;
                }
                
                case wkbMultiLineString:
                case wkbMultiLineString25D: {
                    OGRMultiLineString* multiLine = static_cast<OGRMultiLineString*>(geometry);
                    for (int k = 0; k < multiLine->getNumGeometries(); ++k) {
                        OGRLineString* lineString = static_cast<OGRLineString*>(multiLine->getGeometryRef(k));
                        GdalLineString gls;
                        gls.layer = layerName;
                        gls.color = layerColor;
                        for (int j = 0; j < lineString->getNumPoints(); ++j) {
                            gls.points.append(QPointF(lineString->getX(j), lineString->getY(j)));
                        }
                        if (gls.points.size() >= 2) {
                            m_data.lineStrings.append(gls);
                        }
                    }
                    break;
                }
                
                case wkbPolygon:
                case wkbPolygon25D: {
                    OGRPolygon* polygon = static_cast<OGRPolygon*>(geometry);
                    GdalPolygon gp;
                    gp.layer = layerName;
                    gp.color = layerColor;
                    gp.fillColor = QColor(layerColor.red(), layerColor.green(), layerColor.blue(), 50);
                    
                    // Exterior ring
                    OGRLinearRing* extRing = polygon->getExteriorRing();
                    if (extRing) {
                        QVector<QPointF> ring;
                        for (int j = 0; j < extRing->getNumPoints(); ++j) {
                            ring.append(QPointF(extRing->getX(j), extRing->getY(j)));
                        }
                        gp.rings.append(ring);
                    }
                    
                    // Interior rings (holes)
                    for (int r = 0; r < polygon->getNumInteriorRings(); ++r) {
                        OGRLinearRing* intRing = polygon->getInteriorRing(r);
                        QVector<QPointF> ring;
                        for (int j = 0; j < intRing->getNumPoints(); ++j) {
                            ring.append(QPointF(intRing->getX(j), intRing->getY(j)));
                        }
                        gp.rings.append(ring);
                    }
                    
                    if (!gp.rings.isEmpty()) {
                        m_data.polygons.append(gp);
                    }
                    break;
                }
                
                case wkbMultiPolygon:
                case wkbMultiPolygon25D: {
                    OGRMultiPolygon* multiPoly = static_cast<OGRMultiPolygon*>(geometry);
                    for (int p = 0; p < multiPoly->getNumGeometries(); ++p) {
                        OGRPolygon* polygon = static_cast<OGRPolygon*>(multiPoly->getGeometryRef(p));
                        GdalPolygon gp;
                        gp.layer = layerName;
                        gp.color = layerColor;
                        gp.fillColor = QColor(layerColor.red(), layerColor.green(), layerColor.blue(), 50);
                        
                        OGRLinearRing* extRing = polygon->getExteriorRing();
                        if (extRing) {
                            QVector<QPointF> ring;
                            for (int j = 0; j < extRing->getNumPoints(); ++j) {
                                ring.append(QPointF(extRing->getX(j), extRing->getY(j)));
                            }
                            gp.rings.append(ring);
                        }
                        
                        for (int r = 0; r < polygon->getNumInteriorRings(); ++r) {
                            OGRLinearRing* intRing = polygon->getInteriorRing(r);
                            QVector<QPointF> ring;
                            for (int j = 0; j < intRing->getNumPoints(); ++j) {
                                ring.append(QPointF(intRing->getX(j), intRing->getY(j)));
                            }
                            gp.rings.append(ring);
                        }
                        
                        if (!gp.rings.isEmpty()) {
                            m_data.polygons.append(gp);
                        }
                    }
                    break;
                }
                
                default:
                    // Unsupported geometry type
                    break;
            }
            
            OGRFeature::DestroyFeature(feature);
        }
    }
    
    return !m_data.points.isEmpty() || !m_data.lineStrings.isEmpty() || !m_data.polygons.isEmpty();
}

bool GdalReader::readRasterData(GDALDataset* dataset)
{
    int bandCount = dataset->GetRasterCount();
    if (bandCount == 0) return false;
    
    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();
    
    // Get geotransform
    double geoTransform[6];
    if (dataset->GetGeoTransform(geoTransform) != CE_None) {
        // No transform, use pixel coordinates
        geoTransform[0] = 0;      // Top left X
        geoTransform[1] = 1;      // Pixel width
        geoTransform[2] = 0;      // Rotation (0 for north-up)
        geoTransform[3] = height; // Top left Y
        geoTransform[4] = 0;      // Rotation
        geoTransform[5] = -1;     // Pixel height (negative for north-up)
    }
    
    // Calculate extent
    double minX = geoTransform[0];
    double maxY = geoTransform[3];
    double maxX = geoTransform[0] + width * geoTransform[1];
    double minY = geoTransform[3] + height * geoTransform[5];
    
    QRectF rasterExtent(minX, minY, maxX - minX, maxY - minY);
    if (m_data.extent.isNull()) {
        m_data.extent = rasterExtent;
    } else {
        m_data.extent = m_data.extent.united(rasterExtent);
    }
    
    // Get CRS
    const char* projWkt = dataset->GetProjectionRef();
    if (projWkt && projWkt[0] != '\0' && m_data.crs.isEmpty()) {
        OGRSpatialReference srs;
        if (srs.importFromWkt(projWkt) == OGRERR_NONE) {
            const char* authName = srs.GetAuthorityName(nullptr);
            const char* authCode = srs.GetAuthorityCode(nullptr);
            if (authName && authCode) {
                m_data.crs = QString("%1:%2").arg(authName).arg(authCode);
            }
        }
    }
    
    // Add layer info
    GdalLayer layerInfo;
    layerInfo.name = "Raster";
    layerInfo.type = "raster";
    layerInfo.featureCount = 1;
    layerInfo.extent = rasterExtent;
    layerInfo.visible = true;
    m_data.layers.append(layerInfo);
    
    // Read raster data into QImage
    QImage image;
    
    if (bandCount >= 3) {
        // RGB or RGBA
        image = QImage(width, height, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        
        GDALRasterBand* redBand = dataset->GetRasterBand(1);
        GDALRasterBand* greenBand = dataset->GetRasterBand(2);
        GDALRasterBand* blueBand = dataset->GetRasterBand(3);
        GDALRasterBand* alphaBand = (bandCount >= 4) ? dataset->GetRasterBand(4) : nullptr;
        
        QVector<uint8_t> redData(width * height);
        QVector<uint8_t> greenData(width * height);
        QVector<uint8_t> blueData(width * height);
        QVector<uint8_t> alphaData(width * height, 255);
        
        redBand->RasterIO(GF_Read, 0, 0, width, height, redData.data(), width, height, GDT_Byte, 0, 0);
        greenBand->RasterIO(GF_Read, 0, 0, width, height, greenData.data(), width, height, GDT_Byte, 0, 0);
        blueBand->RasterIO(GF_Read, 0, 0, width, height, blueData.data(), width, height, GDT_Byte, 0, 0);
        if (alphaBand) {
            alphaBand->RasterIO(GF_Read, 0, 0, width, height, alphaData.data(), width, height, GDT_Byte, 0, 0);
        }
        
        for (int y = 0; y < height; ++y) {
            uchar* line = image.scanLine(y);
            for (int x = 0; x < width; ++x) {
                int idx = y * width + x;
                line[x * 4 + 0] = redData[idx];
                line[x * 4 + 1] = greenData[idx];
                line[x * 4 + 2] = blueData[idx];
                line[x * 4 + 3] = alphaData[idx];
            }
        }
    } else {
        // Grayscale
        image = QImage(width, height, QImage::Format_Grayscale8);
        
        GDALRasterBand* band = dataset->GetRasterBand(1);
        QVector<uint8_t> data(width * height);
        band->RasterIO(GF_Read, 0, 0, width, height, data.data(), width, height, GDT_Byte, 0, 0);
        
        for (int y = 0; y < height; ++y) {
            uchar* line = image.scanLine(y);
            for (int x = 0; x < width; ++x) {
                line[x] = data[y * width + x];
            }
        }
    }
    
    GdalRaster raster;
    raster.image = image;
    raster.bounds = rasterExtent;
    raster.layer = "Raster";
    m_data.rasters.append(raster);
    
    return true;
}

QColor GdalReader::getLayerColor(int index)
{
    return s_layerColors[index % s_numColors];
}

QStringList GdalReader::supportedVectorFormats()
{
    return QStringList() << "Shapefile (*.shp)"
                         << "GeoJSON (*.geojson *.json)"
                         << "GeoPackage (*.gpkg)"
                         << "KML (*.kml *.kmz)"
                         << "GPS Exchange (*.gpx)"
                         << "MapInfo (*.tab *.mif)"
                         << "CSV (*.csv)";
}

QStringList GdalReader::supportedRasterFormats()
{
    return QStringList() << "GeoTIFF (*.tif *.tiff)"
                         << "JPEG (*.jpg *.jpeg)"
                         << "PNG (*.png)"
                         << "BMP (*.bmp)"
                         << "ECW (*.ecw)"
                         << "JPEG2000 (*.jp2)";
}

QString GdalReader::fileFilter()
{
    return "All Supported Files (*.shp *.geojson *.json *.gpkg *.kml *.kmz *.gpx *.tif *.tiff *.jpg *.jpeg *.png);;"
           "Shapefile (*.shp);;"
           "GeoJSON (*.geojson *.json);;"
           "GeoPackage (*.gpkg);;"
           "KML (*.kml *.kmz);;"
           "GeoTIFF (*.tif *.tiff);;"
           "All Files (*)";
}
