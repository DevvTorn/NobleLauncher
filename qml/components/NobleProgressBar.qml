import QtQuick

Item {
    id: root
    property real value: 0
    property bool indeterminate: false
    property color fillColor: App.colorAccent
    property color trackColor: App.colorSurfaceAlt
    property int thickness: 6
    implicitHeight: thickness
    implicitWidth: 200
    clip: true

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.trackColor
        border.width: 1
        border.color: App.colorBorder
    }

    Rectangle {
        visible: !root.indeterminate
        width: Math.max(0, Math.min(1, root.value)) * parent.width
        height: parent.height
        radius: height / 2
        color: root.fillColor
        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: runner
        visible: root.indeterminate
        width: parent.width * 0.32
        height: parent.height
        radius: height / 2
        color: root.fillColor
        x: -width
        NumberAnimation on x {
            running: root.indeterminate && App.animationsEnabled
            loops: Animation.Infinite
            from: -runner.width
            to: root.width
            duration: 1100
        }
    }
}
