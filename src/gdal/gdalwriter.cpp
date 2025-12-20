#include "gdal/gdalwriter.h"
#include "canvas/canvaswidget.h"
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <QFileInfo>
#include <QDebug>

GdalWriter::GdalWriter() {}
GdalWriter::~GdalWriter() {}

bool GdalWriter::exportToShapefile(const QVector<CanvasPolyline>& polylines,
                                   const QString& filePath,
                                   const QString& crs)
{
    m_lastError.clear();
    
    if (polylines.isEmpty()) {
        m_lastError = "No polylines to export";
        return false;
    }
    
    // Get driver
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (!driver) {
        m_lastError = "Shapefile driver not available";
        return false;
    }
    
    // Create dataset
    GDALDataset* dataset = driver->Create(filePath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        m_lastError = QString("Failed to create file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    // Set up spatial reference if provided
    OGRSpatialReference* srs = nullptr;
    if (!crs.isEmpty()) {
        srs = new OGRSpatialReference();
        if (srs->SetFromUserInput(crs.toUtf8().constData()) != OGRERR_NONE) {
            delete srs;
            srs = nullptr;
        }
    }
    
    // Determine geometry type (polygon or polyline)
    bool hasPolygons = false;
    bool hasPolylines = false;
    for (const auto& poly : polylines) {
        if (poly.closed) hasPolygons = true;
        else hasPolylines = true;
    }
    
    // Create layer(s)
    if (hasPolylines) {
        OGRLayer* lineLayer = dataset->CreateLayer("polylines", srs, wkbLineString, nullptr);
        if (lineLayer) {
            // Add name field
            OGRFieldDefn nameField("Name", OFTString);
            nameField.SetWidth(64);
            if (lineLayer->CreateField(&nameField) != OGRERR_NONE) {
                qWarning() << "Failed to create Name field";
            }
            
            OGRFieldDefn layerField("Layer", OFTString);
            layerField.SetWidth(64);
            if (lineLayer->CreateField(&layerField) != OGRERR_NONE) {
                qWarning() << "Failed to create Layer field";
            }
            
            for (int i = 0; i < polylines.size(); ++i) {
                const auto& poly = polylines[i];
                if (poly.closed) continue;  // Skip polygons
                
                OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
                feature->SetField("Name", QString("Line_%1").arg(i).toUtf8().constData());
                feature->SetField("Layer", poly.layer.toUtf8().constData());
                
                OGRLineString line;
                for (const auto& pt : poly.points) {
                    line.addPoint(pt.x(), pt.y());
                }
                feature->SetGeometry(&line);
                if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
                     qWarning() << "Failed to create feature";
                }
                OGRFeature::DestroyFeature(feature);
            }
        }
    }
    
    if (hasPolygons) {
        OGRLayer* polyLayer = dataset->CreateLayer("polygons", srs, wkbPolygon, nullptr);
        if (polyLayer) {
            OGRFieldDefn nameField("Name", OFTString);
            nameField.SetWidth(64);
            if (polyLayer->CreateField(&nameField) != OGRERR_NONE) {
                qWarning() << "Failed to create Name field";
            }
            
            OGRFieldDefn layerField("Layer", OFTString);
            layerField.SetWidth(64);
            if (polyLayer->CreateField(&layerField) != OGRERR_NONE) {
                 qWarning() << "Failed to create Layer field";
            }
            
            OGRFieldDefn areaField("Area", OFTReal);
            if (polyLayer->CreateField(&areaField) != OGRERR_NONE) {
                qWarning() << "Failed to create Area field";
            }
            
            for (int i = 0; i < polylines.size(); ++i) {
                const auto& poly = polylines[i];
                if (!poly.closed) continue;  // Skip polylines
                
                OGRFeature* feature = OGRFeature::CreateFeature(polyLayer->GetLayerDefn());
                feature->SetField("Name", QString("Polygon_%1").arg(i).toUtf8().constData());
                feature->SetField("Layer", poly.layer.toUtf8().constData());
                
                OGRLinearRing ring;
                for (const auto& pt : poly.points) {
                    ring.addPoint(pt.x(), pt.y());
                }
                ring.closeRings();
                
                OGRPolygon polygon;
                polygon.addRing(&ring);
                feature->SetGeometry(&polygon);
                feature->SetField("Area", polygon.get_Area());
                
                if (polyLayer->CreateFeature(feature) != OGRERR_NONE) {
                     qWarning() << "Failed to create feature";
                }
                OGRFeature::DestroyFeature(feature);
            }
        }
    }
    
    if (srs) delete srs;
    GDALClose(dataset);
    
    return true;
}

bool GdalWriter::exportPegsToShapefile(const QVector<CanvasPeg>& pegs,
                                       const QString& filePath,
                                       const QString& crs)
{
    m_lastError.clear();
    
    if (pegs.isEmpty()) {
        m_lastError = "No pegs to export";
        return false;
    }
    
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (!driver) {
        m_lastError = "Shapefile driver not available";
        return false;
    }
    
    GDALDataset* dataset = driver->Create(filePath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        m_lastError = QString("Failed to create file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    OGRSpatialReference* srs = nullptr;
    if (!crs.isEmpty()) {
        srs = new OGRSpatialReference();
        if (srs->SetFromUserInput(crs.toUtf8().constData()) != OGRERR_NONE) {
            delete srs;
            srs = nullptr;
        }
    }
    
    OGRLayer* layer = dataset->CreateLayer("pegs", srs, wkbPoint, nullptr);
    if (!layer) {
        m_lastError = "Failed to create layer";
        if (srs) delete srs;
        GDALClose(dataset);
        return false;
    }
    
    // Fields
    OGRFieldDefn nameField("Name", OFTString);
    nameField.SetWidth(64);
    if (layer->CreateField(&nameField) != OGRERR_NONE) {
        qWarning() << "Failed to create Name field";
    }
    
    OGRFieldDefn xField("X", OFTReal);
    if (layer->CreateField(&xField) != OGRERR_NONE) {
        qWarning() << "Failed to create X field";
    }
    
    OGRFieldDefn yField("Y", OFTReal);
    if (layer->CreateField(&yField) != OGRERR_NONE) {
        qWarning() << "Failed to create Y field";
    }
    
    OGRFieldDefn layerField("Layer", OFTString);
    layerField.SetWidth(64);
    if (layer->CreateField(&layerField) != OGRERR_NONE) {
        qWarning() << "Failed to create Layer field";
    }
    
    for (const auto& peg : pegs) {
        OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
        feature->SetField("Name", peg.name.toUtf8().constData());
        feature->SetField("X", peg.position.x());
        feature->SetField("Y", peg.position.y());
        feature->SetField("Layer", peg.layer.toUtf8().constData());
        
        OGRPoint point(peg.position.x(), peg.position.y());
        feature->SetGeometry(&point);
        
        if (layer->CreateFeature(feature) != OGRERR_NONE) {
            qWarning() << "Failed to create feature";
        }
        OGRFeature::DestroyFeature(feature);
    }
    
    if (srs) delete srs;
    GDALClose(dataset);
    
    return true;
}

bool GdalWriter::exportToGeoJSON(const QVector<CanvasPolyline>& polylines,
                                 const QString& filePath,
                                 const QString& crs)
{
    m_lastError.clear();
    
    if (polylines.isEmpty()) {
        m_lastError = "No polylines to export";
        return false;
    }
    
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    if (!driver) {
        m_lastError = "GeoJSON driver not available";
        return false;
    }
    
    GDALDataset* dataset = driver->Create(filePath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        m_lastError = QString("Failed to create file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    OGRSpatialReference* srs = nullptr;
    if (!crs.isEmpty()) {
        srs = new OGRSpatialReference();
        if (srs->SetFromUserInput(crs.toUtf8().constData()) != OGRERR_NONE) {
            delete srs;
            srs = nullptr;
        }
    }
    
    OGRLayer* layer = dataset->CreateLayer("features", srs, wkbUnknown, nullptr);
    if (!layer) {
        m_lastError = "Failed to create layer";
        if (srs) delete srs;
        GDALClose(dataset);
        return false;
    }
    
    OGRFieldDefn nameField("name", OFTString);
    if (layer->CreateField(&nameField) != OGRERR_NONE) {
        qWarning() << "Failed to create name field";
    }
    
    OGRFieldDefn typeField("type", OFTString);
    if (layer->CreateField(&typeField) != OGRERR_NONE) {
        qWarning() << "Failed to create type field";
    }
    
    OGRFieldDefn layerField("layer", OFTString);
    if (layer->CreateField(&layerField) != OGRERR_NONE) {
        qWarning() << "Failed to create layer field";
    }
    
    for (int i = 0; i < polylines.size(); ++i) {
        const auto& poly = polylines[i];
        
        OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
        feature->SetField("name", QString("Feature_%1").arg(i).toUtf8().constData());
        feature->SetField("type", poly.closed ? "Polygon" : "LineString");
        feature->SetField("layer", poly.layer.toUtf8().constData());
        
        if (poly.closed) {
            OGRLinearRing ring;
            for (const auto& pt : poly.points) {
                ring.addPoint(pt.x(), pt.y());
            }
            ring.closeRings();
            OGRPolygon polygon;
            polygon.addRing(&ring);
            feature->SetGeometry(&polygon);
        } else {
            OGRLineString line;
            for (const auto& pt : poly.points) {
                line.addPoint(pt.x(), pt.y());
            }
            feature->SetGeometry(&line);
        }
        
        if (layer->CreateFeature(feature) != OGRERR_NONE) {
            qWarning() << "Failed to create feature";
        }
        OGRFeature::DestroyFeature(feature);
    }
    
    if (srs) delete srs;
    GDALClose(dataset);
    
    return true;
}

bool GdalWriter::exportPegsToGeoJSON(const QVector<CanvasPeg>& pegs,
                                     const QString& filePath,
                                     const QString& crs)
{
    m_lastError.clear();
    
    if (pegs.isEmpty()) {
        m_lastError = "No pegs to export";
        return false;
    }
    
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    if (!driver) {
        m_lastError = "GeoJSON driver not available";
        return false;
    }
    
    GDALDataset* dataset = driver->Create(filePath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        m_lastError = QString("Failed to create file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    OGRLayer* layer = dataset->CreateLayer("pegs", nullptr, wkbPoint, nullptr);
    if (!layer) {
        m_lastError = "Failed to create layer";
        GDALClose(dataset);
        return false;
    }
    
    OGRFieldDefn nameField("name", OFTString);
    if (layer->CreateField(&nameField) != OGRERR_NONE) {
        qWarning() << "Failed to create name field";
    }
    
    OGRFieldDefn xField("x", OFTReal);
    if (layer->CreateField(&xField) != OGRERR_NONE) {
        qWarning() << "Failed to create x field";
    }
    
    OGRFieldDefn yField("y", OFTReal);
    if (layer->CreateField(&yField) != OGRERR_NONE) {
        qWarning() << "Failed to create y field";
    }
    
    for (const auto& peg : pegs) {
        OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
        feature->SetField("name", peg.name.toUtf8().constData());
        feature->SetField("x", peg.position.x());
        feature->SetField("y", peg.position.y());
        
        OGRPoint point(peg.position.x(), peg.position.y());
        feature->SetGeometry(&point);
        
        if (layer->CreateFeature(feature) != OGRERR_NONE) {
            qWarning() << "Failed to create feature";
        }
        OGRFeature::DestroyFeature(feature);
    }
    
    GDALClose(dataset);
    return true;
}

bool GdalWriter::exportToKML(const QVector<CanvasPolyline>& polylines,
                             const QVector<CanvasPeg>& pegs,
                             const QString& filePath)
{
    m_lastError.clear();
    
    if (polylines.isEmpty() && pegs.isEmpty()) {
        m_lastError = "No data to export";
        return false;
    }
    
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("KML");
    if (!driver) {
        m_lastError = "KML driver not available";
        return false;
    }
    
    GDALDataset* dataset = driver->Create(filePath.toUtf8().constData(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        m_lastError = QString("Failed to create file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    // KML uses WGS84 by default
    OGRSpatialReference srs;
    srs.SetWellKnownGeogCS("WGS84");
    
    // Pegs layer
    if (!pegs.isEmpty()) {
        OGRLayer* pegLayer = dataset->CreateLayer("Pegs", &srs, wkbPoint, nullptr);
        if (pegLayer) {
            OGRFieldDefn nameField("Name", OFTString);
            if (pegLayer->CreateField(&nameField) != OGRERR_NONE) {
                qWarning() << "Failed to create Name field";
            }
            
            for (const auto& peg : pegs) {
                OGRFeature* feature = OGRFeature::CreateFeature(pegLayer->GetLayerDefn());
                feature->SetField("Name", peg.name.toUtf8().constData());
                
                OGRPoint point(peg.position.x(), peg.position.y());
                feature->SetGeometry(&point);
                
                if (pegLayer->CreateFeature(feature) != OGRERR_NONE) {
                    qWarning() << "Failed to create feature";
                }
                OGRFeature::DestroyFeature(feature);
            }
        }
    }
    
    // Polylines layer
    if (!polylines.isEmpty()) {
        OGRLayer* lineLayer = dataset->CreateLayer("Lines", &srs, wkbUnknown, nullptr);
        if (lineLayer) {
            OGRFieldDefn nameField("Name", OFTString);
            if (lineLayer->CreateField(&nameField) != OGRERR_NONE) {
                qWarning() << "Failed to create Name field";
            }
            
            for (int i = 0; i < polylines.size(); ++i) {
                const auto& poly = polylines[i];
                
                OGRFeature* feature = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
                feature->SetField("Name", QString("Feature_%1").arg(i).toUtf8().constData());
                
                if (poly.closed) {
                    OGRLinearRing ring;
                    for (const auto& pt : poly.points) {
                        ring.addPoint(pt.x(), pt.y());
                    }
                    ring.closeRings();
                    OGRPolygon polygon;
                    polygon.addRing(&ring);
                    feature->SetGeometry(&polygon);
                } else {
                    OGRLineString line;
                    for (const auto& pt : poly.points) {
                        line.addPoint(pt.x(), pt.y());
                    }
                    feature->SetGeometry(&line);
                }
                
                if (lineLayer->CreateFeature(feature) != OGRERR_NONE) {
                    qWarning() << "Failed to create feature";
                }
                OGRFeature::DestroyFeature(feature);
            }
        }
    }
    
    GDALClose(dataset);
    return true;
}
