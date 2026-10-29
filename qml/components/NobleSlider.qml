import QtQuick
import QtQuick.Controls

Slider {
    id: control
    implicitHeight: 28
    hoverEnabled: true

    background: Rectangle {
        x: 0
        y: (control.height - height) / 2
        width: control.availableWidth
        height: 5
        radius: 3
        color: App.colorSurfaceAlt
        border.width: 1
        border.color: App.colorBorder

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: App.colorAccent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: (control.height - height) / 2
        width: control.pressed ? 20 : 16
        height: width
        radius: width / 2
        color: control.hovered || control.pressed ? "#ffffff" : App.colorText
        border.width: 3
        border.color: App.colorAccent
        Behavior on width { NumberAnimation { duration: 90 } }
    }
}
