import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    implicitHeight: 30
    font.family: App.fontFamily
    font.pixelSize: App.fs(14)
    hoverEnabled: true

    indicator: Rectangle {
        width: 20
        height: 20
        radius: 6
        color: control.checked ? App.colorAccent : (control.hovered ? App.colorSurfaceAlt : "transparent")
        border.width: 1
        border.color: control.checked ? App.colorAccent : App.colorBorder
        Behavior on color { ColorAnimation { duration: 120 } }
        Icon {
            anchors.centerIn: parent
            name: "check"
            size: 14
            color: "#ffffff"
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        color: App.colorText
        font.family: App.fontFamily
        font.pixelSize: App.fs(14)
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + 8
    }
}
