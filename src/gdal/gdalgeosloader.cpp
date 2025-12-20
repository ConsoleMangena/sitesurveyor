#include "gdal/gdalgeosloader.h"
#include "dxf/dxfreader.h"

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <geos_c.h>

#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <cstring>

// Color palette for layers
static const QColor s_layerColors[] = {
    QColor(255, 255, 255),  // White (default)
    QColor(255, 0, 0),      // Red
    QColor(255, 255, 0),    // Yellow
    QColor(0, 255, 0),      // Green
    QColor(0, 255, 255),    // Cyan
    QColor(0, 0, 255),      // Blue
    QColor(255, 0, 255),    // Magenta
    QColor(255, 128, 0),    // Orange
    QColor(128, 0, 255),    // Purple
    QColor(0, 128, 255),    // Light Blue
};
static const int s_numColors = sizeof(s_layerColors) / sizeof(s_layerColors[0]);

// GEOS error handler
static void geosErrorHandler(const char* message, void* /*userdata*/) {
    qDebug() << "GEOS Error:" << message;
}

static void geosNoticeHandler(const char* /*message*/, void* /*userdata*/) {
    // Ignore notices (e.g. "Self-intersection") - do not print
}

GdalGeosLoader::GdalGeosLoader()
{
    // Initialize GEOS context (thread-safe)
    m_geosContext = GEOS_init_r();
    if (m_geosContext) {
        GEOSContext_setErrorMessageHandler_r(static_cast<GEOSContextHandle_t>(m_geosContext), 
                                             geosErrorHandler, nullptr);
        GEOSContext_setNoticeMessageHandler_r(static_cast<GEOSContextHandle_t>(m_geosContext), 
                                              geosNoticeHandler, nullptr);
    }
}

GdalGeosLoader::~GdalGeosLoader()
{
    if (m_geosContext) {
        GEOS_finish_r(static_cast<GEOSContextHandle_t>(m_geosContext));
    }
}

bool GdalGeosLoader::loadDxf(const QString& filepath, DxfData& targetData)
{
    targetData.clear();
    m_lastError.clear();
    m_geometriesProcessed = 0;
    m_geometriesRepaired = 0;
    m_geometriesFailed = 0;
    m_issueLog.clear();
    
    if (!m_geosContext) {
        m_lastError = "GEOS context initialization failed";
        return false;
    }
    
    // Configure GDAL DXF driver options
    // DXF_INLINE_BLOCKS=TRUE - Explodes blocks into simple primitives
    // DXF_MERGE_BLOCK_GEOMETRIES=FALSE - Keep geometries separate
    CPLSetConfigOption("DXF_INLINE_BLOCKS", "TRUE");
    CPLSetConfigOption("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE");
    
    // Suppress OGR warnings (Non closed ring, etc)
    CPLPushErrorHandler(CPLQuietErrorHandler);
    
    // Open the DXF file with GDAL
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(filepath.toUtf8().constData(), 
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr));
    
    if (!dataset) {
        m_lastError = QString("Failed to open DXF file: %1").arg(CPLGetLastErrorMsg());
        return false;
    }
    
    // Track layer colors
    QMap<QString, QColor> layerColors;
    int layerIndex = 0;
    
    // Iterate through all layers
    int layerCount = dataset->GetLayerCount();
    for (int i = 0; i < layerCount; ++i) {
        OGRLayer* layer = dataset->GetLayer(i);
        if (!layer) continue;
        
        QString layerName = QString::fromUtf8(layer->GetName());
        
        // Assign a color to this layer if not already assigned
        // Assign a color to this layer if not already assigned
        if (!layerColors.contains(layerName)) {
            layerColors[layerName] = getLayerColor(layerIndex++);
        }
        
        // Process all features in this layer
        layer->ResetReading();
        OGRFeature* feature;
        while ((feature = layer->GetNextFeature()) != nullptr) {
            processFeature(feature, layerName, targetData);
            OGRFeature::DestroyFeature(feature);
        }
    }
    
    // Cleanup
    GDALClose(dataset);
    
    // Reset config options
    CPLSetConfigOption("DXF_INLINE_BLOCKS", nullptr);
    CPLSetConfigOption("DXF_MERGE_BLOCK_GEOMETRIES", nullptr);
    
    // Restore error handler
    CPLPopErrorHandler();
    
    return !targetData.isEmpty();
}

void GdalGeosLoader::processFeature(void* featurePtr, const QString& layerName, DxfData& targetData)
{
    OGRFeature* feature = static_cast<OGRFeature*>(featurePtr);
    OGRGeometry* ogrGeom = feature->GetGeometryRef();
    if (!ogrGeom) return;
    
    // Get color from feature (AutoCAD Color Index)
    QColor color = getLayerColor(0);  // Default white
    int colorIndex = feature->GetFieldIndex("Color");
    if (colorIndex >= 0 && feature->IsFieldSet(colorIndex)) {
        int aci = feature->GetFieldAsInteger(colorIndex);
        if (aci > 0 && aci != 256) { // 256 is ByLayer
            color = aciToColor(aci);
        }
    }
    
    // Get layer name from feature if available
    QString featureLayer = layerName;
    int layerFieldIndex = feature->GetFieldIndex("Layer");
    if (layerFieldIndex >= 0 && feature->IsFieldSet(layerFieldIndex)) {
        featureLayer = QString::fromUtf8(feature->GetFieldAsString(layerFieldIndex));
    }
    
    // Ensure layer exists in targetData
    bool layerFound = false;
    for (const auto& layer : targetData.layers) {
        if (layer.name == featureLayer) {
            layerFound = true;
            break;
        }
    }
    
    if (!layerFound) {
        DxfLayer newLayer;
        newLayer.name = featureLayer;
        // If color is ByLayer (256), we should use a default or random color for the layer
        // If color is explicit, we might still want a distinct color for the layer
        // For now, let's generate a color based on layer name hash or just cycle
        newLayer.color = getLayerColor(qHash(featureLayer)); 
        newLayer.visible = true;
        newLayer.locked = false;
        targetData.layers.append(newLayer);
    }
    
    // Get text properties if this is a text entity
    QString textValue;
    double textHeight = 2.5;  // Default
    double textAngle = 0.0;
    
    int textIndex = feature->GetFieldIndex("Text");
    if (textIndex >= 0 && feature->IsFieldSet(textIndex)) {
        textValue = QString::fromUtf8(feature->GetFieldAsString(textIndex));
    }
    
    // Extract text height from feature field (fallback)
    int heightIndex = feature->GetFieldIndex("Height");
    if (heightIndex >= 0 && feature->IsFieldSet(heightIndex)) {
        textHeight = feature->GetFieldAsDouble(heightIndex);
    }
    
    // Extract text angle from feature field (fallback)
    int angleIndex = feature->GetFieldIndex("Angle");
    if (angleIndex >= 0 && feature->IsFieldSet(angleIndex)) {
        textAngle = feature->GetFieldAsDouble(angleIndex);
    }
    
    // Extract text properties from OGR Style string (primary method)
    // GDAL DXF driver stores text info in style like: LABEL(f:"Arial",t:"TEXT",s:2.5g,a:45)
    const char* styleStr = feature->GetStyleString();
    if (styleStr && !textValue.isEmpty()) {
        QString style = QString::fromUtf8(styleStr);
        
        // Parse size from style: s:2.5g or s:2.5
        QRegularExpression sizeRe("s:([0-9.]+)");
        QRegularExpressionMatch sizeMatch = sizeRe.match(style);
        if (sizeMatch.hasMatch()) {
            textHeight = sizeMatch.captured(1).toDouble();
        }
        
        // Parse angle from style: a:45
        QRegularExpression angleRe("a:([0-9.-]+)");
        QRegularExpressionMatch angleMatch = angleRe.match(style);
        if (angleMatch.hasMatch()) {
            textAngle = angleMatch.captured(1).toDouble();
        }
    }
    
    m_geometriesProcessed++;
    
    // --- GEOS Validation via WKB ---
    GEOSContextHandle_t ctx = static_cast<GEOSContextHandle_t>(m_geosContext);
    
    // Get WKB from OGR
    int wkbSize = ogrGeom->WkbSize();
    std::vector<unsigned char> wkbBuffer(wkbSize);
    OGRErr err = ogrGeom->exportToWkb(OGRwkbByteOrder::wkbNDR, wkbBuffer.data());
    if (err != OGRERR_NONE) {
        m_geometriesFailed++;
        return;
    }
    
    // Import into GEOS
    GEOSGeometry* geosGeom = GEOSGeomFromWKB_buf_r(ctx, wkbBuffer.data(), wkbSize);
    if (!geosGeom) {
        // WKB import failed, extract directly from OGR
        extractOgrGeometry(ogrGeom, featureLayer, color, textValue, textHeight, textAngle, targetData);
        return;
    }
    
    // Validate with GEOS
    char isValid = GEOSisValid_r(ctx, geosGeom);
    if (isValid != 1) {
        // Get the reason for invalidity
        char* reason = GEOSisValidReason_r(ctx, geosGeom);
        QString issueReason = reason ? QString::fromUtf8(reason) : "Unknown";
        if (reason) GEOSFree_r(ctx, reason);
        
        // Disable Auto-fix as per user request
        // Log it and load original geometry as-is
        m_geometriesFailed++; // We count it as failed validation, but we still load it
        m_issueLog.append(QString("[INVALID] Layer '%1': %2").arg(featureLayer, issueReason));
        
        GEOSGeom_destroy_r(ctx, geosGeom);
        
        // Fallback to direct OGR extraction (loads as-is)
        extractOgrGeometry(ogrGeom, featureLayer, color, textValue, textHeight, textAngle, targetData);
        return;
    }
    
    // Export validated geometry back to WKB
    size_t validWkbSize = 0;
    unsigned char* validWkb = GEOSGeomToWKB_buf_r(ctx, geosGeom, &validWkbSize);
    GEOSGeom_destroy_r(ctx, geosGeom);
    
    if (validWkb && validWkbSize > 0) {
        // Import WKB back into OGR
        OGRGeometry* validOgrGeom = nullptr;
        OGRErr importErr = OGRGeometryFactory::createFromWkb(validWkb, nullptr, &validOgrGeom, validWkbSize);
        GEOSFree_r(ctx, validWkb);
        
        if (importErr == OGRERR_NONE && validOgrGeom) {
            extractOgrGeometry(validOgrGeom, featureLayer, color, textValue, textHeight, textAngle, targetData);
            OGRGeometryFactory::destroyGeometry(validOgrGeom);
        } else {
            extractOgrGeometry(ogrGeom, featureLayer, color, textValue, textHeight, textAngle, targetData);
        }
    } else {
        extractOgrGeometry(ogrGeom, featureLayer, color, textValue, textHeight, textAngle, targetData);
    }
}

void GdalGeosLoader::extractOgrGeometry(void* geomPtr, const QString& layer, 
                                        const QColor& color, const QString& textValue,
                                        double textHeight, double textAngle,
                                        DxfData& targetData)
{
    OGRGeometry* geom = static_cast<OGRGeometry*>(geomPtr);
    if (!geom) return;
    
    OGRwkbGeometryType geomType = wkbFlatten(geom->getGeometryType());
    
    switch (geomType) {
        case wkbPoint: {
            OGRPoint* pt = static_cast<OGRPoint*>(geom);
            if (!textValue.isEmpty()) {
                DxfText text;
                text.text = textValue;
                text.position = QPointF(pt->getX(), pt->getY());
                text.height = textHeight;
                text.angle = textAngle;
                text.layer = layer;
                text.color = color;
                targetData.texts.append(text);
            }
            break;
        }
        
        case wkbLineString: {
            OGRLineString* ls = static_cast<OGRLineString*>(geom);
            if (ls->getNumPoints() >= 2) {
                DxfPolyline poly;
                poly.layer = layer;
                poly.color = color;
                poly.closed = ls->get_IsClosed();
                for (int i = 0; i < ls->getNumPoints(); ++i) {
                    poly.points.append(QPointF(ls->getX(i), ls->getY(i)));
                }
                targetData.polylines.append(poly);
            }
            break;
        }
        
        case wkbPolygon: {
            OGRPolygon* polygon = static_cast<OGRPolygon*>(geom);
            
            // Exterior ring
            OGRLinearRing* extRing = polygon->getExteriorRing();
            if (extRing && extRing->getNumPoints() >= 3) {
                DxfPolyline poly;
                poly.layer = layer;
                poly.color = color;
                poly.closed = true;
                for (int i = 0; i < extRing->getNumPoints(); ++i) {
                    poly.points.append(QPointF(extRing->getX(i), extRing->getY(i)));
                }
                targetData.polylines.append(poly);
            }
            
            // Interior rings (holes)
            for (int r = 0; r < polygon->getNumInteriorRings(); ++r) {
                OGRLinearRing* intRing = polygon->getInteriorRing(r);
                if (intRing && intRing->getNumPoints() >= 3) {
                    DxfPolyline poly;
                    poly.layer = layer;
                    poly.color = color;
                    poly.closed = true;
                    for (int i = 0; i < intRing->getNumPoints(); ++i) {
                        poly.points.append(QPointF(intRing->getX(i), intRing->getY(i)));
                    }
                    targetData.polylines.append(poly);
                }
            }
            break;
        }
        
        case wkbMultiPoint: {
            OGRMultiPoint* mp = static_cast<OGRMultiPoint*>(geom);
            for (int i = 0; i < mp->getNumGeometries(); ++i) {
                extractOgrGeometry(mp->getGeometryRef(i), layer, color, textValue, textHeight, textAngle, targetData);
            }
            break;
        }
        
        case wkbMultiLineString: {
            OGRMultiLineString* mls = static_cast<OGRMultiLineString*>(geom);
            for (int i = 0; i < mls->getNumGeometries(); ++i) {
                extractOgrGeometry(mls->getGeometryRef(i), layer, color, "", 2.5, 0.0, targetData);
            }
            break;
        }
        
        case wkbMultiPolygon: {
            OGRMultiPolygon* mpoly = static_cast<OGRMultiPolygon*>(geom);
            for (int i = 0; i < mpoly->getNumGeometries(); ++i) {
                extractOgrGeometry(mpoly->getGeometryRef(i), layer, color, "", 2.5, 0.0, targetData);
            }
            break;
        }
        
        case wkbGeometryCollection: {
            OGRGeometryCollection* gc = static_cast<OGRGeometryCollection*>(geom);
            for (int i = 0; i < gc->getNumGeometries(); ++i) {
                extractOgrGeometry(gc->getGeometryRef(i), layer, color, "", 2.5, 0.0, targetData);
            }
            break;
        }
        
        case wkbCircularString:
        case wkbCompoundCurve:
        case wkbCurvePolygon: {
            // Linearize curves
            OGRGeometry* linearized = geom->getLinearGeometry();
            if (linearized) {
                extractOgrGeometry(linearized, layer, color, textValue, textHeight, textAngle, targetData);
                OGRGeometryFactory::destroyGeometry(linearized);
            }
            break;
        }
        
        default:
            qDebug() << "Unhandled geometry type:" << geom->getGeometryName();
            break;
    }
}

QColor GdalGeosLoader::getLayerColor(int index)
{
    return s_layerColors[index % s_numColors];
}
