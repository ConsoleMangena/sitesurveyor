import QtQuick 2.15
import QtLocation 6.0
import QtPositioning 6.0

Item {
    id: window
    
    // Properties to be set from C++
    property double centerLat: 40.785091 // Central Park Default
    property double centerLon: -73.968285
    property int zoomLevel: 16

    Plugin {
        id: mapPlugin
        name: "osm" // OpenStreetMap
        
        // Use standard OSM tile server
        // In production, should configure custom provider or cache
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(centerLat, centerLon)
        zoomLevel: parent.zoomLevel
        
        // Qt6: gesture was replaced by individual handlers
        // Using WheelHandler and DragHandler for interaction
        
        WheelHandler {
            id: wheelHandler
            target: null  // Don't transform the map item itself
            onWheel: function(event) {
                if (event.angleDelta.y > 0)
                    map.zoomLevel = Math.min(map.zoomLevel + 1, 20)
                else
                    map.zoomLevel = Math.max(map.zoomLevel - 1, 1)
            }
        }
        
        // Copyright notice required by OSM
        Text {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 5
            text: "© OpenStreetMap contributors"
            font.pixelSize: 10
            color: "black"
            style: Text.Outline
            styleColor: "white"
        }
    }
    
    // Function to update center from C++
    function setCenter(lat, lon) {
        map.center = QtPositioning.coordinate(lat, lon)
    }
    
    function setZoom(lvl) {
        map.zoomLevel = lvl
    }
}
