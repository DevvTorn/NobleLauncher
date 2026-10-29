import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "dialogs"

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    title: "Noble Launcher 2.9.0"
    color: App.colorBg
    font.family: App.fontFamily

    property string currentPage: "home"
    property string settingsSection: "interface"
    property bool consoleVisible: false

    function pageSource(page) {
        if (page === "builds") return "pages/BuildsPage.qml"
        if (page === "mods") return "pages/ModsPage.qml"
        if (page === "profile") return "pages/ProfilePage.qml"
        if (page === "settings") return "pages/SettingsPage.qml"
        return "pages/HomePage.qml"
    }

    function setPage(page) {
        if (page === currentPage) {
            if (page === "settings")
                settingsSection = settingsSection
            return
        }
        pageLoader.opacity = 0
        currentPage = page
        fadeTimer.restart()
    }

    function applySettingsSection() {
        if (pageLoader.item && pageLoader.item.setSection)
            pageLoader.item.setSection(settingsSection)
    }
    property bool pendingAccount: false
    property var pendingInstance: undefined
    property bool pendingInstanceCreate: false
    property string pendingStorageKind: ""
    property var pendingConfirm: null

    function openAccountDialog() {
        if (accountDialogLoader.item) { accountDialogLoader.item.open(); return }
        pendingAccount = true
        accountDialogLoader.active = true
    }
    function openCreateInstance() {
        if (instanceDialogLoader.item) { instanceDialogLoader.item.openCreate(); return }
        pendingInstanceCreate = true
        pendingInstance = undefined
        instanceDialogLoader.active = true
    }
    function openInstanceDialog(instance) {
        if (instanceDialogLoader.item) { instanceDialogLoader.item.openEdit(instance); return }
        pendingInstanceCreate = false
        pendingInstance = instance
        instanceDialogLoader.active = true
    }
    function openStorageDialog(kind) {
        if (storageDialogLoader.item) { storageDialogLoader.item.openForKind(kind); return }
        pendingStorageKind = kind
        storageDialogLoader.active = true
    }
    function confirm(dialogTitle, message, acceptText, callback) {
        if (confirmDialogLoader.item) {
            confirmDialogLoader.item.dialogTitle = dialogTitle
            confirmDialogLoader.item.message = message
            confirmDialogLoader.item.acceptLabel = acceptText
            confirmDialogLoader.item.acceptCallback = callback
            confirmDialogLoader.item.open()
            return
        }
        pendingConfirm = { dialogTitle: dialogTitle, message: message, acceptText: acceptText, callback: callback }
        confirmDialogLoader.active = true
    }
    function openConsole() {
        App.setConsoleEnabled(true)
        consoleVisible = true
    }
    function closeConsole() {
        consoleVisible = false
        App.setConsoleEnabled(false)
    }
    function toggleConsole() {
        if (consoleVisible) closeConsole()
        else openConsole()
    }

    Timer {
        id: fadeTimer
        interval: 60
        onTriggered: pageLoader.opacity = 1
    }

    BackgroundLayer {
        anchors.fill: parent
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            Layout.fillHeight: true
            Layout.preferredWidth: root.width < 1180 ? 78 : 220
            compact: root.width < 1180
            currentPage: root.currentPage
            onNavigate: (page) => root.setPage(page)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Topbar {
                Layout.fillWidth: true
                page: root.currentPage
                narrow: root.width < 1180
                onOpenAccounts: root.openAccountDialog()
                onToggleConsole: root.toggleConsole()
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Math.round(22 * App.spacingScale)
                Layout.rightMargin: Math.round(22 * App.spacingScale)
                Layout.bottomMargin: Math.round(20 * App.spacingScale)

                Loader {
                    id: pageLoader
                    anchors.fill: parent
                    source: root.pageSource(root.currentPage)
                    asynchronous: false
                    opacity: 1
                    Behavior on opacity { NumberAnimation { duration: App.animationsEnabled ? 180 : 1 } }
                    onLoaded: {
                        if (item) {
                            item.rootRef = root
                            if (item.setSection && root.settingsSection)
                                item.setSection(root.settingsSection)
                        }
                    }
                }

                ConsolePanel {
                    id: consolePanel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    opened: root.consoleVisible
                    onRequestClose: root.closeConsole()
                }
            }
        }
    }

    Loader {
        id: accountDialogLoader
        active: false
        source: "dialogs/AccountDialog.qml"
        onLoaded: if (root.pendingAccount) { root.pendingAccount = false; item.open() }
    }
    Loader {
        id: instanceDialogLoader
        active: false
        source: "dialogs/InstanceDialog.qml"
        onLoaded: {
            if (root.pendingInstanceCreate) {
                root.pendingInstanceCreate = false
                item.openCreate()
            } else if (root.pendingInstance !== undefined) {
                item.openEdit(root.pendingInstance)
                root.pendingInstance = undefined
            }
        }
    }
    Loader {
        id: confirmDialogLoader
        active: false
        source: "dialogs/ConfirmDialog.qml"
        onLoaded: {
            if (root.pendingConfirm) {
                item.dialogTitle = root.pendingConfirm.dialogTitle
                item.message = root.pendingConfirm.message
                item.acceptLabel = root.pendingConfirm.acceptText
                item.acceptCallback = root.pendingConfirm.callback
                root.pendingConfirm = null
                item.open()
            }
        }
    }
    Loader {
        id: storageDialogLoader
        active: false
        source: "dialogs/StorageDialog.qml"
        onLoaded: if (root.pendingStorageKind.length > 0) { item.openForKind(root.pendingStorageKind); root.pendingStorageKind = "" }
    }

    ToastHost {
        anchors.fill: parent
        z: 900
    }

    IntroOverlay {
        id: intro
        anchors.fill: parent
    }

    Connections {
        target: App
        function onConsoleRequested() { root.consoleVisible = true }
        function onNavigationRequested(page, section) {
            root.settingsSection = section && section.length > 0 ? section : root.settingsSection
            root.setPage(page)
            Qt.callLater(root.applySettingsSection)
        }
        function onToast(message, type) {
            if (message === "__replay_intro__")
                intro.play()
        }
    }

    Component.onCompleted: {
        App.profileEvent("skinViewerOpens")
        if (!App.screenshotMode && App.settings.introEnabled)
            intro.play()
    }

    onClosing: {
        App.settings.saveNow()
    }

    Shortcut {
        sequences: ["Ctrl+1"]
        onActivated: root.setPage("home")
    }
    Shortcut {
        sequences: ["Ctrl+2"]
        onActivated: root.setPage("builds")
    }
    Shortcut {
        sequences: ["Ctrl+3"]
        onActivated: root.setPage("mods")
    }
    Shortcut {
        sequences: ["Ctrl+4"]
        onActivated: root.setPage("profile")
    }
    Shortcut {
        sequences: ["Ctrl+5"]
        onActivated: root.setPage("settings")
    }
    Shortcut {
        sequences: ["Ctrl+L"]
        onActivated: root.toggleConsole()
    }
}
