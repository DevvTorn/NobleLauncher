import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../components"
import "../I18n.js" as I18n

ScrollView {
    id: page
    property var rootRef
    clip: true
    contentWidth: width
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    function t(key) { return I18n.t(App.lang, key) }
    function navigate(p) { if (rootRef) rootRef.setPage(p) }
    function levelTitle() { return App.lang === "en" ? App.statsSummary.levelTitleEn : App.statsSummary.levelTitleRu }

    readonly property var counters: App.statsSummary.counters || ({})
    readonly property var selected: App.selectedInstance || ({})
    readonly property int selectedIndex: {
        for (var i = 0; i < App.instances.length; ++i) {
            if (App.instances[i].id === App.selectedInstanceId)
                return i
        }
        return App.instances.length > 0 ? 0 : -1
    }
    readonly property bool installed: String(page.selected.installedVersionId || "").length > 0
    readonly property int gamePhaseCount: App.gamePhase === "running" ? 1 : 0

    ColumnLayout {
        width: page.availableWidth
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 268
            radius: 18
            color: App.settings.theme === "light" ? "#ffffff" : "#121217"
            border.width: 1
            border.color: App.colorBorder
            clip: true

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: App.settings.theme === "light" ? "#ffffff" : "#17171e" }
                    GradientStop { position: 1.0; color: App.settings.theme === "light" ? "#f4f4f7" : "#0e0e13" }
                }
            }

            Rectangle {
                width: 380; height: 380; radius: 190
                x: parent.width - 220
                y: -190
                color: App.colorAccent
                opacity: 0.13
                layer.enabled: true
                layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 96 }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        text: page.t("selected_build")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                        font.letterSpacing: 0.5
                    }
                    NobleComboBox {
                        id: buildCombo
                        Layout.fillWidth: true
                        Layout.maximumWidth: 420
                        implicitHeight: 40
                        model: App.instances
                        textRole: "name"
                        placeholderText: page.t("no_builds")
                        currentIndex: page.selectedIndex
                        onActivated: (index) => { App.selectedInstanceId = App.instances[index].id }
                    }
                    NobleButton {
                        variant: "icon"
                        iconName: "plus"
                        minHeight: 40
                        implicitWidth: 40
                        ToolTip.visible: hovered
                        ToolTip.text: page.t("create_build")
                        onClicked: if (page.rootRef) page.rootRef.openCreateInstance()
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 18

                    Rectangle {
                        Layout.preferredWidth: 108
                        Layout.preferredHeight: 108
                        radius: 20
                        color: App.colorSurfaceAlt
                        border.width: 1
                        border.color: App.colorBorder
                        clip: true
                        Image {
                            anchors.fill: parent
                            anchors.margins: 3
                            source: page.selected.iconSource || "qrc:/assets/logo.png"
                            fillMode: Image.PreserveAspectCrop
                            smooth: false
                            sourceSize.width: 216
                            sourceSize.height: 216
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Text {
                            Layout.fillWidth: true
                            text: page.selected.name || page.t("no_builds")
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(27)
                            font.weight: Font.Black
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                implicitHeight: 26
                                implicitWidth: versionChip.implicitWidth + 16
                                radius: 7
                                color: App.colorSurfaceAlt
                                border.width: 1
                                border.color: App.colorBorder
                                Text {
                                    id: versionChip
                                    anchors.centerIn: parent
                                    text: (page.selected.version || "—") + " · " + (page.selected.loader || "Vanilla")
                                    color: App.colorTextDim
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(10)
                                }
                            }
                            Rectangle {
                                implicitHeight: 26
                                implicitWidth: ramChip.implicitWidth + 16
                                radius: 7
                                color: App.colorSurfaceAlt
                                border.width: 1
                                border.color: App.colorBorder
                                Text {
                                    id: ramChip
                                    anchors.centerIn: parent
                                    text: (page.selected.ram || 0) + " " + page.t("mmb") + " · " + (page.selected.mods ? page.selected.mods.length : 0) + " " + page.t("mods_count")
                                    color: App.colorTextDim
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(10)
                                }
                            }
                            Rectangle {
                                implicitHeight: 26
                                implicitWidth: stateChip.implicitWidth + 16
                                radius: 7
                                color: page.installed ? Qt.rgba(App.colorSuccess.r, App.colorSuccess.g, App.colorSuccess.b, 0.12) : Qt.rgba(App.colorWarn.r, App.colorWarn.g, App.colorWarn.b, 0.1)
                                Text {
                                    id: stateChip
                                    anchors.centerIn: parent
                                    text: page.installed ? page.t("installed_version") : page.t("not_installed")
                                    color: page.installed ? App.colorSuccess : App.colorWarn
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(10)
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: page.selected.name ? page.t("launch_hint") : page.t("no_builds_hint")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(11)
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        Layout.preferredWidth: 250
                        spacing: 7
                        NobleButton {
                            Layout.fillWidth: true
                            implicitHeight: 50
                            text: App.gamePhase === "running" ? page.t("stop")
                                 : App.gamePhase === "stopping" ? page.t("stopping")
                                 : App.gamePhase === "launching" ? page.t("launching")
                                 : page.t("launch")
                            iconName: App.gamePhase === "running" || App.gamePhase === "stopping" ? "stop" : "play"
                            variant: App.gamePhase === "running" ? "danger" : "primary"
                            fontSize: 16
                            busy: App.gamePhase === "launching" || App.gamePhase === "stopping"
                            enabled: App.instances.length > 0 && App.gamePhase !== "stopping"
                            onClicked: {
                                if (App.gamePhase === "running")
                                    App.stopGame()
                                else if (App.gamePhase === "idle")
                                    App.launchGame(App.selectedInstanceId, App.selectedAccountId)
                            }
                        }
                        NobleProgressBar {
                            Layout.fillWidth: true
                            thickness: 5
                            value: App.taskRunning ? App.taskProgress : (App.gamePhase === "launching" ? 0.15 : 0)
                            indeterminate: (App.taskRunning && App.taskProgress <= 0) || App.gamePhase === "launching"
                            visible: App.taskRunning || App.gamePhase === "launching"
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: App.taskRunning || App.gamePhase === "launching"
                            text: App.taskRunning ? (App.taskMessage || App.taskStage) : page.t("launching")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: page.t("profile_time_game") + ": " + App.formatDuration(page.counters.gameSeconds || 0)
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            NobleButton {
                                Layout.fillWidth: true
                                variant: "secondary"
                                iconName: "edit"
                                text: page.t("settings_build")
                                minHeight: 32
                                fontSize: 10
                                enabled: App.instances.length > 0
                                onClicked: page.navigate("builds")
                            }
                            NobleButton {
                                variant: "icon"
                                iconName: "folder"
                                minHeight: 32
                                implicitWidth: 32
                                ToolTip.visible: hovered
                                ToolTip.text: page.t("open_folder")
                                enabled: App.instances.length > 0
                                onClicked: App.openInstanceFolder(App.selectedInstanceId)
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 310
            spacing: 12

            Card {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                padding: 10
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    SkinViewer {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        showAngleControls: true
                        animationMode: "idle"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            Layout.fillWidth: true
                            text: App.skinName
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(11)
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        BusyIndicator {
                            width: 16; height: 16
                            running: App.skinLoading
                            visible: running
                        }
                        Text {
                            visible: !App.skinLoading && App.skinError.length > 0
                            text: page.t("skin_error")
                            color: App.colorWarn
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(9)
                            ToolTip.visible: skinErrorHover.hovered
                            ToolTip.text: App.skinError
                            HoverHandler { id: skinErrorHover }
                        }
                        NobleButton {
                            variant: "icon"
                            iconName: "refresh"
                            minHeight: 26
                            implicitWidth: 26
                            enabled: !App.skinLoading
                            ToolTip.visible: hovered
                            ToolTip.text: page.t("profile_refresh_skin")
                            onClicked: App.refreshSkin()
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
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            width: 52; height: 52; radius: 14
                            color: App.colorSurfaceAlt
                            border.width: 2
                            border.color: App.colorAccent
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: App.profileIconSource.length > 0 ? App.profileIconSource : App.avatarSource
                                fillMode: Image.PreserveAspectCrop
                                smooth: false
                                sourceSize.width: 104
                                sourceSize.height: 104
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: App.selectedAccount.name || page.t("no_account")
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(16)
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                            Text {
                                text: App.selectedAccount.provider === "elyby" ? "Ely.by"
                                      : App.selectedAccount.provider === "offline" ? page.t("offline")
                                      : page.t("add_account")
                                color: App.colorAccent
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(10)
                                font.weight: Font.DemiBold
                            }
                        }
                        NobleButton {
                            variant: "secondary"
                            iconName: "users"
                            text: page.t("profile")
                            minHeight: 32
                            fontSize: 10
                            onClicked: page.navigate("profile")
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 8

                        Repeater {
                            model: [
                                { icon: "trophy", value: page.t("profile_level") + " " + (App.statsSummary.level || 1), label: page.levelTitle() },
                                { icon: "clock", value: App.formatDuration(page.counters.gameSeconds || 0), label: page.t("time_in_game") },
                                { icon: "builds", value: App.instances.length, label: page.t("your_builds") },
                                { icon: "mods", value: (page.counters.modsInstalled || 0), label: page.t("mods_count") }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 10
                                color: App.colorSurfaceAlt
                                border.width: 1
                                border.color: App.colorBorder
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10
                                    Rectangle {
                                        width: 34; height: 34; radius: 9
                                        color: App.colorAccentSoft
                                        Icon { anchors.centerIn: parent; name: modelData.icon; size: 17; color: App.colorAccent }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.value
                                            color: App.colorText
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(14)
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(9)
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            Layout.fillWidth: true
                            text: page.t("profile_points") + ": " + (App.statsSummary.points || 0)
                                  + " · " + (App.statsSummary.unlockedCount || 0) + "/" + (App.statsSummary.totalAchievements || 0)
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(9)
                        }
                        NobleProgressBar {
                            Layout.preferredWidth: 150
                            thickness: 4
                            value: App.statsSummary.nextAt > 0 ? Math.min(1, (App.statsSummary.points || 0) / App.statsSummary.nextAt) : 1
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 170
            spacing: 12

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: page.width * 0.44
                padding: 14
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: page.t("your_builds")
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(13)
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: page.t("all_builds") + " →"
                            color: App.colorAccent
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            TapHandler { onTapped: page.navigate("builds") }
                        }
                    }
                    ColumnLayout {
                        visible: App.instances.length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: page.t("no_builds_hint")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(11)
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
                        }
                        NobleButton {
                            Layout.fillWidth: true
                            iconName: "plus"
                            text: page.t("create_build")
                            minHeight: 32
                            fontSize: 10
                            onClicked: if (page.rootRef) page.rootRef.openCreateInstance()
                        }
                    }
                    ListView {
                        visible: App.instances.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        orientation: ListView.Horizontal
                        spacing: 8
                        clip: true
                        model: App.instances
                        delegate: Rectangle {
                            id: buildCard
                            required property var modelData
                            property bool selectedCard: modelData.id === App.selectedInstanceId
                            width: 194
                            height: 94
                            radius: 11
                            color: selectedCard ? App.colorAccentSoft
                                                : cardHover.hovered ? App.colorSurface
                                                                    : App.colorSurfaceAlt
                            border.width: selectedCard ? 2 : 1
                            border.color: selectedCard ? App.colorAccent
                                                       : cardHover.hovered ? Qt.rgba(App.colorAccent.r, App.colorAccent.g, App.colorAccent.b, 0.45)
                                                                           : App.colorBorder
                            activeFocusOnTab: true
                            scale: cardTap.pressed ? 0.97 : 1
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }
                            Behavior on scale { NumberAnimation { duration: 90 } }
                            HoverHandler { id: cardHover }
                            TapHandler {
                                id: cardTap
                                onTapped: {
                                    buildCard.forceActiveFocus()
                                    App.selectedInstanceId = modelData.id
                                }
                            }
                            Keys.onReturnPressed: App.selectedInstanceId = modelData.id
                            Keys.onSpacePressed: App.selectedInstanceId = modelData.id
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 9
                                Rectangle {
                                    width: 34; height: 34; radius: 8
                                    color: App.colorSurface
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        source: modelData.iconSource || "qrc:/assets/logo.png"
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: false
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: App.colorText
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(11)
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: (modelData.version || "") + " · " + (modelData.loader || "Vanilla")
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(9)
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: (modelData.mods ? modelData.mods.length : 0) + " " + page.t("mods_count")
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(8)
                                    }
                                }
                                Icon {
                                    name: "check"
                                    size: 14
                                    color: App.colorAccent
                                    visible: buildCard.selectedCard
                                }
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "transparent"
                                border.width: buildCard.activeFocus ? 2 : 0
                                border.color: App.colorAccent
                            }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: page.width * 0.27
                padding: 14
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 7
                    Text {
                        text: page.t("playtime_by_build")
                        color: App.colorText
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(13)
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: !App.statsSummary.perBuild || App.statsSummary.perBuild.length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: page.t("no_time_yet")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(11)
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: App.statsSummary.perBuild ? App.statsSummary.perBuild.slice(0, 3) : []
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(10)
                                elide: Text.ElideRight
                            }
                            Text {
                                text: App.formatDuration(modelData.seconds)
                                color: App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(9)
                            }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: page.width * 0.27
                padding: 14
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 7
                    Text {
                        Layout.fillWidth: true
                        text: page.t("news")
                        color: App.colorText
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(13)
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: App.newsItems.length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: page.t("news_empty")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(11)
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: App.newsItems.slice(0, 2)
                        delegate: ColumnLayout {
                            id: newsItem
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: newsItem.modelData.title
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(10)
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                            Text {
                                Layout.fillWidth: true
                                text: newsItem.modelData.category || newsItem.modelData.date || ""
                                color: App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(8)
                                elide: Text.ElideRight
                            }
                            TapHandler {
                                cursorShape: Qt.PointingHandCursor
                                onTapped: if (newsItem.modelData.url) App.openExternal(newsItem.modelData.url)
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 4 }
    }

    Connections {
        target: App
        function onInstancesChanged() { buildCombo.currentIndex = page.selectedIndex }
    }
}
