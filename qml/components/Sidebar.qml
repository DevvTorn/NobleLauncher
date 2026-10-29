import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../I18n.js" as I18n

Rectangle {
    id: root
    property string currentPage: "home"
    property bool compact: false
    signal navigate(string page)

    width: compact ? 78 : 220
    color: App.settings.theme === "light" ? "#ffffff" : "#0d0d10"
    border.width: 1
    border.color: App.colorBorder

    readonly property var navItems: [
        { page: "home", icon: "home", key: "nav_home" },
        { page: "builds", icon: "builds", key: "nav_builds" },
        { page: "mods", icon: "mods", key: "nav_mods" },
        { page: "profile", icon: "profile", key: "nav_profile" },
        { page: "settings", icon: "settings", key: "nav_settings" }
    ]

    function t(key) { return I18n.t(App.lang, key) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Item {
            Layout.fillWidth: true
            implicitHeight: 58
            RowLayout {
                anchors.fill: parent
                spacing: 10
                Image {
                    source: "qrc:/assets/logo.png"
                    sourceSize.width: 40
                    sourceSize.height: 40
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    visible: !root.compact
                    text: "Noble"
                    color: App.colorText
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(17)
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                }
                TapHandler {
                    cursorShape: Qt.PointingHandCursor
                    onTapped: root.navigate("home")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: App.colorBorder
        }

        Repeater {
            model: root.navItems
            delegate: Rectangle {
                id: navButton
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 44
                radius: 10
                color: root.currentPage === modelData.page
                       ? App.colorAccentSoft
                       : (navHover.hovered ? App.colorSurfaceAlt : "transparent")
                Behavior on color { ColorAnimation { duration: 130 } }

                HoverHandler { id: navHover }

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 20
                    radius: 2
                    color: App.colorAccent
                    visible: root.currentPage === modelData.page
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.compact ? 0 : 14
                    anchors.rightMargin: 8
                    spacing: 12
                    Item { Layout.fillWidth: root.compact }
                    Icon {
                        name: modelData.icon
                        size: 21
                        color: root.currentPage === modelData.page ? App.colorAccent : App.colorTextDim
                    }
                    Text {
                        visible: !root.compact
                        Layout.fillWidth: true
                        text: root.t(modelData.key)
                        color: root.currentPage === modelData.page ? App.colorText : App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(14)
                        font.weight: root.currentPage === modelData.page ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillWidth: root.compact }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.navigate(modelData.page)
                }
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 2
            NobleButton {
                variant: "icon"; iconName: "telegram"; minHeight: 30; implicitWidth: 30
                ToolTip.visible: hovered; ToolTip.text: "Telegram"
                Accessible.name: "Telegram"
                onClicked: App.openExternal(App.telegramUrl)
            }
            NobleButton {
                variant: "icon"; iconName: "github"; minHeight: 30; implicitWidth: 30
                ToolTip.visible: hovered; ToolTip.text: "GitHub"
                Accessible.name: "GitHub"
                onClicked: App.openExternal(App.githubUrl)
            }
            NobleButton {
                variant: "icon"; iconName: "globe"; minHeight: 30; implicitWidth: 30
                ToolTip.visible: hovered; ToolTip.text: root.t("website")
                Accessible.name: root.t("website")
                onClicked: App.openExternal(App.siteUrl)
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "v" + App.version
            color: App.colorTextDim
            font.family: App.fontFamily
            font.pixelSize: App.fs(10)
        }
    }
}
