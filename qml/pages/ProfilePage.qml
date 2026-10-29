import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

Item {
    id: page
    property var rootRef
    property string filter: "all"
    property bool slimModel: false

    function t(key) { return I18n.t(App.lang, key) }
    function levelTitle() { return App.lang === "en" ? App.statsSummary.levelTitleEn : App.statsSummary.levelTitleRu }
    function filteredAchievements() {
        var all = App.statsSummary.achievements || []
        var result = []
        for (var i = 0; i < all.length; ++i) {
            var unlocked = all[i].unlockedAt !== null && all[i].unlockedAt !== undefined
            if (page.filter === "unlocked" && !unlocked) continue
            if (page.filter === "locked" && unlocked) continue
            result.push(all[i])
        }
        return result
    }
    function achievementTitle(a) { return App.lang === "en" ? a.titleEn : a.titleRu }
    function achievementDesc(a) { return App.lang === "en" ? a.descRu : a.descRu }
    function isUnlocked(a) { return a.unlockedAt !== null && a.unlockedAt !== undefined }
    function formatAchievementDate(value) {
        if (!value) return ""
        var d = new Date(value)
        return d.toLocaleDateString()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        ColumnLayout {
            Layout.preferredWidth: 430
            Layout.fillHeight: true
            spacing: 12

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 148
                padding: 16
                RowLayout {
                    anchors.fill: parent
                    spacing: 16
                    Rectangle {
                        width: 82; height: 82; radius: 20
                        color: App.colorSurfaceAlt
                        border.width: 2
                        border.color: App.colorAccent
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: App.profileIconSource.length > 0 ? App.profileIconSource : App.avatarSource
                            fillMode: Image.PreserveAspectCrop
                            smooth: false
                            sourceSize.width: 164
                            sourceSize.height: 164
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: App.selectedAccount.name || page.t("no_account")
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(19)
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }
                        Text {
                            text: App.selectedAccount.provider === "elyby" ? "Ely.by"
                                 : App.selectedAccount.provider === "offline" ? page.t("offline") : page.t("add_account")
                            color: App.colorAccent
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(11)
                            font.weight: Font.DemiBold
                        }
                        RowLayout {
                            visible: App.instances.length > 0
                            spacing: 5
                            Image {
                                width: 16; height: 16; source: App.selectedInstance.iconSource || "qrc:/assets/logo.png"
                                fillMode: Image.PreserveAspectCrop; smooth: false
                            }
                            Text {
                                Layout.fillWidth: true
                                text: (App.selectedInstance.name || "") + " · " + (App.selectedInstance.version || "")
                                color: App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(9)
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            text: page.t("profile_streak") + ": " + (App.statsSummary.streak || 0) + " " + page.t("profile_days")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                        }
                        RowLayout {
                            spacing: 6
                            NobleButton {
                                variant: "ghost"; iconName: "image"; text: page.t("profile_change_icon")
                                minHeight: 28; fontSize: 10
                                onClicked: iconDialog.open()
                            }
                            NobleButton {
                                visible: App.profileIconSource.length > 0
                                variant: "icon"; iconName: "close"; minHeight: 26; implicitWidth: 26
                                ToolTip.visible: hovered; ToolTip.text: page.t("profile_remove_icon")
                                onClicked: App.removeProfileIcon()
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: App.statsSummary.level || 1
                            color: App.colorAccent
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(32)
                            font.weight: Font.Black
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: page.levelTitle()
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Text {
                            text: (App.statsSummary.points || 0) + " " + page.t("profile_points")
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    padding: 12
                    ColumnLayout {
                        spacing: 2
                        Text { text: page.t("profile_time_game"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        Text { text: App.formatDuration((App.statsSummary.counters || {}).gameSeconds || 0); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        Text { text: ((App.statsSummary.counters || {}).gameLaunches || 0) + " · " + page.t("launch"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    padding: 12
                    ColumnLayout {
                        spacing: 2
                        Text { text: page.t("profile_time_launcher"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        Text { text: App.formatDuration(((App.statsSummary.counters || {}).launcherMinutes || 0) * 60); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        Text { text: (App.statsSummary.unlockedCount || 0) + "/" + (App.statsSummary.totalAchievements || 0) + " " + page.t("profile_achievements").toLowerCase(); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 12
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: page.t("profile_skin")
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(14)
                            font.weight: Font.DemiBold
                        }
                        NobleButton {
                            variant: "icon"; iconName: "refresh"; minHeight: 28; implicitWidth: 28
                            ToolTip.visible: hovered; ToolTip.text: page.t("refresh")
                            onClicked: App.refreshSkin()
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 210
                        SkinViewer {
                            anchors.fill: parent
                            showAngleControls: true
                            slim: page.slimModel
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 108
                            height: 34
                            radius: 9
                            color: Qt.rgba(0, 0, 0, 0.55)
                            visible: App.skinLoading
                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                BusyIndicator { width: 16; height: 16; running: App.skinLoading }
                                Text {
                                    text: page.t("loading")
                                    color: "#ffffff"
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(9)
                                }
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: page.t("profile_skin_hint")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(9)
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !App.skinLoading && App.skinError.length > 0
                        text: App.skinError
                        color: App.colorWarn
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(9)
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        NobleSwitch {
                            text: page.t("profile_slim")
                            checked: page.slimModel
                            onToggled: page.slimModel = checked
                        }
                        Item { Layout.fillWidth: true }
                        NobleButton {
                            variant: "secondary"
                            iconName: "upload"
                            text: page.t("profile_upload_skin")
                            minHeight: 34
                            fontSize: 11
                            enabled: App.selectedAccount.provider === "elyby"
                            ToolTip.visible: hovered
                            ToolTip.text: enabled ? page.t("profile_upload_skin") : page.t("profile_skin_offline")
                            onClicked: skinDialog.open()
                        }
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 16
            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: page.t("profile_achievements") + " · " + (App.statsSummary.unlockedCount || 0)
                        color: App.colorText
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(15)
                        font.weight: Font.DemiBold
                    }
                    Repeater {
                        model: [
                            { id: "all", key: "profile_all" },
                            { id: "unlocked", key: "profile_unlocked" },
                            { id: "locked", key: "profile_locked" }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            implicitHeight: 28
                            implicitWidth: filterLabel.implicitWidth + 20
                            radius: 7
                            color: page.filter === modelData.id ? App.colorAccent : "transparent"
                            border.width: 1
                            border.color: page.filter === modelData.id ? App.colorAccent : App.colorBorder
                            Text {
                                id: filterLabel
                                anchors.centerIn: parent
                                text: page.t(modelData.key)
                                color: page.filter === modelData.id ? "#ffffff" : App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(10)
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.filter = modelData.id }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: page.t("profile_to_next")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                    NobleProgressBar {
                        Layout.fillWidth: true
                        thickness: 4
                        value: App.statsSummary.nextAt > 0 ? Math.min(1, (App.statsSummary.points || 0) / App.statsSummary.nextAt) : 1
                    }
                    Text {
                        text: App.statsSummary.nextAt > 0
                              ? (App.statsSummary.toNext + " " + page.t("profile_points"))
                              : page.t("profile_max_level")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                }

                GridView {
                    id: achievementsGrid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    cellWidth: Math.max(250, width / 2)
                    cellHeight: 98
                    model: page.filteredAchievements()
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Item {
                        required property var modelData
                        width: achievementsGrid.cellWidth
                        height: achievementsGrid.cellHeight
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: 10
                            color: achievementHover.hovered ? App.colorSurfaceAlt : "transparent"
                            border.width: 1
                            border.color: page.isUnlocked(modelData)
                                          ? Qt.rgba(App.colorSuccess.r, App.colorSuccess.g, App.colorSuccess.b, 0.35)
                                          : App.colorBorder
                            HoverHandler { id: achievementHover }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10
                                Rectangle {
                                    width: 40; height: 40; radius: 10
                                    color: page.isUnlocked(modelData) ? Qt.rgba(App.colorSuccess.r, App.colorSuccess.g, App.colorSuccess.b, 0.14) : App.colorSurfaceAlt
                                    Icon {
                                        anchors.centerIn: parent
                                        name: page.isUnlocked(modelData) ? "trophy" : "lock"
                                        size: 18
                                        color: page.isUnlocked(modelData) ? App.colorSuccess : App.colorTextDim
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: page.achievementTitle(modelData)
                                        color: page.isUnlocked(modelData) ? App.colorText : App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(11)
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: App.lang === "en" ? modelData.descEn : modelData.descRu
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(9)
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: "+" + modelData.points + " · "
                                              + (page.isUnlocked(modelData) ? page.formatAchievementDate(modelData.unlockedAt) : page.t("profile_locked_short"))
                                        color: page.isUnlocked(modelData) ? App.colorSuccess : App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(8)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: iconDialog
        title: page.t("profile_pick_icon")
        fileMode: FileDialog.OpenFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: App.setProfileIcon(selectedFile.toString())
    }
    FileDialog {
        id: skinDialog
        title: page.t("profile_pick_skin")
        fileMode: FileDialog.OpenFile
        nameFilters: ["PNG image (*.png)"]
        onAccepted: App.uploadSkin(selectedFile.toString(), page.slimModel)
    }
}
