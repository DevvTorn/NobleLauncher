import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../I18n.js" as I18n

Rectangle {
    id: root
    property bool opened: false
    property bool wrapLines: true
    property bool autoScroll: true
    property string searchText: ""
    signal requestClose()

    function t(key) { return I18n.t(App.lang, key) }

    visible: opened && App.consoleModel !== null
    height: visible ? Math.min(360, parent ? parent.height * 0.42 : 360) : 0
    color: App.settings.theme === "light" ? "#ffffff" : "#0c0c10"
    border.width: 1
    border.color: App.colorBorder
    radius: 14

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                width: 8; height: 8; radius: 4
                color: App.gamePhase === "running" ? App.colorSuccess
                     : App.gamePhase === "launching" || App.gamePhase === "stopping" ? App.colorWarn : App.colorTextDim
            }
            Text {
                text: root.t("console")
                color: App.colorText
                font.family: App.fontFamily
                font.pixelSize: App.fs(14)
                font.weight: Font.DemiBold
            }
            Text {
                text: App.gamePhase === "running" ? root.t("task_running")
                     : App.gamePhase === "launching" ? root.t("launching")
                     : App.gamePhase === "stopping" ? root.t("stopping") : ""
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(11)
            }

            Item { Layout.fillWidth: true }

            NobleTextField {
                Layout.preferredWidth: Math.min(280, root.width * 0.28)
                implicitHeight: 32
                placeholderText: root.t("console_search")
                onTextChanged: root.searchText = text
            }
            NobleButton {
                variant: "icon"; iconName: "download"; minHeight: 32; implicitWidth: 32
                ToolTip.visible: hovered; ToolTip.text: root.t("console_to_bottom")
                onClicked: { logView.positionViewAtEnd(); root.autoScroll = true }
            }
            NobleButton {
                variant: "icon"; iconName: "copy"; minHeight: 32; implicitWidth: 32
                ToolTip.visible: hovered; ToolTip.text: root.t("console_copy")
                onClicked: App.copyText(App.consoleText())
            }
            NobleButton {
                variant: "icon"; iconName: "save"; minHeight: 32; implicitWidth: 32
                ToolTip.visible: hovered; ToolTip.text: root.t("console_save")
                onClicked: saveDialog.open()
            }
            NobleButton {
                variant: "icon"; iconName: "trash"; minHeight: 32; implicitWidth: 32
                ToolTip.visible: hovered; ToolTip.text: root.t("console_clear")
                onClicked: App.clearConsole()
            }
            NobleButton {
                variant: "icon"; iconName: "close"; minHeight: 32; implicitWidth: 32
                ToolTip.visible: hovered; ToolTip.text: root.t("console_close")
                onClicked: root.requestClose()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: App.settings.theme === "light" ? "#f7f7fa" : "#07070a"
            border.width: 1
            border.color: App.colorBorder
            clip: true

            ListView {
                id: logView
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: App.consoleModel
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 400
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                function isAtBottom() {
                    return contentHeight - (contentY + height) < 24
                }

                onCountChanged: if (root.autoScroll && isAtBottom()) positionViewAtEnd()
                onMovementStarted: root.autoScroll = isAtBottom()

                delegate: RowLayout {
                    id: lineDelegate
                    required property string type
                    required property string text
                    required property string timeText
                    width: logView.width
                    spacing: 8
                    visible: root.searchText.length === 0 || text.toLowerCase().indexOf(root.searchText.toLowerCase()) >= 0
                    height: visible ? Math.max(18, lineText.implicitHeight) : 0

                    Text {
                        text: lineDelegate.timeText
                        color: App.colorTextDim
                        font.family: "Consolas"
                        font.pixelSize: App.fs(11)
                        Layout.alignment: Qt.AlignTop
                    }
                    Text {
                        id: lineText
                        Layout.fillWidth: true
                        text: lineDelegate.text
                        wrapMode: root.wrapLines ? Text.WrapAnywhere : Text.NoWrap
                        color: lineDelegate.type === "error" ? App.colorError
                             : lineDelegate.type === "warn" ? App.colorWarn
                             : lineDelegate.type === "success" ? App.colorSuccess
                             : lineDelegate.type === "game" ? (App.settings.theme === "light" ? "#2b4a8a" : "#b9c8e6")
                             : App.colorText
                        font.family: "Consolas"
                        font.pixelSize: App.fs(11.5)
                        elide: root.wrapLines ? Text.ElideNone : Text.ElideRight
                        textFormat: Text.PlainText
                        Layout.alignment: Qt.AlignTop
                    }
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: root.t("console_save")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Text files (*.txt *.log)"]
        defaultSuffix: "txt"
        onAccepted: App.saveConsoleToFile(selectedFile.toString())
    }

    Behavior on height { NumberAnimation { duration: App.animationsEnabled ? 200 : 1; easing.type: Easing.OutCubic } }
}
