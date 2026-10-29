import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

NobleDialog {
    id: control
    property string kind: "instances"
    property string path: ""
    property var info: ({})
    property string mode: "move"

    function t(key) { return I18n.t(App.lang, key) }
    function openForKind(value) {
        kind = value === "runtime" ? "runtime" : "instances"
        path = ""
        info = ({})
        mode = "move"
        open()
        folderDialog.open()
    }
    function inspect() {
        if (path.length > 0)
            App.inspectStorage(control.kind, control.path)
    }

    title: control.t("storage_title")
    acceptText: control.t("storage_confirm")
    rejectText: control.t("cancel")
    acceptEnabled: control.path.length > 0 && !control.info.problem
    width: 540

    onAccepted: App.moveStorage(control.kind, control.path, control.mode)

    Connections {
        target: App
        function onStorageInspected(value) { control.info = value }
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: control.kind === "runtime" ? control.t("disk_runtime") : control.t("disk_instances")
            color: App.colorTextDim
            font.family: App.fontFamily
            font.pixelSize: App.fs(10)
        }
        RowLayout {
            Layout.fillWidth: true
            NobleTextField {
                Layout.fillWidth: true
                text: control.path
                readOnly: true
                placeholderText: "D:\\Games\\NobleLauncher"
            }
            NobleButton {
                variant: "secondary"
                iconName: "folder"
                text: control.t("disk_change")
                minHeight: 38
                fontSize: 10
                onClicked: folderDialog.open()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 18
            rowSpacing: 8

            Text { text: control.t("storage_free"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
            Text { text: control.info.freeBytes !== undefined ? App.formatBytes(control.info.freeBytes) : "—"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }

            Text { text: control.t("storage_data"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
            Text { text: control.info.currentBytes !== undefined ? App.formatBytes(control.info.currentBytes) + " · " + control.info.currentFiles + " файлов" : "—"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }

            Text { text: " "; visible: control.info.sameVolume === true }
            Text {
                visible: control.info.sameVolume === true
                text: control.t("storage_same_volume")
                color: App.colorSuccess
                font.family: App.fontFamily
                font.pixelSize: App.fs(9)
            }
            Text { text: " "; visible: control.info.sameVolume === false }
            Text {
                visible: control.info.sameVolume === false
                text: control.t("storage_cross_volume")
                color: App.colorWarn
                font.family: App.fontFamily
                font.pixelSize: App.fs(9)
            }
        }

        Text {
            Layout.fillWidth: true
            visible: control.info.problem !== undefined && control.info.problem.length > 0
            text: control.t("storage_problem") + ": " + (control.info.problem || "")
            color: App.colorError
            font.family: App.fontFamily
            font.pixelSize: App.fs(10)
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [{ id: "move", key: "storage_mode_move" }, { id: "copy", key: "storage_mode_copy" }]
                delegate: Rectangle {
                    required property var modelData
                    implicitHeight: 34
                    implicitWidth: modeText.implicitWidth + 24
                    radius: 8
                    color: control.mode === modelData.id ? App.colorAccent : "transparent"
                    border.width: 1
                    border.color: control.mode === modelData.id ? App.colorAccent : App.colorBorder
                    Text {
                        id: modeText
                        anchors.centerIn: parent
                        text: control.t(modelData.key)
                        color: control.mode === modelData.id ? "#ffffff" : App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: control.mode = modelData.id }
                }
            }
            Item { Layout.fillWidth: true }
        }
        Text {
            Layout.fillWidth: true
            text: control.mode === "move" ? control.t("storage_mode_move") : control.t("storage_mode_copy")
            color: App.colorTextDim
            font.family: App.fontFamily
            font.pixelSize: App.fs(9)
            wrapMode: Text.WordWrap
        }
        Text {
            visible: App.taskRunning
            Layout.fillWidth: true
            text: App.taskMessage + " · " + Math.round(App.taskProgress * 100) + "%"
            color: App.colorAccent
            font.family: App.fontFamily
            font.pixelSize: App.fs(9)
        }
    }

    FolderDialog {
        id: folderDialog
        title: control.t("storage_target")
        onAccepted: {
            control.path = App.toLocalFile(selectedFolder.toString())
            control.inspect()
        }
    }
}
