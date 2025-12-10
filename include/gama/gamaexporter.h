#ifndef GAMAEXPORTER_H
#define GAMAEXPORTER_H

#include <QString>
#include <QVector>
#include <QPointF>

struct GamaPoint {
    QString id;
    double x{0.0};
    double y{0.0};
    double z{0.0};
    bool fixX{false};
    bool fixY{false};
    bool fixZ{false};
    bool adjust{true};
};

struct GamaDistance {
    QString from;
    QString to;
    double value{0.0};
    double stddev{0.001};  // default 1mm
};

struct GamaDirection {
    QString from;
    QString to;
    double value{0.0};     // degrees
    double stddev{0.0003}; // default ~1" in radians
};

struct GamaAngle {
    QString from;
    QString left;
    QString right;
    double value{0.0};     // degrees
    double stddev{0.0003};
};

struct GamaNetwork {
    QString description{"Survey Network"};
    double sigmaApr{10.0};
    double confidenceLevel{0.95};
    QVector<GamaPoint> points;
    QVector<GamaDistance> distances;
    QVector<GamaDirection> directions;
    QVector<GamaAngle> angles;
};

class GamaExporter
{
public:
    GamaExporter();
    ~GamaExporter() = default;
    
    // Set network data
    void setNetwork(const GamaNetwork& network);
    
    // Export to GAMA XML format
    bool exportToXml(const QString& filePath) const;
    
    // Get XML as string
    QString toXmlString() const;
    
    // Add individual elements
    void addPoint(const GamaPoint& point);
    void addDistance(const GamaDistance& distance);
    void addDirection(const GamaDirection& direction);
    void addAngle(const GamaAngle& angle);
    
    // Clear all data
    void clear();
    
private:
    QString formatAngle(double degrees) const;
    
    GamaNetwork m_network;
};

#endif // GAMAEXPORTER_H
