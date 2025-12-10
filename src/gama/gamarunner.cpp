#include "gama/gamarunner.h"

#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDir>

GamaRunner::GamaRunner()
{
}

void GamaRunner::setExecutablePath(const QString& path)
{
    m_executablePath = path;
}

bool GamaRunner::isAvailable() const
{
    QProcess process;
    process.start(m_executablePath, {"--version"});
    if (!process.waitForStarted(3000)) {
        return false;
    }
    process.waitForFinished(5000);
    return process.exitCode() == 0 || process.exitCode() == 1;
}

QString GamaRunner::version() const
{
    QProcess process;
    process.start(m_executablePath, {"--version"});
    if (!process.waitForStarted(3000)) {
        return QString();
    }
    process.waitForFinished(5000);
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

GamaAdjustmentResults GamaRunner::runAdjustmentFromString(const QString& xmlContent)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        GamaAdjustmentResults results;
        results.success = false;
        results.errorMessage = "Failed to create temporary file";
        return results;
    }
    
    QTextStream stream(&tempFile);
    stream << xmlContent;
    tempFile.close();
    
    return runAdjustment(tempFile.fileName());
}

GamaAdjustmentResults GamaRunner::runAdjustment(const QString& inputXmlPath)
{
    GamaAdjustmentResults results;
    results.success = false;
    
    if (!QFile::exists(inputXmlPath)) {
        results.errorMessage = "Input XML file not found";
        m_lastError = results.errorMessage;
        return results;
    }
    
    // Create output file path
    QString outputXmlPath = inputXmlPath + ".adj.xml";
    
    QProcess process;
    QStringList args;
    args << inputXmlPath;
    args << "--xml" << outputXmlPath;
    args << "--text" << (inputXmlPath + ".txt");
    
    process.start(m_executablePath, args);
    
    if (!process.waitForStarted(5000)) {
        results.errorMessage = "Failed to start gama-local. Is it installed?";
        m_lastError = results.errorMessage;
        return results;
    }
    
    if (!process.waitForFinished(60000)) {
        results.errorMessage = "gama-local timed out";
        m_lastError = results.errorMessage;
        process.kill();
        return results;
    }
    
    results.rawOutput = QString::fromUtf8(process.readAllStandardOutput());
    QString errorOutput = QString::fromUtf8(process.readAllStandardError());
    
    if (process.exitCode() != 0) {
        results.errorMessage = QString("gama-local failed with exit code %1: %2")
            .arg(process.exitCode())
            .arg(errorOutput);
        m_lastError = results.errorMessage;
        return results;
    }
    
    // Parse the text output for statistics
    QString textPath = inputXmlPath + ".txt";
    if (QFile::exists(textPath)) {
        QFile textFile(textPath);
        if (textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            parseTextOutput(QString::fromUtf8(textFile.readAll()), results);
            textFile.close();
        }
    }
    
    // Parse the XML output for adjusted coordinates
    if (QFile::exists(outputXmlPath)) {
        QFile xmlFile(outputXmlPath);
        if (xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            results.rawXmlOutput = QString::fromUtf8(xmlFile.readAll());
            xmlFile.close();
        }
        parseOutputXml(outputXmlPath, results);
    }
    
    results.success = true;
    return results;
}

bool GamaRunner::parseTextOutput(const QString& output, GamaAdjustmentResults& results)
{
    // Parse degrees of freedom
    QRegularExpression dofRegex(R"(Degrees of freedom\s*:\s*(\d+))");
    auto dofMatch = dofRegex.match(output);
    if (dofMatch.hasMatch()) {
        results.degreesOfFreedom = dofMatch.captured(1).toDouble();
    }
    
    // Parse m0 (sigma0)
    QRegularExpression m0Regex(R"(m0\s*=\s*([0-9.]+))");
    auto m0Match = m0Regex.match(output);
    if (m0Match.hasMatch()) {
        results.sigma0 = m0Match.captured(1).toDouble();
    }
    
    // Chi-square test
    results.chiSquarePassed = output.contains("passed") && output.contains("chi-square");
    
    return true;
}

bool GamaRunner::parseOutputXml(const QString& xmlPath, GamaAdjustmentResults& results)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QLatin1String("point")) {
                GamaAdjustedPoint pt;
                
                for (const auto& attr : xml.attributes()) {
                    if (attr.name() == QLatin1String("id")) {
                        pt.id = attr.value().toString();
                    } else if (attr.name() == QLatin1String("x")) {
                        pt.x = attr.value().toDouble();
                    } else if (attr.name() == QLatin1String("y")) {
                        pt.y = attr.value().toDouble();
                    } else if (attr.name() == QLatin1String("z")) {
                        pt.z = attr.value().toDouble();
                    }
                }
                
                // Read child elements for standard deviations
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && 
                         xml.name() == QLatin1String("point"))) {
                    xml.readNext();
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == QLatin1String("x")) {
                            xml.readNext();
                            pt.x = xml.text().toDouble();
                        } else if (xml.name() == QLatin1String("y")) {
                            xml.readNext();
                            pt.y = xml.text().toDouble();
                        }
                    }
                }
                
                if (!pt.id.isEmpty()) {
                    results.adjustedPoints.append(pt);
                }
            }
            
            // Parse coordinates section
            if (xml.name() == QLatin1String("coordinates")) {
                // Continue parsing within coordinates
            }
        }
    }
    
    file.close();
    return !xml.hasError();
}
