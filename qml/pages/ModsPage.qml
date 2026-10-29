import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

Item {
    id: page
    property var rootRef
    property var versionProject: ({})
    property string projectType: "mod"
    property string loaderFilter: ""
    property string queryText: ""

    function t(key) { return I18n.t(App.lang, key) }
    function selected() { return App.selectedInstance || ({}) }
    function mcVersion() { return selected().version || "" }
    function currentLoader() { return selected().loader || "" }
    function typeLabel(type) {
        if (type === "modpack") return page.t("type_modpack")
        if (type === "resourcepack") return page.t("type_resourcepack")
        if (type === "shader") return page.t("type_shader")
        return page.t("type_mod")
    }
    function kindFor(type) {
        if (type === "resourcepack") return "resourcepack"
        if (type === "shader") return "shaderpack"
        return "mod"
    }
    function doSearch() {
        App.searchMods(queryText, projectType, mcVersion(), loaderFilter, 0)
    }
    function installProject(project, versionId) {
        if (project.projectType === "modpack") {
            var url = "https://modrinth.com/modpack/" + project.slug
            if (versionId)
                url += "/version/" + versionId
            App.installFromUrl(url)
            return
        }
        App.enqueueMod(project.id, versionId || "", page.kindFor(project.projectType), project.title)
    }
    function openVersions(project) {
        versionProject = project
        App.loadModVersions(project.id, project.projectType)
        versionsDialog.open()
    }
    function statusText(status) {
        if (status === "queued") return page.t("queue_queued")
        if (status === "resolve") return page.t("queue_resolve")
        if (status === "download") return page.t("queue_download")
        if (status === "verify") return page.t("queue_verify")
        if (status === "install") return page.t("queue_install")
        if (status === "done") return page.t("queue_done")
        if (status === "skipped") return page.t("queue_skipped")
        if (status === "error") return page.t("queue_error")
        if (status === "cancelled") return page.t("queue_cancelled")
        return status
    }
    function statusColor(status) {
        if (status === "done") return App.colorSuccess
        if (status === "error") return App.colorError
        if (status === "skipped" || status === "cancelled") return App.colorWarn
        return App.colorAccent
    }

    Component.onCompleted: doSearch()
    Connections {
        target: App
        function onInstancesChanged() { doSearch() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                NobleTextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: page.t("search_mods")
                    text: page.queryText
                    onTextChanged: page.queryText = text
                    onAccepted: page.doSearch()
                }
                NobleComboBox {
                    id: typeCombo
                    Layout.preferredWidth: 150
                    model: [
                        { value: "mod", label: page.t("type_mod") },
                        { value: "modpack", label: page.t("type_modpack") },
                        { value: "resourcepack", label: page.t("type_resourcepack") },
                        { value: "shader", label: page.t("type_shader") }
                    ]
                    textRole: "label"
                    currentIndex: 0
                    onActivated: {
                        page.projectType = model[index].value
                        page.doSearch()
                    }
                }
                NobleComboBox {
                    Layout.preferredWidth: 140
                    visible: page.projectType === "mod"
                    model: [
                        { value: "", label: "Any loader" },
                        { value: "Fabric", label: "Fabric" },
                        { value: "Forge", label: "Forge" },
                        { value: "NeoForge", label: "NeoForge" },
                        { value: "Quilt", label: "Quilt" }
                    ]
                    textRole: "label"
                    currentIndex: {
                        if (!page.loaderFilter) return 0
                        for (var i = 1; i < 5; ++i) if (model[i].value === page.loaderFilter) return i
                        return 0
                    }
                    onActivated: { page.loaderFilter = model[index].value; page.doSearch() }
                }
                NobleButton {
                    iconName: "search"
                    text: page.t("search")
                    minHeight: 40
                    fontSize: 12
                    onClicked: page.doSearch()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    radius: 7
                    color: App.colorSurfaceAlt
                    border.width: 1
                    border.color: App.colorBorder
                    implicitHeight: 28
                    implicitWidth: buildChip.implicitWidth + 18
                    Text {
                        id: buildChip
                        anchors.centerIn: parent
                        text: App.selectedInstanceId
                              ? (page.selected().name + " · " + page.mcVersion() + " · " + page.currentLoader())
                              : page.t("select_instance_first")
                        color: App.selectedInstanceId ? App.colorTextDim : App.colorWarn
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(11)
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: page.t("dependencies_auto")
                    color: App.colorTextDim
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(10)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 44
                radius: 10
                color: App.colorSurface
                border.width: 1
                border.color: App.colorBorder
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6
                    Icon { name: "external"; size: 16; color: App.colorTextDim }
                    NobleTextField {
                        id: directField
                        Layout.fillWidth: true
                        implicitHeight: 32
                        placeholderText: page.t("direct_link_ph")
                        background: Rectangle { color: "transparent" }
                        onAccepted: { App.installFromUrl(text); text = "" }
                    }
                    NobleButton {
                        variant: "icon"
                        iconName: "download"
                        minHeight: 32; implicitWidth: 32
                        ToolTip.visible: hovered
                        ToolTip.text: page.t("direct_link")
                        onClicked: { App.installFromUrl(directField.text); directField.text = "" }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        Text {
                            Layout.fillWidth: true
                            text: (App.modSearchLoading ? page.t("loading") : (App.modSearchResults.length + " " + page.t("type_mod").toLowerCase()))
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(11)
                        }
                        BusyIndicator { visible: App.modSearchLoading; running: App.modSearchLoading; width: 20; height: 20 }
                    }

                    ListView {
                        id: resultsView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: App.modSearchResults
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Item {
                            required property var modelData
                            width: resultsView.width
                            height: 88
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 4
                                radius: 10
                                color: resultHover.hovered ? App.colorSurfaceAlt : "transparent"
                                HoverHandler { id: resultHover }
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12
                                Rectangle {
                                    width: 56; height: 56; radius: 10; clip: true
                                    color: App.colorSurfaceAlt
                                    Image {
                                        anchors.fill: parent
                                        source: modelData.iconUrl || ""
                                        visible: modelData.iconUrl && modelData.iconUrl.length > 0
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        smooth: false
                                    }
                                    Icon {
                                        visible: !modelData.iconUrl || modelData.iconUrl.length === 0
                                        anchors.centerIn: parent
                                        name: "mods"; size: 22; color: App.colorTextDim
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.title
                                            color: App.colorText
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(13)
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Rectangle {
                                            implicitHeight: 18
                                            implicitWidth: typeBadge.implicitWidth + 10
                                            radius: 5
                                            color: App.colorAccentSoft
                                            Text {
                                                id: typeBadge
                                                anchors.centerIn: parent
                                                text: page.typeLabel(modelData.projectType)
                                                color: App.colorAccent
                                                font.family: App.fontFamily
                                                font.pixelSize: App.fs(8)
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.description
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(10)
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: (modelData.author || "") + " · ↓ " + App.formatCount(modelData.downloads || 0)
                                        color: App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(9)
                                        elide: Text.ElideRight
                                    }
                                }
                                NobleButton {
                                    variant: "icon"
                                    iconName: "chevron-down"
                                    minHeight: 32; implicitWidth: 32
                                    ToolTip.visible: hovered
                                    ToolTip.text: page.t("versions")
                                    onClicked: page.openVersions(modelData)
                                }
                                NobleButton {
                                    variant: "icon"
                                    iconName: "download"
                                    minHeight: 36; implicitWidth: 36
                                    enabled: modelData.projectType === "modpack" || App.selectedInstanceId.length > 0
                                    ToolTip.visible: hovered
                                    ToolTip.text: page.t("install")
                                    onClicked: page.installProject(modelData, "")
                                }
                            }
                        }
                    }

                    Text {
                        visible: !App.modSearchLoading && App.modSearchResults.length === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: page.queryText.length === 0 ? page.t("search_prompt") : page.t("no_results")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(12)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 8
                        visible: App.modSearchResults.length >= 20
                        Item { Layout.fillWidth: true }
                        NobleButton {
                            text: page.t("loading").replace("…", "") + " +20"
                            variant: "ghost"
                            minHeight: 32
                            fontSize: 11
                            onClicked: App.loadMoreMods()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        Card {
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            padding: 14
            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: page.t("queue_title")
                        color: App.colorText
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(14)
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        visible: App.queueActive > 0
                        implicitWidth: 22; implicitHeight: 20; radius: 6
                        color: App.colorAccent
                        Text {
                            anchors.centerIn: parent
                            text: App.queueActive
                            color: "#ffffff"
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            font.weight: Font.Bold
                        }
                    }
                    NobleButton {
                        variant: "icon"
                        iconName: "trash"
                        minHeight: 28; implicitWidth: 28
                        ToolTip.visible: hovered
                        ToolTip.text: page.t("queue_clear")
                        onClicked: App.clearQueue()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: page.t("overall_progress")
                        color: App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                        Layout.fillWidth: true
                    }
                    Text {
                        text: Math.round(App.queueProgress * 100) + "%"
                        color: App.colorAccent
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                }
                NobleProgressBar {
                    Layout.fillWidth: true
                    thickness: 4
                    value: App.queueProgress
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    clip: true
                    model: App.queueItems
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: 108
                        radius: 10
                        color: App.colorSurfaceAlt
                        border.width: 1
                        border.color: App.colorBorder

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    color: App.colorText
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(11)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: page.statusText(modelData.status)
                                    color: page.statusColor(modelData.status)
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(9)
                                }
                                NobleButton {
                                    visible: modelData.status !== "done" && modelData.status !== "skipped"
                                             && modelData.status !== "error" && modelData.status !== "cancelled"
                                    variant: "icon"; iconName: "close"; minHeight: 22; implicitWidth: 22
                                    onClicked: App.cancelQueueItem(modelData.id)
                                }
                                NobleButton {
                                    visible: modelData.status === "error" || modelData.status === "cancelled" || modelData.status === "skipped"
                                    variant: "icon"; iconName: "refresh"; minHeight: 22; implicitWidth: 22
                                    onClicked: App.retryQueueItem(modelData.id)
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.error && modelData.error.length > 0 ? modelData.error : modelData.message
                                color: modelData.status === "error" ? App.colorError : App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(9)
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                            NobleProgressBar {
                                Layout.fillWidth: true
                                thickness: 4
                                value: modelData.progress
                                indeterminate: modelData.status === "resolve" && modelData.progress <= 0.01
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.instanceName
                                    color: App.colorTextDim
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(8)
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: modelData.speedBps > 0
                                    text: App.formatSpeed(modelData.speedBps) + (modelData.etaSeconds > 0 ? " · " + modelData.etaSeconds + " " + page.t("sec") : "")
                                    color: App.colorTextDim
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(8)
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: App.queueItems.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: page.t("queue_empty")
                    color: App.colorTextDim
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(11)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    NobleDialog {
        id: versionsDialog
        title: page.versionProject.title || page.t("versions")
        acceptText: page.t("close")
        rejectText: ""
        showFooter: true
        width: 520
        onAccepted: close()

        ColumnLayout {
            width: parent.width
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: page.versionProject.description || ""
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(11)
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                clip: true
                spacing: 4
                model: App.modVersions
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 52
                    radius: 8
                    color: versionHover.hovered ? App.colorSurfaceAlt : "transparent"
                    border.width: 1
                    border.color: App.colorBorder
                    HoverHandler { id: versionHover }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name || modelData.version
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(11)
                                elide: Text.ElideRight
                            }
                            Text {
                                text: (modelData.version || "") + " · " + (modelData.gameVersions || []).join(", ")
                                color: App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(9)
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        NobleButton {
                            variant: "icon"
                            iconName: "download"
                            minHeight: 30; implicitWidth: 30
                            ToolTip.visible: hovered
                            ToolTip.text: page.t("install")
                            onClicked: {
                                page.installProject(page.versionProject, modelData.id)
                                versionsDialog.close()
                            }
                        }
                    }
                }
            }
            Text {
                visible: App.modVersions.length === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                text: page.t("loading")
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(11)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
