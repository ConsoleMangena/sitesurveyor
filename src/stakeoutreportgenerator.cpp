#include "stakeoutreportgenerator.h"

#include <QTextStream>

namespace {
QString htmlEscape(const QString& input)
{
    QString escaped = input;
    escaped.replace('&', "&amp;");
    escaped.replace('<', "&lt;");
    escaped.replace('>', "&gt;");
    escaped.replace('"', "&quot;");
    escaped.replace('\'', "&#39;");
    return escaped;
}
}

QString StakeoutReportGenerator::generateHtml(const QVector<StakeoutRecord>& records)
{
    QString html;
    QTextStream stream(&html);
    stream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    stream << "<title>Stakeout Report</title>";
    stream << "<style>body{font-family:sans-serif;margin:24px;}table{border-collapse:collapse;width:100%;}"
              "th,td{border:1px solid #ccc;padding:6px;text-align:left;}th{background:#f2f2f2;}"
              ".pending{color:#888;} .fail{color:#c0392b;font-weight:bold;}";
    stream << "</style></head><body>";
    stream << "<h1>Stakeout Report</h1>";
    stream << "<p>Total records: " << records.size() << "</p>";
    stream << "<table><thead><tr>"
              "<th>Status</th><th>Point</th><th>Description</th>"
              "<th>Design E</th><th>Design N</th><th>Design Z</th>"
              "<th>Measured E</th><th>Measured N</th><th>Measured Z</th>"
              "<th>ΔE</th><th>ΔN</th><th>ΔZ</th>"
              "<th>Horizontal</th><th>Vertical</th><th>Remarks</th>"
              "</tr></thead><tbody>";
    for (const StakeoutRecord& record : records) {
        const bool hasMeasurement = record.hasMeasurement();
        const QString statusClass = hasMeasurement ? QString() : QStringLiteral("pending");
        stream << "<tr class='" << statusClass << "'>";
        stream << "<td>" << htmlEscape(record.status.isEmpty() ? QStringLiteral("Planned") : record.status) << "</td>";
        stream << "<td>" << htmlEscape(record.designPoint) << "</td>";
        stream << "<td>" << htmlEscape(record.description) << "</td>";
        stream << "<td>" << QString::number(record.designE, 'f', 3) << "</td>";
        stream << "<td>" << QString::number(record.designN, 'f', 3) << "</td>";
        stream << "<td>" << QString::number(record.designZ, 'f', 3) << "</td>";
        if (hasMeasurement) {
            stream << "<td>" << QString::number(record.measuredE, 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.measuredN, 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.measuredZ, 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.deltaE(), 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.deltaN(), 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.deltaZ(), 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.horizontalResidual(), 'f', 3) << "</td>";
            stream << "<td>" << QString::number(record.verticalResidual(), 'f', 3) << "</td>";
        } else {
            for (int i = 0; i < 8; ++i) {
                stream << "<td class='pending'>–</td>";
            }
        }
        stream << "<td>" << htmlEscape(record.remarks) << "</td>";
        stream << "</tr>";
    }
    stream << "</tbody></table>";
    stream << "</body></html>";
    return html;
}
