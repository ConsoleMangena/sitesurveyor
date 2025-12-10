#ifndef GAMARUNNER_H
#define GAMARUNNER_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QMap>

struct GamaAdjustedPoint {
    QString id;
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double stddevX{0.0};
    double stddevY{0.0};
    double stddevZ{0.0};
};

struct GamaAdjustmentResults {
    bool success{false};
    QString errorMessage;
    
    // Network statistics
    double degreesOfFreedom{0.0};
    double sigma0{0.0};
    double chiSquareTest{0.0};
    bool chiSquarePassed{false};
    
    // Adjusted coordinates
    QVector<GamaAdjustedPoint> adjustedPoints;
    
    // Original output
    QString rawOutput;
    QString rawXmlOutput;
};

class GamaRunner
{
public:
    GamaRunner();
    ~GamaRunner() = default;
    
    // Set path to gama-local executable (default: "gama-local")
    void setExecutablePath(const QString& path);
    QString executablePath() const { return m_executablePath; }
    
    // Check if gama-local is available
    bool isAvailable() const;
    QString version() const;
    
    // Run adjustment
    GamaAdjustmentResults runAdjustment(const QString& inputXmlPath);
    GamaAdjustmentResults runAdjustmentFromString(const QString& xmlContent);
    
    // Get last error
    QString lastError() const { return m_lastError; }
    
private:
    bool parseOutputXml(const QString& xmlPath, GamaAdjustmentResults& results);
    bool parseTextOutput(const QString& output, GamaAdjustmentResults& results);
    
    QString m_executablePath{"gama-local"};
    QString m_lastError;
};

#endif // GAMARUNNER_H
