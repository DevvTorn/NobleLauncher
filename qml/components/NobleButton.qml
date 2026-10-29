import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control
    property string variant: "primary"
    property string iconName: ""
    property bool busy: false
    property int minHeight: 40
    property int fontSize: 14

    implicitHeight: minHeight
    implicitWidth: variant === "icon" ? minHeight : Math.max(76, contentRow.implicitWidth + 26)
    padding: 0
    spacing: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: text.length > 0 ? text : iconName
    opacity: enabled ? 1 : 0.42
    scale: pressed && enabled ? 0.975 : 1
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

    readonly property color accent: App.colorAccent
    readonly property color textColor: {
        if (!enabled)
            return App.colorTextDim
        if (variant === "primary")
            return "#ffffff"
        if (variant === "danger")
            return "#ffffff"
        if (variant === "ghost" || variant === "icon")
            return hovered ? App.colorAccent : App.colorText
        return App.colorText
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: 8
        anchors.centerIn: parent

        Icon {
            visible: control.iconName.length > 0
            name: control.iconName
            size: 18
            color: control.textColor
        }

        Text {
            visible: control.text.length > 0 && control.variant !== "icon"
            text: control.text
            color: control.textColor
            textFormat: Text.PlainText
            font.family: App.fontFamily
            font.pixelSize: App.fs(control.fontSize)
            font.weight: control.variant === "primary" ? Font.DemiBold : Font.Medium
            verticalAlignment: Text.AlignVCenter
        }

        BusyIndicator {
            visible: control.busy
            running: control.busy
            implicitWidth: 18
            implicitHeight: 18
            palette.dark: control.variant === "primary" ? "#ffffff" : App.colorAccent
        }
    }

    background: Rectangle {
        radius: 10
        color: {
            if (control.variant === "primary")
                return control.down ? Qt.darker(control.accent, 1.25) : (control.hovered ? App.colorAccentHover : control.accent)
            if (control.variant === "danger")
                return control.down ? Qt.darker(App.colorError, 1.2) : (control.hovered ? Qt.lighter(App.colorError, 1.12) : App.colorError)
            if (control.variant === "secondary")
                return control.hovered ? App.colorSurfaceAlt : "transparent"
            if (control.variant === "ghost" || control.variant === "icon")
                return control.down ? App.colorAccentSoft : (control.hovered ? App.colorAccentSoft : "transparent")
            return App.colorSurfaceAlt
        }
        border.width: control.variant === "secondary" ? 1 : (control.activeFocus ? 2 : 0)
        border.color: control.activeFocus ? App.colorAccent : App.colorBorder
        Behavior on color { ColorAnimation { duration: 130 } }
    }
}
