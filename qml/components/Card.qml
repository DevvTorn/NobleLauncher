import QtQuick

Rectangle {
    id: root
    property int padding: Math.round(16 * App.spacingScale)
    property bool interactive: false
    property color fillColor: App.colorSurface
    default property alias contentData: contentItem.data

    implicitWidth: contentItem.implicitWidth + padding * 2
    implicitHeight: contentItem.implicitHeight + padding * 2
    radius: 14
    color: fillColor
    border.width: 1
    border.color: hover.hovered && interactive
                  ? Qt.rgba(App.colorAccent.r, App.colorAccent.g, App.colorAccent.b, 0.55)
                  : App.colorBorder
    Behavior on border.color { ColorAnimation { duration: 140 } }

    HoverHandler { id: hover }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: root.padding
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
}
