import QtQuick
import QtQuick.Controls

Switch {
    id: control
    implicitHeight: 32
    implicitWidth: Math.max(52, contentRow.implicitWidth + 52)
    font.family: App.fontFamily
    font.pixelSize: App.fs(14)
    hoverEnabled: true

    indicator: Rectangle {
        x: control.text.length > 0 ? control.width - width : 0
        y: (control.height - height) / 2
        width: 42
        height: 24
        radius: 12
        color: control.checked ? App.colorAccent : (control.hovered ? App.colorBorder : App.colorSurfaceAlt)
        border.width: 1
        border.color: control.checked ? App.colorAccent : App.colorBorder
        Behavior on color { ColorAnimation { duration: 150 } }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: control.checked ? "#ffffff" : App.colorTextDim
            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Row {
        id: contentRow
        spacing: 8
        Text {
            text: control.text
            color: App.colorText
            font.family: App.fontFamily
            font.pixelSize: App.fs(14)
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: parent.verticalCenter
            visible: control.text.length > 0
        }
    }
}
