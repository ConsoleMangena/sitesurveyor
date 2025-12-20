#include "gama/gamaexporter.h"

#include <QFile>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <QtMath>
#include <QMap>
#include <QSet>

GamaExporter::GamaExporter()
{
}

void GamaExporter::setNetwork(const GamaNetwork& network)
{
    m_network = network;
}

void GamaExporter::addPoint(const GamaPoint& point)
{
    m_network.points.append(point);
}

void GamaExporter::addDistance(const GamaDistance& distance)
{
    m_network.distances.append(distance);
}

void GamaExporter::addDirection(const GamaDirection& direction)
{
    m_network.directions.append(direction);
}

void GamaExporter::addAngle(const GamaAngle& angle)
{
    m_network.angles.append(angle);
}

void GamaExporter::clear()
{
    m_network = GamaNetwork();
}

QString GamaExporter::formatAngle(double degrees) const
{
    // Convert decimal degrees to DMS format: "DDD-MM-SS.ss"
    double absVal = qAbs(degrees);
    int d = static_cast<int>(absVal);
    double minFloat = (absVal - d) * 60.0;
    int m = static_cast<int>(minFloat);
    double s = (minFloat - m) * 60.0;
    
    QString result = QString("%1-%2-%3")
        .arg(d)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 5, 'f', 2, QChar('0'));
    
    if (degrees < 0) {
        result = "-" + result;
    }
    
    return result;
}

QString GamaExporter::toXmlString() const
{
    QString output;
    QXmlStreamWriter xml(&output);
    
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    
    xml.writeStartDocument();
    
    // Root element
    xml.writeStartElement("gama-local");
    xml.writeAttribute("version", "2.0");
    
    // Network element
    xml.writeStartElement("network");
    xml.writeAttribute("axes-xy", "sw");  // South-West convention
    xml.writeAttribute("angles", "left-handed");
    
    // Description
    xml.writeTextElement("description", m_network.description);
    
    // Parameters
    xml.writeStartElement("parameters");
    xml.writeAttribute("sigma-apr", QString::number(m_network.sigmaApr));
    xml.writeAttribute("conf-pr", QString::number(m_network.confidenceLevel));
    xml.writeEndElement(); // parameters
    
    // Points and observations
    xml.writeStartElement("points-observations");
    
    // Write points
    for (const auto& pt : m_network.points) {
        xml.writeStartElement("point");
        xml.writeAttribute("id", pt.id);
        xml.writeAttribute("x", QString::number(pt.x, 'f', 4));
        xml.writeAttribute("y", QString::number(pt.y, 'f', 4));
        
        if (pt.fixX && pt.fixY) {
            xml.writeAttribute("fix", "xy");
        } else if (pt.fixX) {
            xml.writeAttribute("fix", "x");
        } else if (pt.fixY) {
            xml.writeAttribute("fix", "y");
        }
        
        if (pt.adjust) {
            xml.writeAttribute("adj", "xy");
        }
        
        xml.writeEndElement(); // point
    }
    
    // Group observations by "from" point
    QMap<QString, QVector<int>> distancesByFrom;
    QMap<QString, QVector<int>> directionsByFrom;
    
    for (int i = 0; i < m_network.distances.size(); ++i) {
        distancesByFrom[m_network.distances[i].from].append(i);
    }
    
    for (int i = 0; i < m_network.directions.size(); ++i) {
        directionsByFrom[m_network.directions[i].from].append(i);
    }
    
    // Write observations grouped by from point
    QSet<QString> fromPoints;
    for (const auto& d : m_network.distances) fromPoints.insert(d.from);
    for (const auto& d : m_network.directions) fromPoints.insert(d.from);
    
    for (const QString& fromPt : fromPoints) {
        xml.writeStartElement("obs");
        xml.writeAttribute("from", fromPt);
        
        // Directions from this point
        if (directionsByFrom.contains(fromPt)) {
            for (int idx : directionsByFrom[fromPt]) {
                const auto& dir = m_network.directions[idx];
                xml.writeStartElement("direction");
                xml.writeAttribute("to", dir.to);
                xml.writeAttribute("val", formatAngle(dir.value));
                xml.writeEndElement();
            }
        }
        
        // Distances from this point
        if (distancesByFrom.contains(fromPt)) {
            for (int idx : distancesByFrom[fromPt]) {
                const auto& dist = m_network.distances[idx];
                xml.writeStartElement("distance");
                xml.writeAttribute("to", dist.to);
                xml.writeAttribute("val", QString::number(dist.value, 'f', 4));
                xml.writeEndElement();
            }
        }
        
        xml.writeEndElement(); // obs
    }
    
    // Write angles (standalone)
    for (const auto& angle : m_network.angles) {
        xml.writeStartElement("obs");
        xml.writeAttribute("from", angle.from);
        
        xml.writeStartElement("angle");
        xml.writeAttribute("bs", angle.left);
        xml.writeAttribute("fs", angle.right);
        xml.writeAttribute("val", formatAngle(angle.value));
        xml.writeEndElement(); // angle
        
        xml.writeEndElement(); // obs
    }
    
    xml.writeEndElement(); // points-observations
    xml.writeEndElement(); // network
    xml.writeEndElement(); // gama-local
    
    xml.writeEndDocument();
    
    return output;
}

bool GamaExporter::exportToXml(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream << toXmlString();
    file.close();
    
    return true;
}
