import QtQuick
import QtQuick.Controls

TextField {
    id: control
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12
    topPadding: 8
    bottomPadding: 8
    color: App.colorText
    placeholderTextColor: App.colorTextDim
    selectionColor: App.colorAccent
    selectedTextColor: "#ffffff"
    font.family: App.fontFamily
    font.pixelSize: App.fs(14)
    selectByMouse: true

    background: Rectangle {
        radius: 10
        color: control.hovered ? App.colorSurfaceAlt : App.colorSurface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? App.colorAccent : App.colorBorder
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }
}
