import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: control
    property string title: ""
    property string acceptText: qsTr("Сохранить")
    property string rejectText: qsTr("Отмена")
    property bool showFooter: true
    property bool showAccept: true
    property bool showReject: true
    property bool acceptEnabled: true
    default property alias contentData: body.data

    signal accepted()
    signal rejected()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 80 : 560)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    Overlay.modal: Rectangle { color: "#a6000000" }

    background: Rectangle {
        radius: 16
        color: App.colorSurface
        border.width: 1
        border.color: App.colorBorder
        layer.enabled: true
    }

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
            implicitHeight: 56
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                Text {
                    Layout.fillWidth: true
                    text: control.title
                    color: App.colorText
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(17)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                NobleButton {
                    variant: "icon"
                    iconName: "close"
                    minHeight: 32
                    implicitWidth: 32
                    onClicked: control.reject()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: App.colorBorder
        }

        Item {
            id: body
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            implicitHeight: childrenRect.height
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: App.colorBorder
            visible: control.showFooter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 10
            visible: control.showFooter
            Item { Layout.fillWidth: true }
            NobleButton {
                visible: control.showReject && control.rejectText.length > 0
                text: control.rejectText
                variant: "secondary"
                onClicked: control.reject()
            }
            NobleButton {
                visible: control.showAccept && control.acceptText.length > 0
                text: control.acceptText
                variant: "primary"
                enabled: control.acceptEnabled
                onClicked: control.accepted()
            }
        }
    }

    function accept() { accepted(); close(); }
    function reject() { rejected(); close(); }
}
