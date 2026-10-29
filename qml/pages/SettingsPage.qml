import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

Item {
    id: page
    property var rootRef
    property string activeSection: "interface"
    property var selectedOps: ({})
    property var selectedPids: ({})
    property bool showProcesses: false

    function t(key) { return I18n.t(App.lang, key) }
    function activate(id) { page.activeSection = id }
    function opSelected(id) { return page.selectedOps[id] !== undefined ? page.selectedOps[id] : true }
    function setOp(id, value) {
        var copy = {}
        for (var key in page.selectedOps) copy[key] = page.selectedOps[key]
        copy[id] = value
        page.selectedOps = copy
    }
    function selectedOpList() {
        var list = []
        var ops = App.optimizerStatus.ops || []
        for (var i = 0; i < ops.length; ++i) {
            var id = ops[i].id
            if (ops[i].state === "unsupported") continue
            if (opSelected(id)) list.push(id)
        }
        return list
    }
    function pidSelected(pid) { return page.selectedPids[String(pid)] === true }
    function setPid(pid, value) {
        var copy = {}
        for (var key in page.selectedPids) copy[key] = page.selectedPids[key]
        copy[String(pid)] = value
        page.selectedPids = copy
    }
    function selectedPidList() {
        var list = []
        for (var key in page.selectedPids) {
            if (page.selectedPids[key]) list.push(Number(key))
        }
        return list
    }
    function steamStateText() {
        if (!App.steamStatus.installed) return page.t("friends_not_installed")
        if (!App.steamStatus.running) return page.t("friends_not_running")
        if (!App.steamStatus.loggedIn) return page.t("friends_not_logged")
        return page.t("friends_ready")
    }
    function updateStateText() {
        var state = App.updateStatus.state
        if (state === "available") return page.t("about_update_available") + App.updateStatus.version
        if (state === "not-available") return page.t("about_update_none")
        if (state === "checking") return page.t("loading")
        if (state === "error") return page.t("about_update_error")
        return page.t("about_check_updates")
    }
    function backgroundTiles() {
        var list = [{ id: "", name: page.t("background_none"), source: "", thumbSource: "", active: App.settings.activeBackgroundId === "", add: false }]
        var items = App.backgrounds
        for (var i = 0; i < items.length; ++i) {
            var item = items[i]
            list.push({ id: item.id, name: item.name, source: item.source, thumbSource: item.thumbSource, active: App.settings.activeBackgroundId === item.id, add: false })
        }
        list.push({ id: "__add__", name: page.t("add_backgrounds"), source: "", thumbSource: "", active: false, add: true })
        return list
    }

    function setSection(section) {
        if (section && section.length > 0)
            page.activeSection = section
    }
    Component.onCompleted: {
        if (rootRef && rootRef.settingsSection)
            activeSection = rootRef.settingsSection
        App.loadMinecraftVersions()
        App.detectJava()
    }
    onActiveSectionChanged: {
        if (activeSection === "optimize") App.refreshOptimizer()
        if (activeSection === "java") App.detectJava()
        if (activeSection === "friends") App.refreshSteam()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        Card {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            padding: 10

            ColumnLayout {
                anchors.fill: parent
                spacing: 3
                Repeater {
                    model: [
                        { id: "interface", icon: "settings", key: "settings_interface" },
                        { id: "appearance", icon: "palette", key: "settings_appearance" },
                        { id: "font", icon: "font", key: "settings_font" },
                        { id: "java", icon: "java", key: "settings_java" },
                        { id: "performance", icon: "rocket", key: "settings_performance" },
                        { id: "disk", icon: "disk", key: "settings_disk" },
                        { id: "optimize", icon: "star", key: "settings_optimize" },
                        { id: "friends", icon: "users", key: "settings_friends" },
                        { id: "about", icon: "console", key: "settings_about" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 8
                        color: page.activeSection === modelData.id ? App.colorAccentSoft : (sectionHover.hovered ? App.colorSurfaceAlt : "transparent")
                        HoverHandler { id: sectionHover }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 10
                            Icon {
                                name: modelData.icon
                                size: 17
                                color: page.activeSection === modelData.id ? App.colorAccent : App.colorTextDim
                            }
                            Text {
                                Layout.fillWidth: true
                                text: page.t(modelData.key)
                                color: page.activeSection === modelData.id ? App.colorText : App.colorTextDim
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(12)
                                font.weight: page.activeSection === modelData.id ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.activate(modelData.id) }
                    }
                }
                Item { Layout.fillHeight: true }
                Text {
                    Layout.fillWidth: true
                    text: "Noble Launcher " + App.version + "\nC++ / Qt Quick"
                    color: App.colorTextDim
                    font.family: App.fontFamily
                    font.pixelSize: App.fs(9)
                }
            }
        }

        ScrollView {
            id: contentScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: contentScroll.availableWidth
                spacing: 12

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "interface"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 14
                        Text { text: page.t("settings_interface"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 24
                            rowSpacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Text { text: page.t("language"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                NobleComboBox {
                                    Layout.fillWidth: true
                                    model: [{ value: "ru", label: "Русский" }, { value: "en", label: "English" }]
                                    textRole: "label"
                                    currentIndex: App.lang === "en" ? 1 : 0
                                    onActivated: App.settings.lang = model[index].value
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Text { text: page.t("theme"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                NobleComboBox {
                                    Layout.fillWidth: true
                                    model: [{ value: "dark", label: page.t("theme_dark") }, { value: "light", label: page.t("theme_light") }]
                                    textRole: "label"
                                    currentIndex: App.settings.theme === "light" ? 1 : 0
                                    onActivated: App.settings.theme = model[index].value
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Text { text: page.t("accent"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                RowLayout {
                                    spacing: 8
                                    Repeater {
                                        model: ["#e11d2e", "#ff5a1f", "#8b5cf6", "#2f7cf6", "#12a37a"]
                                        delegate: Rectangle {
                                            required property string modelData
                                            width: 26; height: 26; radius: 13
                                            color: modelData
                                            border.width: App.settings.accent === modelData ? 3 : 1
                                            border.color: App.settings.accent === modelData ? App.colorText : App.colorBorder
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.settings.accent = modelData }
                                        }
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                Text { text: page.t("density"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                NobleComboBox {
                                    Layout.fillWidth: true
                                    model: [{ value: "comfortable", label: page.t("density_comfortable") }, { value: "compact", label: page.t("density_compact") }]
                                    textRole: "label"
                                    currentIndex: App.settings.density === "compact" ? 1 : 0
                                    onActivated: App.settings.density = model[index].value
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: page.t("font_scale"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); Layout.fillWidth: true }
                                Text { text: Math.round(App.settings.fontScale * 100) + "%"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            }
                            NobleSlider {
                                Layout.fillWidth: true
                                from: 0.8; to: 1.4; stepSize: 0.05
                                value: App.settings.fontScale
                                onMoved: App.settings.fontScale = value
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("animations"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text { text: page.t("animations_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                Text { visible: App.reducedMotion; text: page.t("reduced_motion"); color: App.colorWarn; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                            }
                            NobleSwitch {
                                checked: App.settings.animations
                                onToggled: App.settings.animations = checked
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("intro"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text { text: page.t("intro_sound_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                            }
                            Text { text: page.t("intro_sound"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            NobleSwitch {
                                checked: App.settings.introSound
                                onToggled: App.settings.introSound = checked
                            }
                            NobleButton {
                                variant: "ghost"; iconName: "play"; text: page.t("intro_replay"); minHeight: 30; fontSize: 10
                                onClicked: App.replayIntro()
                            }
                            NobleSwitch {
                                checked: App.settings.introEnabled
                                onToggled: App.settings.introEnabled = checked
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("show_console"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text { text: page.t("show_console_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                            NobleSwitch {
                                checked: App.settings.showConsole
                                onToggled: App.settings.showConsole = checked
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: page.t("links"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); Layout.fillWidth: true }
                            NobleButton { variant: "secondary"; iconName: "telegram"; text: page.t("telegram"); minHeight: 30; fontSize: 10; onClicked: App.openExternal(App.telegramUrl) }
                            NobleButton { variant: "secondary"; iconName: "globe"; text: page.t("website"); minHeight: 30; fontSize: 10; onClicked: App.openExternal(App.siteUrl) }
                            NobleButton { variant: "secondary"; iconName: "github"; text: page.t("github"); minHeight: 30; fontSize: 10; onClicked: App.openExternal(App.githubUrl) }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "appearance"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("backgrounds"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                            NobleButton { variant: "secondary"; iconName: "plus"; text: page.t("add_backgrounds"); minHeight: 32; fontSize: 10; onClicked: backgroundDialog.open() }
                            NobleButton { variant: "icon"; iconName: "folder"; minHeight: 32; implicitWidth: 32; ToolTip.visible: hovered; ToolTip.text: page.t("background_open_folder"); onClicked: App.openBackgroundsFolder() }
                        }
                        Text { Layout.fillWidth: true; text: page.t("background_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }

                        GridView {
                            id: backgroundsGrid
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            cellWidth: 190
                            cellHeight: 128
                            clip: true
                            model: page.backgroundTiles()
                            ScrollBar.vertical: ScrollBar {}
                            delegate: Item {
                                required property var modelData
                                width: backgroundsGrid.cellWidth
                                height: backgroundsGrid.cellHeight
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    radius: 10
                                    color: App.colorSurfaceAlt
                                    border.width: modelData.active ? 2 : 1
                                    border.color: modelData.active ? App.colorAccent : App.colorBorder
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        source: modelData.thumbSource || modelData.source
                                        visible: source.length > 0 && !modelData.add
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                    }
                                    Rectangle {
                                        anchors.fill: parent
                                        visible: modelData.add
                                        color: "transparent"
                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            Icon { name: "plus"; size: 26; color: App.colorAccent; Layout.alignment: Qt.AlignHCenter }
                                            Text { text: modelData.name; color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                                        }
                                    }
                                    Rectangle {
                                        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 6
                                        radius: 6
                                        color: Qt.rgba(0,0,0,0.55)
                                        implicitHeight: 20; implicitWidth: bgName.implicitWidth + 12
                                        visible: !modelData.add
                                        Text { id: bgName; anchors.centerIn: parent; text: modelData.name; color: "#ffffff"; font.family: App.fontFamily; font.pixelSize: App.fs(8); elide: Text.ElideRight; width: Math.min(implicitWidth, 130) }
                                    }
                                    Rectangle {
                                        visible: !modelData.add && modelData.id !== ""
                                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 5
                                        width: 22; height: 22; radius: 6
                                        color: Qt.rgba(0,0,0,0.6)
                                        Icon { anchors.centerIn: parent; name: "trash"; size: 12; color: "#ffffff" }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (page.rootRef)
                                                    page.rootRef.confirm(page.t("background_remove"), modelData.name, page.t("delete"), function() { App.removeBackground(modelData.id) })
                                            }
                                        }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (modelData.add) backgroundDialog.open()
                                            else App.setBackground(modelData.id)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: page.t("blur"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                Text { text: App.settings.backgroundBlur + "%"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            }
                            NobleSlider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: App.settings.backgroundBlur; onMoved: App.settings.backgroundBlur = Math.round(value) }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: page.t("darken"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                Text { text: App.settings.backgroundDarken + "%"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            }
                            NobleSlider { Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: App.settings.backgroundDarken; onMoved: App.settings.backgroundDarken = Math.round(value) }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: page.t("saturation"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                                Text { text: App.settings.backgroundSaturation + "%"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            }
                            NobleSlider { Layout.fillWidth: true; from: 0; to: 200; stepSize: 5; value: App.settings.backgroundSaturation; onMoved: App.settings.backgroundSaturation = Math.round(value) }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "font"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: page.t("settings_font"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        Text { Layout.fillWidth: true; text: page.t("font_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }
                        RowLayout {
                            spacing: 6
                            Repeater {
                                model: [{ id: "minecraft", key: "font_minecraft" }, { id: "system", key: "font_system" }, { id: "custom", key: "font_custom" }]
                                delegate: Rectangle {
                                    required property var modelData
                                    implicitHeight: 32; implicitWidth: fontModeLabel.implicitWidth + 24
                                    radius: 8
                                    color: App.settings.fontMode === modelData.id ? App.colorAccent : "transparent"
                                    border.width: 1
                                    border.color: App.settings.fontMode === modelData.id ? App.colorAccent : App.colorBorder
                                    Text {
                                        id: fontModeLabel
                                        anchors.centerIn: parent
                                        text: page.t(modelData.key)
                                        color: App.settings.fontMode === modelData.id ? "#ffffff" : App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(11)
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.applyFontMode(modelData.id, App.settings.activeFontId) }
                                }
                            }
                            Item { Layout.fillWidth: true }
                            NobleButton { variant: "secondary"; iconName: "plus"; text: page.t("font_add"); minHeight: 32; fontSize: 10; onClicked: fontDialog.open() }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 84
                            radius: 10
                            color: App.colorSurfaceAlt
                            border.width: 1
                            border.color: App.colorBorder
                            Text {
                                anchors.centerIn: parent
                                text: page.t("font_preview_text")
                                color: App.colorText
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(16)
                                wrapMode: Text.WordWrap
                                width: parent.width - 30
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(220, Math.max(60, App.customFonts.length * 52))
                            visible: App.customFonts.length > 0
                            spacing: 4
                            clip: true
                            model: App.customFonts
                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 48
                                radius: 8
                                color: modelData.active ? App.colorAccentSoft : App.colorSurfaceAlt
                                border.width: 1
                                border.color: modelData.active ? App.colorAccent : App.colorBorder
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    Icon { name: "font"; size: 18; color: App.colorAccent }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Text { Layout.fillWidth: true; text: modelData.name; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(11); elide: Text.ElideRight }
                                        Text { text: modelData.format + " · " + App.formatBytes(modelData.size); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                                    }
                                    NobleButton {
                                        variant: "secondary"; text: page.t("font_apply"); minHeight: 26; fontSize: 9
                                        onClicked: App.applyFontMode("custom", modelData.id)
                                    }
                                    NobleButton {
                                        variant: "icon"; iconName: "trash"; minHeight: 26; implicitWidth: 26
                                        onClicked: App.removeFont(modelData.id)
                                    }
                                }
                            }
                        }
                        Text { visible: App.customFonts.length === 0; Layout.fillWidth: true; text: page.t("font_empty"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "java"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: page.t("settings_java"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        RowLayout {
                            spacing: 6
                            Repeater {
                                model: [{ id: "auto", key: "java_auto" }, { id: "manual", key: "java_manual" }]
                                delegate: Rectangle {
                                    required property var modelData
                                    implicitHeight: 32; implicitWidth: javaModeLabel.implicitWidth + 24
                                    radius: 8
                                    color: App.settings.javaMode === modelData.id ? App.colorAccent : "transparent"
                                    border.width: 1
                                    border.color: App.settings.javaMode === modelData.id ? App.colorAccent : App.colorBorder
                                    Text {
                                        id: javaModeLabel
                                        anchors.centerIn: parent
                                        text: page.t(modelData.key)
                                        color: App.settings.javaMode === modelData.id ? "#ffffff" : App.colorTextDim
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(11)
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.settings.javaMode = modelData.id }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: App.settings.javaMode === "manual" ? page.t("java_manual_hint") : page.t("java_auto_hint")
                            color: App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(10)
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: App.settings.javaMode === "manual"
                            NobleTextField {
                                Layout.fillWidth: true
                                text: App.settings.javaPath
                                placeholderText: "C:\\Program Files\\Java\\jdk-21\\bin\\javaw.exe"
                                onTextChanged: App.settings.javaPath = text
                            }
                            NobleButton { variant: "secondary"; iconName: "folder"; text: page.t("choose_java"); minHeight: 38; fontSize: 10; onClicked: javaDialog.open() }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("java_detected"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            NobleButton { variant: "ghost"; iconName: "refresh"; text: page.t("refresh"); minHeight: 28; fontSize: 10; onClicked: App.detectJava() }
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(220, Math.max(70, App.javaList.length * 44))
                            spacing: 4
                            clip: true
                            model: App.javaList
                            delegate: Rectangle {
                                id: javaItem
                                required property var modelData
                                property bool current: App.settings.javaPath === modelData.path
                                width: ListView.view.width
                                height: 40
                                radius: 8
                                color: current ? App.colorAccentSoft : (javaHover.hovered ? App.colorSurface : App.colorSurfaceAlt)
                                border.width: 1
                                border.color: current ? App.colorAccent : App.colorBorder
                                HoverHandler { id: javaHover }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    Icon { name: "java"; size: 16; color: App.colorAccent }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.label || ("Java " + (modelData.majorVersion > 0 ? modelData.majorVersion : "?") + " · " + modelData.path)
                                        color: App.colorText
                                        font.family: App.fontFamily
                                        font.pixelSize: App.fs(10)
                                        elide: Text.ElideMiddle
                                    }
                                    Icon { name: "check"; size: 13; color: App.colorAccent; visible: javaItem.current }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        App.settings.javaMode = "manual"
                                        App.settings.javaPath = modelData.path
                                    }
                                }
                            }
                        }
                        Text { visible: App.javaList.length === 0; Layout.fillWidth: true; text: page.t("java_none"); color: App.colorWarn; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: page.t("java_install"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); Layout.fillWidth: true }
                            NobleButton { variant: "secondary"; text: "Java 8"; minHeight: 30; fontSize: 10; onClicked: App.installJava(8) }
                            NobleButton { variant: "secondary"; text: "Java 17"; minHeight: 30; fontSize: 10; onClicked: App.installJava(17) }
                            NobleButton { variant: "secondary"; text: "Java 21"; minHeight: 30; fontSize: 10; onClicked: App.installJava(21) }
                        }
                        Text { Layout.fillWidth: true; text: page.t("java_note"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "performance"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: page.t("settings_performance"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("ram"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            Text { text: App.settings.ram + " " + page.t("mmb"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        }
                        NobleSlider { Layout.fillWidth: true; from: 1024; to: 16384; stepSize: 512; value: App.settings.ram; onMoved: App.settings.ram = Math.round(value) }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("performance_concurrency"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            Text { text: App.settings.concurrency; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        }
                        NobleSlider { Layout.fillWidth: true; from: 1; to: 16; stepSize: 1; value: App.settings.concurrency; onMoved: App.settings.concurrency = Math.round(value) }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("performance_auto_update"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                            }
                            NobleSwitch { checked: App.settings.autoUpdate; onToggled: App.settings.autoUpdate = checked }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("performance_memory"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Electron ~200–260 МБ → C++/Qt " + App.memoryMB + " МБ"
                                    color: App.colorSuccess
                                    font.family: App.fontFamily
                                    font.pixelSize: App.fs(11)
                                    font.weight: Font.DemiBold
                                }
                                Text { Layout.fillWidth: true; text: page.t("performance_memory_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                            }
                            NobleButton { variant: "secondary"; iconName: "trash"; text: page.t("performance_clear_cache"); minHeight: 32; fontSize: 10; onClicked: App.clearAppCache() }
                        }
                        Text { Layout.fillWidth: true; text: page.t("show_console_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "disk"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: page.t("settings_disk"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                        Text { Layout.fillWidth: true; text: page.t("disk_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.preferredWidth: 150; text: page.t("disk_instances"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            NobleTextField { Layout.fillWidth: true; text: App.settings.instancesDir; readOnly: true }
                            NobleButton { variant: "secondary"; text: page.t("disk_change"); minHeight: 38; fontSize: 10; onClicked: page.rootRef.openStorageDialog("instances") }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.preferredWidth: 150; text: page.t("disk_runtime"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            NobleTextField { Layout.fillWidth: true; text: App.settings.runtimeDir; readOnly: true }
                            NobleButton { variant: "secondary"; text: page.t("disk_change"); minHeight: 38; fontSize: 10; onClicked: page.rootRef.openStorageDialog("runtime") }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            NobleButton { variant: "secondary"; iconName: "folder"; text: page.t("disk_open_data"); minHeight: 32; fontSize: 10; onClicked: App.openDataFolder() }
                            NobleButton {
                                variant: "ghost"; iconName: "refresh"; text: page.t("disk_migration"); minHeight: 32; fontSize: 10
                                onClicked: page.rootRef.confirm(page.t("disk_migration"), page.t("disk_migration_hint"), page.t("refresh"), function() { App.rollbackInstanceMigration() })
                            }
                            NobleButton {
                                variant: "ghost"; iconName: "rotate"; text: "Откатить импорт данных"; minHeight: 32; fontSize: 10
                                onClicked: App.rollbackDataMigration()
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text { Layout.fillWidth: true; text: page.t("disk_migration_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "optimize"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("settings_optimize"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }
                            BusyIndicator { visible: App.optimizerBusy; running: App.optimizerBusy; width: 22; height: 22 }
                            NobleButton { variant: "ghost"; iconName: "refresh"; text: page.t("refresh"); minHeight: 30; fontSize: 10; onClicked: App.refreshOptimizer() }
                        }
                        Text { Layout.fillWidth: true; text: page.t("optimize_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }
                        Text { visible: !App.optimizerStatus.supported; Layout.fillWidth: true; text: page.t("optimize_unavailable"); color: App.colorWarn; font.family: App.fontFamily; font.pixelSize: App.fs(10) }

                        Repeater {
                            model: App.optimizerStatus.ops || []
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 58
                                radius: 9
                                color: App.colorSurfaceAlt
                                border.width: 1
                                border.color: App.colorBorder
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10
                                    NobleCheckBox {
                                        checked: page.opSelected(modelData.id)
                                        enabled: modelData.state !== "unsupported"
                                        onToggled: page.setOp(modelData.id, checked)
                                        visible: modelData.id !== "priority"
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            Layout.fillWidth: true
                                            text: App.lang === "en" ? modelData.titleEn : modelData.titleRu
                                            color: App.colorText
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(11)
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: App.lang === "en" ? modelData.detailEn : modelData.detailRu
                                            color: App.colorTextDim
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(9)
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                    Rectangle {
                                        implicitHeight: 20
                                        implicitWidth: stateText.implicitWidth + 12
                                        radius: 6
                                        color: modelData.state === "applied" ? Qt.rgba(App.colorSuccess.r, App.colorSuccess.g, App.colorSuccess.b, 0.14)
                                             : modelData.state === "unsupported" ? App.colorSurface
                                             : Qt.rgba(App.colorWarn.r, App.colorWarn.g, App.colorWarn.b, 0.12)
                                        Text {
                                            id: stateText
                                            anchors.centerIn: parent
                                            text: modelData.state === "applied" ? page.t("optimize_state_applied")
                                                : modelData.state === "unsupported" ? page.t("optimize_state_unsupported")
                                                : page.t("optimize_state_pending")
                                            color: modelData.state === "applied" ? App.colorSuccess
                                                : modelData.state === "unsupported" ? App.colorTextDim : App.colorWarn
                                            font.family: App.fontFamily
                                            font.pixelSize: App.fs(8)
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            NobleButton {
                                iconName: "rocket"
                                text: page.t("optimize_apply")
                                minHeight: 38
                                enabled: !App.optimizerBusy && App.optimizerStatus.supported === true
                                onClicked: App.applyOptimization(page.selectedOpList(), page.selectedPidList())
                            }
                            NobleButton {
                                variant: "secondary"
                                iconName: "rotate"
                                text: page.t("optimize_rollback")
                                minHeight: 38
                                enabled: !App.optimizerBusy && App.optimizerStatus.supported === true
                                onClicked: App.rollbackOptimization()
                            }
                            Item { Layout.fillWidth: true }
                            NobleButton {
                                variant: "ghost"
                                iconName: "users"
                                text: page.t("optimize_show_processes")
                                minHeight: 34
                                fontSize: 10
                                onClicked: { page.showProcesses = !page.showProcesses; if (page.showProcesses) App.refreshOptimizerProcesses() }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: page.showProcesses
                            spacing: 6
                            Text { text: page.t("optimize_processes"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(11); font.weight: Font.DemiBold }
                            Text { Layout.fillWidth: true; text: page.t("optimize_processes_hint"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                            ListView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(200, Math.max(60, App.optimizerProcesses.length * 36))
                                model: App.optimizerProcesses
                                spacing: 3
                                clip: true
                                delegate: Rectangle {
                                    required property var modelData
                                    width: ListView.view.width
                                    height: 34
                                    radius: 7
                                    color: App.colorSurfaceAlt
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        NobleCheckBox { checked: page.pidSelected(modelData.pid); onToggled: page.setPid(modelData.pid, checked) }
                                        Text { Layout.fillWidth: true; text: modelData.name + " · " + (modelData.title || ""); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(9); elide: Text.ElideRight }
                                        Text { text: modelData.memoryMb + " " + page.t("mmb"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                                    }
                                }
                            }
                            Text { visible: App.optimizerProcesses.length === 0; Layout.fillWidth: true; text: page.t("optimize_no_processes"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }
                        Text { text: page.t("optimize_log"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 100
                            radius: 8
                            color: App.settings.theme === "light" ? "#f7f7fa" : "#07070a"
                            border.width: 1
                            border.color: App.colorBorder
                            clip: true
                            Flickable {
                                anchors.fill: parent
                                anchors.margins: 8
                                contentHeight: logText.implicitHeight
                                clip: true
                                ScrollBar.vertical: ScrollBar {}
                                Text {
                                    id: logText
                                    width: parent.width
                                    text: (App.optimizerStatus.lastLog || []).join("\n")
                                    color: App.colorTextDim
                                    font.family: "Consolas"
                                    font.pixelSize: App.fs(9)
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                        Text { Layout.fillWidth: true; text: page.t("optimize_safety"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "friends"
                    padding: 18
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: page.t("settings_friends"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(17); font.weight: Font.Bold }

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("friends_steam"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text { Layout.fillWidth: true; text: page.t("friends_steam_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                            }
                            NobleSwitch { checked: App.settings.friendsSteam; onToggled: App.settings.friendsSteam = checked }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

                        RowLayout {
                            Layout.fillWidth: true
                            Icon { name: "users"; size: 18; color: App.steamStatus.running && App.steamStatus.loggedIn ? App.colorSuccess : App.colorWarn }
                            Text { Layout.fillWidth: true; text: page.t("friends_status") + ": " + page.steamStateText(); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(11) }
                            NobleButton { variant: "ghost"; iconName: "refresh"; text: page.t("refresh"); minHeight: 28; fontSize: 9; onClicked: App.refreshSteam() }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            NobleButton {
                                variant: "secondary"; iconName: "globe"; text: App.steamStatus.installed ? page.t("friends_open") : page.t("friends_get_steam")
                                minHeight: 32; fontSize: 10
                                onClicked: App.steamStatus.installed ? App.inviteFriends() : App.openSteamStore()
                            }
                            NobleButton {
                                iconName: "download"; text: page.t("friends_install")
                                minHeight: 32; fontSize: 10
                                enabled: App.selectedInstanceId.length > 0
                                onClicked: App.installE4Steam(App.selectedInstanceId)
                            }
                            NobleButton {
                                variant: "secondary"; iconName: "users"; text: page.t("friends_invite")
                                minHeight: 32; fontSize: 10
                                onClicked: App.inviteFriends()
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text { text: page.t("noble_beta"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(12) }
                                Text { Layout.fillWidth: true; text: page.t("noble_beta_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9); wrapMode: Text.WordWrap }
                            }
                            NobleSwitch { checked: App.settings.nobleBuilds; onToggled: App.settings.nobleBuilds = checked }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    visible: page.activeSection === "about"
                    padding: 20
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        RowLayout {
                            spacing: 14
                            Image { source: "qrc:/assets/logo.png"; sourceSize.width: 56; sourceSize.height: 56; Layout.preferredWidth: 54; Layout.preferredHeight: 54 }
                            ColumnLayout {
                                spacing: 1
                                Text { text: "Noble Launcher"; color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(20); font.weight: Font.Black }
                                Text { text: page.t("about_version") + " · C++ / Qt Quick"; color: App.colorAccent; font.family: App.fontFamily; font.pixelSize: App.fs(11) }
                            }
                        }
                        Text { Layout.fillWidth: true; text: page.t("about_desc"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }
                        Text { text: page.t("about_changelog"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(13); font.weight: Font.DemiBold }
                        Repeater {
                            model: ["about_changelog_1", "about_changelog_2", "about_changelog_3", "about_changelog_4", "about_changelog_5"]
                            delegate: RowLayout {
                                required property string modelData
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle { width: 5; height: 5; radius: 3; color: App.colorAccent }
                                Text { Layout.fillWidth: true; text: page.t(modelData); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10); wrapMode: Text.WordWrap }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("about_env"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            Text {
                                text: App.envMissing.length > 0 ? page.t("about_env_missing") + App.envMissing : page.t("about_env_ok")
                                color: App.envMissing.length > 0 ? App.colorWarn : App.colorSuccess
                                font.family: App.fontFamily
                                font.pixelSize: App.fs(10)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: page.t("about_memory"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                            Text { text: App.memoryMB + " " + page.t("mmb"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            NobleButton { variant: "secondary"; iconName: "console"; text: page.t("about_open_logs"); minHeight: 30; fontSize: 10; onClicked: App.openLogs() }
                            NobleButton { variant: "secondary"; iconName: "folder"; text: page.t("disk_open_data"); minHeight: 30; fontSize: 10; onClicked: App.openDataFolder() }
                            NobleButton { variant: "secondary"; iconName: "refresh"; text: page.t("about_check_updates"); minHeight: 30; fontSize: 10; onClicked: App.checkUpdates() }
                            Item { Layout.fillWidth: true }
                            Text { text: page.updateStateText(); color: App.updateStatus.state === "available" ? App.colorSuccess : App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(9) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            NobleButton { variant: "ghost"; iconName: "play"; text: page.t("intro_replay"); minHeight: 28; fontSize: 9; onClicked: App.replayIntro() }
                            Item { Layout.fillWidth: true }
                            NobleButton {
                                variant: "ghost"; iconName: "trash"; text: page.t("about_reset"); minHeight: 28; fontSize: 9
                                onClicked: page.rootRef.confirm(page.t("about_reset"), page.t("about_reset_hint"), page.t("about_reset"), function() { App.resetSettings() })
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }

    FileDialog {
        id: backgroundDialog
        title: page.t("add_backgrounds")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: {
            var urls = []
            for (var i = 0; i < selectedFiles.length; ++i) urls.push(selectedFiles[i].toString())
            App.importBackgrounds(urls)
        }
    }
    FileDialog {
        id: fontDialog
        title: page.t("font_add")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Fonts (*.ttf *.otf *.woff *.woff2 *.ttc)"]
        onAccepted: {
            var urls = []
            for (var i = 0; i < selectedFiles.length; ++i) urls.push(selectedFiles[i].toString())
            App.importFonts(urls)
        }
    }
    FileDialog {
        id: javaDialog
        title: page.t("choose_java")
        fileMode: FileDialog.OpenFile
        nameFilters: ["Java (*.exe)"]
        onAccepted: App.settings.javaPath = App.toLocalFile(selectedFile.toString())
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (!drop.hasUrls)
                return
            var urls = []
            for (var i = 0; i < drop.urls.length; ++i) urls.push(drop.urls[i].toString())
            if (page.activeSection === "appearance")
                App.importBackgrounds(urls)
            else if (page.activeSection === "font")
                App.importFonts(urls)
        }
    }
}
