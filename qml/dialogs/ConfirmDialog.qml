import QtQuick
import QtQuick.Layouts
import "../components"

NobleDialog {
    id: control
    property string message: ""
    property string acceptLabel: "OK"
    property var acceptCallback: null

    title: control.dialogTitle
    property string dialogTitle: "Подтверждение"
    acceptText: acceptLabel
    rejectText: "Отмена"
    width: 440

    Text {
        width: parent.width
        text: control.message
        color: App.colorTextDim
        font.family: App.fontFamily
        font.pixelSize: App.fs(12)
        wrapMode: Text.WordWrap
    }

    onAccepted: {
        if (control.acceptCallback)
            control.acceptCallback()
    }
}
