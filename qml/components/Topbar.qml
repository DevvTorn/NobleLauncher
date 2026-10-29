import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../I18n.js" as I18n

Item {
    id: root
    property string page: "home"
    property bool narrow: false
    signal openAccounts()
    signal toggleConsole()
    signal openSettings()

    implicitHeight: 76

    function t(key) { return I18n.t(App.lang, key) }

    function titleText() {
        if (page === "home") return App.selectedInstance.name || t("no_build_selected")
        if (page === "builds") return t("nav_builds")
        if (page === "mods") return t("nav_mods")
        if (page === "profile") return t("nav_profile")
        return t("nav_settings")
    }

    function subtitleText() {
        if (page === "home") {
            if (!App.selectedInstanceId) return t("no_builds_hint")
            return (App.selectedInstance.version || "") + " · " + (App.selectedInstance.loader || "Vanilla") + " · " + t("launch_hint")
        }
        if (page === "builds") return t("builds_sub")
        if (page === "mods") return t("mods_sub")
        if (page === "profile") return t("profile_sub")
        return t("settings_sub")
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                text: root.titleText()
                color: App.colorText
                font.family: App.fontFamily
                font.pixelSize: App.fs(22)
                font.weight: Font.Bold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                visible: !root.narrow
                text: root.subtitleText()
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(12)
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            visible: App.taskRunning
            spacing: 8
            Rectangle {
                width: 8; height: 8; radius: 4
                color: App.colorAccent
                SequentialAnimation on opacity {
                    running: App.taskRunning && App.animationsEnabled
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.3; duration: 700 }
                    NumberAnimation { to: 1; duration: 700 }
                }
            }
            ColumnLayout {
                spacing: 0
                Text {
                    text: App.taskTitle
                    color: App.colorTextDim
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(11)
                }
                NobleProgressBar {
                    width: 130
                    thickness: 3
                    value: App.taskProgress
                    indeterminate: App.taskProgress <= 0
                }
            }
            NobleButton {
                visible: App.taskCancellable
                variant: "icon"
                iconName: "close"
                minHeight: 28
                implicitWidth: 28
                ToolTip.visible: hovered
                ToolTip.text: root.t("cancel_task")
                onClicked: App.cancelTask()
            }
        }

        Rectangle {
            width: 1
            height: 28
            color: App.colorBorder
            visible: !root.narrow
        }

        Rectangle {
            id: accountChip
            implicitWidth: Math.min(240, accountRow.implicitWidth + 22)
            implicitHeight: 42
            radius: 21
            color: accountHover.hovered ? App.colorSurfaceAlt : App.colorSurface
            border.width: 1
            border.color: App.colorBorder
            Behavior on color { ColorAnimation { duration: 120 } }

            HoverHandler { id: accountHover }

            RowLayout {
                id: accountRow
                anchors.centerIn: parent
                spacing: 9
                Rectangle {
                    width: 30; height: 30; radius: 15
                    clip: true
                    color: App.colorSurfaceAlt
                    Image {
                        anchors.fill: parent
                        source: App.profileIconSource.length > 0 ? App.profileIconSource : App.avatarSource
                        fillMode: Image.PreserveAspectCrop
                        smooth: false
                        sourceSize.width: 60
                        sourceSize.height: 60
                    }
                }
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: App.selectedAccount.name ? App.selectedAccount.name : root.t("no_account")
                        color: App.colorText
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(12)
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.maximumWidth: 140
                    }
                    Text {
                        text: App.selectedAccount.provider === "elyby" ? root.t("elyby")
                             : App.selectedAccount.provider === "offline" ? root.t("offline")
                             : root.t("add_account")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                }
                Icon { name: "chevron-down"; size: 14; color: App.colorTextDim; Layout.rightMargin: 4 }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openAccounts()
            }
        }

        NobleButton {
            variant: "icon"
            iconName: "console"
            minHeight: 42
            implicitWidth: 42
            ToolTip.visible: hovered
            ToolTip.text: root.t("console")
            onClicked: root.toggleConsole()
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 7
                width: 7; height: 7; radius: 4
                color: App.consoleEnabled ? App.colorAccent : "transparent"
                border.width: App.consoleEnabled ? 0 : 1
                border.color: App.colorTextDim
            }
        }
    }
}
