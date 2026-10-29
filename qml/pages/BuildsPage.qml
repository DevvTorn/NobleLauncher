import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import "../components"
import "../I18n.js" as I18n

Item {
    id: page
    property var rootRef
    property string tab: "mine"
    property string contentKind: "mod"
    property var modsBuild: ({})

    function t(key) { return I18n.t(App.lang, key) }
    function openMods(build) { page.modsBuild = build; modsDialog.open() }
    function selected() { return App.selectedInstance || ({}) }
    function scanItems() {
        var all = App.modsScan.items || []
        var result = []
        for (var i = 0; i < all.length; ++i) {
            if (all[i].kind === page.contentKind)
                result.push(all[i])
        }
        return result
    }
    function openCreate() { if (rootRef) rootRef.openCreateInstance() }
    function openAccount() { if (rootRef) rootRef.openAccountDialog() }
    function setSection(section) { if (section === "noble") page.tab = "noble" }
    function confirmAction(title, text, acceptText, callback) {
        if (rootRef) rootRef.confirm(title, text, acceptText, callback)
    }

    Component.onCompleted: { if (App.selectedInstanceId) App.scanInstance(App.selectedInstanceId) }
    Connections {
        target: App
        function onInstancesChanged() {
            if (App.selectedInstanceId)
                App.scanInstance(App.selectedInstanceId)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    { id: "mine", label: page.t("noble_mine") },
                    { id: "noble", label: page.t("noble_tab") + " · Beta" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    implicitHeight: 36
                    implicitWidth: tabLabel.implicitWidth + 28
                    radius: 9
                    color: page.tab === modelData.id ? App.colorAccent : (tabHover.hovered ? App.colorSurfaceAlt : "transparent")
                    border.width: 1
                    border.color: page.tab === modelData.id ? App.colorAccent : App.colorBorder
                    HoverHandler { id: tabHover }
                    Text {
                        id: tabLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        color: page.tab === modelData.id ? "#ffffff" : App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(13)
                        font.weight: Font.Medium
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.tab = modelData.id }
                }
            }

            Item { Layout.fillWidth: true }

            NobleButton {
                visible: page.tab === "mine"
                variant: "secondary"
                iconName: "download"
                text: page.t("import_build")
                minHeight: 36
                fontSize: 12
                onClicked: importDialog.open()
            }
            NobleButton {
                visible: page.tab === "mine"
                iconName: "plus"
                text: page.t("create_build")
                minHeight: 36
                fontSize: 12
                onClicked: page.openCreate()
            }
            NobleButton {
                visible: page.tab === "noble"
                variant: "secondary"
                iconName: "refresh"
                text: page.t("noble_refresh")
                minHeight: 36
                fontSize: 12
                onClicked: App.loadNobleBuilds(true)
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: page.tab === "mine" ? 0 : 1

            RowLayout {
                spacing: 12

                Card {
                    Layout.preferredWidth: 330
                    Layout.fillHeight: true
                    padding: 12
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        Text {
                            text: page.t("your_builds") + " · " + App.instances.length
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(14)
                            font.weight: Font.DemiBold
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 6
                            clip: true
                            model: App.instances
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 72
                                radius: 10
                                color: modelData.id === App.selectedInstanceId ? App.colorAccentSoft : (itemHover.hovered ? App.colorSurfaceAlt : "transparent")
                                border.width: 1
                                border.color: modelData.id === App.selectedInstanceId ? App.colorAccent : "transparent"
                                HoverHandler { id: itemHover }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10
                                    Rectangle {
                                        width: 42; height: 42; radius: 10; clip: true
                                        color: App.colorSurfaceAlt
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
                                            font.pixelSize: App.fs(13)
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.version || "") + " · " + (modelData.loader || "Vanilla")
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(10)
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.mods ? modelData.mods.length : 0) + " " + page.t("mods_count")
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(9)
                                        }
                                    }
                                    Icon {
                                        name: "chevron-right"
                                        size: 14
                                        color: modelData.id === App.selectedInstanceId ? App.colorAccent : App.colorTextDim
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { App.selectedInstanceId = modelData.id; App.scanInstance(modelData.id) }
                                }
                            }
                        }
                        Text {
                            visible: App.instances.length === 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: page.t("no_builds_hint")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(12)
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Card {
                        anchors.fill: parent
                        visible: App.instances.length > 0
                        padding: 0

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 112
                                radius: 14
                                color: App.settings.theme === "light" ? "#f7f7fa" : "#17171d"
                                border.width: 1
                                border.color: App.colorBorder
                                clip: true

                                Rectangle {
                                    width: 260; height: 260; radius: 130
                                    x: parent.width - 180; y: -120
                                    color: App.colorAccent; opacity: 0.18
                                    layer.enabled: true
                                    layer.effect: MultiEffect { blurEnabled: true; blur: 1; blurMax: 64 }
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 14
                                    Rectangle {
                                        width: 66; height: 66; radius: 16; clip: true
                                        color: App.colorSurface
                                        border.width: 1; border.color: App.colorBorder
                                        Image {
                                            anchors.fill: parent
                                            source: page.selected().iconSource || "qrc:/assets/logo.png"
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: false
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Text {
                                            Layout.fillWidth: true
                                            text: page.selected().name || ""
                                            color: App.colorText
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(20)
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: (page.selected().version || "") + " · " + (page.selected().loader || "Vanilla")
                                                 + (page.selected().loaderVersion ? " " + page.selected().loaderVersion : "")
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(12)
                                        }
                                        Text {
                                            text: String(page.selected().installedVersionId || "").length > 0
                                                  ? page.t("installed_version") : page.t("not_installed")
                                            color: String(page.selected().installedVersionId || "").length > 0 ? App.colorSuccess : App.colorWarn
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(11)
                                        }
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "edit"; minHeight: 34; implicitWidth: 34
                                        ToolTip.visible: hovered; ToolTip.text: page.t("edit")
                                        onClicked: { if (rootRef) rootRef.openInstanceDialog(page.selected()) }
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "copy"; minHeight: 34; implicitWidth: 34
                                        ToolTip.visible: hovered; ToolTip.text: page.t("clone_build")
                                        onClicked: App.cloneInstance(page.selected().id, "")
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "save"; minHeight: 34; implicitWidth: 34
                                        ToolTip.visible: hovered; ToolTip.text: page.t("export_build")
                                        onClicked: exportOptionsDialog.open()
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "folder"; minHeight: 34; implicitWidth: 34
                                        ToolTip.visible: hovered; ToolTip.text: page.t("open_folder")
                                        onClicked: App.openInstanceFolder(page.selected().id)
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "trash"; minHeight: 34; implicitWidth: 34
                                        ToolTip.visible: hovered; ToolTip.text: page.t("delete")
                                        onClicked: {
                                            page.confirmAction(page.t("delete_build"),
                                                               page.t("delete_files"),
                                                               page.t("delete"),
                                                               function() { App.deleteInstance(page.selected().id, true) })
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.margins: 16
                                spacing: 8
                                NobleButton {
                                    iconName: "play"
                                    text: App.gamePhase === "running" ? page.t("stop") : page.t("launch")
                                    variant: App.gamePhase === "running" ? "danger" : "primary"
                                    minHeight: 38
                                    fontSize: 13
                                    onClicked: App.gamePhase === "running" ? App.stopGame()
                                               : App.launchGame(page.selected().id, App.selectedAccountId)
                                }
                                NobleButton {
                                    visible: String(page.selected().installedVersionId || "").length === 0
                                    variant: "secondary"
                                    iconName: "download"
                                    text: page.t("install_version")
                                    minHeight: 38
                                    fontSize: 12
                                    onClicked: App.installInstanceVersion(page.selected().id)
                                }
                                NobleButton {
                                    visible: String(page.selected().installedVersionId || "").length > 0
                                    variant: "secondary"
                                    iconName: "refresh"
                                    text: page.t("reinstall_version")
                                    minHeight: 38
                                    fontSize: 12
                                    onClicked: App.installInstanceVersion(page.selected().id)
                                }
                                NobleButton {
                                    visible: String(page.selected().installedVersionId || "").length > 0
                                    variant: "ghost"
                                    iconName: "trash"
                                    text: page.t("uninstall_version")
                                    minHeight: 38
                                    fontSize: 12
                                    onClicked: {
                                        page.confirmAction(page.t("uninstall_version"), page.t("delete_files"),
                                                           page.t("delete"),
                                                           function() { App.uninstallInstanceVersion(page.selected().id) })
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                NobleButton {
                                    variant: "icon"; iconName: "image"; minHeight: 36; implicitWidth: 36
                                    ToolTip.visible: hovered; ToolTip.text: page.t("profile_change_icon")
                                    onClicked: iconDialog.open()
                                }
                                NobleButton {
                                    variant: "icon"; iconName: "settings"; minHeight: 36; implicitWidth: 36
                                    ToolTip.visible: hovered; ToolTip.text: page.t("settings_build")
                                    onClicked: { if (rootRef) rootRef.openInstanceDialog(page.selected()) }
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: [
                                                { id: "mod", label: page.t("mods") },
                                                { id: "resourcepack", label: page.t("resourcepacks") },
                                                { id: "shaderpack", label: page.t("shaderpacks") }
                                            ]
                                            delegate: Rectangle {
                                                required property var modelData
                                                implicitHeight: 30
                                                implicitWidth: kindLabel.implicitWidth + 22
                                                radius: 8
                                                color: page.contentKind === modelData.id ? App.colorAccentSoft : "transparent"
                                                border.width: 1
                                                border.color: page.contentKind === modelData.id ? App.colorAccent : App.colorBorder
                                                Text {
                                                    id: kindLabel
                                                    anchors.centerIn: parent
                                                    text: modelData.label
                                                    color: page.contentKind === modelData.id ? App.colorAccent : App.colorTextDim
                                                    font.family: App.fontFamily
                                                    font.pixelSize: App.fs(11)
                                                }
                                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.contentKind = modelData.id }
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: (App.modsScan.items ? App.modsScan.items.length : 0) + " " + page.t("mods_count")
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(10)
                                        }
                                        NobleButton {
                                            variant: "icon"; iconName: "refresh"; minHeight: 30; implicitWidth: 30
                                            ToolTip.visible: hovered; ToolTip.text: page.t("scan_refresh")
                                            onClicked: App.scanInstance(page.selected().id)
                                        }
                                    }

                                    ListView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 4
                                        clip: true
                                        model: page.scanItems()
                                        ScrollBar.vertical: ScrollBar {}
                                        delegate: Item {
                                            required property var modelData
                                            width: ListView.view.width
                                            height: 48
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 8
                                                color: rowHover.hovered ? App.colorSurfaceAlt : "transparent"
                                                HoverHandler { id: rowHover }
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 8
                                                Rectangle {
                                                    width: 30; height: 30; radius: 6; clip: true
                                                    color: App.colorSurfaceAlt
                                                    Image {
                                                        anchors.fill: parent
                                                        source: modelData.iconSource || ""
                                                        visible: modelData.iconSource && modelData.iconSource.length > 0
                                                        fillMode: Image.PreserveAspectFit
                                                        smooth: false
                                                    }
                                                    Icon {
                                                        visible: !modelData.iconSource || modelData.iconSource.length === 0
                                                        anchors.centerIn: parent
                                                        name: modelData.kind === "mod" ? "mods" : modelData.kind === "resourcepack" ? "image" : "star"
                                                        size: 16
                                                        color: App.colorTextDim
                                                    }
                                                }
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 0
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 6
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.name
                                                            color: modelData.enabled ? App.colorText : App.colorTextDim
                                                            font.family: App.fontFamily
                                                            font.pixelSize: App.fs(11)
                                                            elide: Text.ElideRight
                                                        }
                                                        Text {
                                                            visible: modelData.warning && modelData.warning.length > 0
                                                            text: "⚠"
                                                            color: App.colorWarn
                                                            font.pixelSize: App.fs(11)
                                                            ToolTip.visible: warningHover.hovered
                                                            ToolTip.text: modelData.warning || ""
                                                            HoverHandler { id: warningHover }
                                                        }
                                                        Rectangle {
                                                            implicitHeight: 18
                                                            implicitWidth: srcLabel.implicitWidth + 10
                                                            radius: 5
                                                            color: modelData.source === "launcher" ? App.colorAccentSoft : App.colorSurfaceAlt
                                                            Text {
                                                                id: srcLabel
                                                                anchors.centerIn: parent
                                                                text: modelData.source === "launcher" ? page.t("source_launcher") : page.t("source_manual")
                                                                color: modelData.source === "launcher" ? App.colorAccent : App.colorTextDim
                                                                font.family: App.fontFamily
                                                                font.pixelSize: App.fs(8)
                                                            }
                                                        }
                                                    }
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: (modelData.version ? modelData.version + " · " : "")
                                                              + (modelData.author ? modelData.author + " · " : "")
                                                              + App.formatBytes(modelData.size)
                                                        color: App.colorTextDim
                                                        font.family: App.fontFamily
                                                        font.pixelSize: App.fs(9)
                                                        elide: Text.ElideRight
                                                    }
                                                }
                                                NobleButton {
                                                    variant: "icon"; iconName: modelData.enabled ? "pause" : "play"
                                                    minHeight: 28; implicitWidth: 28
                                                    ToolTip.visible: hovered; ToolTip.text: page.t("toggle")
                                                    onClicked: App.modFileAction(page.selected().id, modelData.kind, modelData.fileName, "toggle")
                                                }
                                                NobleButton {
                                                    variant: "icon"; iconName: "trash"
                                                    minHeight: 28; implicitWidth: 28
                                                    ToolTip.visible: hovered; ToolTip.text: page.t("delete_file")
                                                    onClicked: {
                                                        page.confirmAction(page.t("delete_file"), modelData.fileName, page.t("delete"),
                                                                           function() { App.modFileAction(page.selected().id, modelData.kind, modelData.fileName, "delete") })
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: page.scanItems().length === 0
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: page.t("content_empty")
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(12)
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        anchors.fill: parent
                        visible: App.instances.length === 0
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10
                            Icon { name: "builds"; size: 54; color: App.colorAccent; Layout.alignment: Qt.AlignHCenter }
                            Text {
                                text: page.t("no_builds")
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(18)
                                font.weight: Font.DemiBold
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                Layout.maximumWidth: 360
                                text: page.t("no_builds_hint")
                                color: App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(12)
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            NobleButton {
                                Layout.alignment: Qt.AlignHCenter
                                text: page.t("create_build")
                                iconName: "plus"
                                onClicked: page.openCreate()
                            }
                        }
                    }
                }
            }

            Item {
                Card {
                    anchors.fill: parent
                    padding: 18
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: page.t("noble_beta")
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(15)
                                font.weight: Font.DemiBold
                            }
                            NobleSwitch {
                                checked: App.settings.nobleBuilds
                                onToggled: App.settings.nobleBuilds = checked
                            }
                        }

                        Text {
                            visible: !App.settings.nobleBuilds
                            Layout.fillWidth: true
                            text: page.t("noble_disabled_hint")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(12)
                            wrapMode: Text.WordWrap
                        }
                        NobleButton {
                            visible: !App.settings.nobleBuilds
                            text: page.t("noble_beta")
                            iconName: "star"
                            onClicked: { App.settings.nobleBuilds = true; App.loadNobleBuilds(false) }
                        }

                        Text {
                            visible: App.nobleFromCache
                            Layout.fillWidth: true
                            text: "Манифест недоступен — показана последняя сохранённая копия."
                            color: App.colorWarn
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            wrapMode: Text.WordWrap
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: App.settings.nobleBuilds
                            model: App.nobleBuilds
                            spacing: 10
                            clip: true
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 116
                                radius: 12
                                color: App.colorSurfaceAlt
                                border.width: 1
                                border.color: App.colorBorder
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12
                                    Rectangle {
                                        width: 84; height: 84; radius: 10; clip: true
                                        color: App.colorSurface
                                        Image {
                                            anchors.fill: parent
                                            source: modelData.banner || modelData.icon || "qrc:/assets/logo.png"
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: App.colorText
                                                font.family: App.fontFamily
                                                font.pixelSize: App.fs(13)
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }
                                            Rectangle {
                                                implicitWidth: 42; implicitHeight: 18; radius: 5
                                                color: App.colorAccentSoft
                                                Text { anchors.centerIn: parent; text: "BETA"; color: App.colorAccent; font.family: App.fontFamily; font.pixelSize: App.fs(8); font.weight: Font.Bold }
                                            }
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.description
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(10)
                                            wrapMode: Text.WordWrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.mcVersion || "") + " · " + (modelData.loader || "Vanilla")
                                                  + " · " + page.t("noble_by") + ": " + (modelData.author || "")
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(9)
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.mods || []).join(" · ")
                                            color: nobleHover.hovered ? App.colorAccent : App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(8)
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                            HoverHandler { id: nobleHover }
                                            TapHandler { cursorShape: Qt.PointingHandCursor; onTapped: page.openMods(modelData) }
                                        }
                                    }
                                    NobleButton {
                                        variant: "icon"
                                        iconName: "download"
                                        minHeight: 38; implicitWidth: 38
                                        enabled: String(modelData.archiveUrl || "").length > 0
                                        ToolTip.visible: hovered
                                        ToolTip.text: enabled ? page.t("noble_install") : page.t("noble_soon")
                                        onClicked: App.installNobleBuild(modelData.id)
                                    }
                                }
                            }
                        }

                        Text {
                            visible: App.settings.nobleBuilds && App.nobleBuilds.length === 0 && !App.nobleLoading
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: page.t("noble_empty")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(12)
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        RowLayout {
                            visible: App.nobleLoading
                            Layout.fillWidth: true
                            spacing: 8
                            BusyIndicator { running: App.nobleLoading; width: 22; height: 22 }
                            Text { text: page.t("loading"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(11) }
                        }
                    }
                }
            }
        }
    }

    NobleDialog {
        id: modsDialog
        title: (page.modsBuild.name || "") + " · " + page.t("mods")
        acceptText: page.t("close")
        showReject: false
        width: 520

        ColumnLayout {
            width: parent.width
            spacing: 6
            Text {
                Layout.fillWidth: true
                text: page.modsBuild.description || ""
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(10)
                wrapMode: Text.WordWrap
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(320, Math.max(80, (page.modsBuild.modLinks || []).length * 36))
                model: page.modsBuild.modLinks || []
                spacing: 4
                clip: true
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 32
                    radius: 7
                    color: linkHover.hovered ? App.colorSurfaceAlt : "transparent"
                    HoverHandler { id: linkHover }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        spacing: 8
                        Icon { name: "mods"; size: 14; color: App.colorAccent }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            elide: Text.ElideRight
                        }
                        NobleButton {
                            variant: "icon"; iconName: "external"; minHeight: 26; implicitWidth: 26
                            ToolTip.visible: hovered; ToolTip.text: modelData.url
                            onClicked: App.openExternal(modelData.url)
                        }
                    }
                }
            }
        }
    }

    NobleDialog {
        id: exportOptionsDialog
        title: page.t("export_build") + " · .Nb"
        acceptText: page.t("export_choose_file")
        rejectText: page.t("cancel")
        width: 460
        onAccepted: exportDialog.open()

        ColumnLayout {
            width: parent.width
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: page.t("export_nb_hint")
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(10)
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text { text: page.t("export_nb_files"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                    Text {
                        Layout.fillWidth: true
                        text: page.t("export_nb_files_desc")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(9)
                        wrapMode: Text.WordWrap
                    }
                }
                NobleSwitch { id: exportEmbedSwitch; checked: true }
            }
            NobleButton {
                Layout.fillWidth: true
                variant: "ghost"
                iconName: "save"
                text: page.t("export_legacy_zip")
                minHeight: 30
                fontSize: 10
                onClicked: { exportOptionsDialog.close(); legacyZipDialog.open() }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: page.t("import_build")
        fileMode: FileDialog.OpenFile
        nameFilters: ["Noble build (*.nb *.zip)", "Noble .Nb (*.nb)", "Legacy ZIP (*.zip)"]
        onAccepted: App.importInstance(selectedFile.toString())
    }
    FileDialog {
        id: exportDialog
        title: page.t("export_build")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Noble .Nb (*.nb)"]
        defaultSuffix: "nb"
        onAccepted: App.exportInstanceNb(page.selected().id, selectedFile.toString(), exportEmbedSwitch.checked)
    }
    FileDialog {
        id: legacyZipDialog
        title: page.t("export_build") + " · ZIP"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Legacy ZIP (*.zip)"]
        defaultSuffix: "zip"
        onAccepted: App.exportInstance(page.selected().id, selectedFile.toString())
    }
    FileDialog {
        id: iconDialog
        title: page.t("profile_pick_icon")
        fileMode: FileDialog.OpenFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: App.setInstanceIcon(page.selected().id, selectedFile.toString())
    }
}
