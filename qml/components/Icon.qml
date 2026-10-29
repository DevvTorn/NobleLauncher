import QtQuick
import QtQuick.Effects

Item {
    id: root
    property string name: ""
    property color color: App.colorText
    property int size: 20
    implicitWidth: size
    implicitHeight: size

    Image {
        id: image
        anchors.fill: parent
        source: root.name ? "qrc:/assets/icons/" + root.name + ".svg" : ""
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: image
        colorizationColor: root.color
        colorization: 1.0
    }
}
