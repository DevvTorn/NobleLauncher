import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

NobleDialog {
    id: control
    property var editingInstance: ({})
    property bool editing: editingInstance && editingInstance.id !== undefined
    property string mcVersion: ""
    property string loader: "Vanilla"
    property string loaderVersion: ""
    property bool suppressLoaderLoad: false

    function t(key) { return I18n.t(App.lang, key) }

    function openCreate() {
        editingInstance = ({})
        nameField.text = ""
        mcVersion = ""
        loader = "Vanilla"
        loaderVersion = ""
        ramSlider.value = App.settings ? App.settings.ram : 4096
        jvmField.text = ""
        gameArgsField.text = ""
        javaField.text = ""
        App.loadMinecraftVersions()
        App.detectJava()
        open()
    }

    function openEdit(instance) {
        if (!instance || !instance.id)
            return
        editingInstance = instance
        nameField.text = instance.name || ""
        mcVersion = instance.version || ""
        loader = instance.loader || "Vanilla"
        loaderVersion = instance.loaderVersion || ""
        ramSlider.value = instance.ram || 4096
        jvmField.text = instance.jvmArgs || ""
        gameArgsField.text = instance.gameArgs || ""
        javaField.text = instance.javaPath || ""
        App.loadMinecraftVersions()
        App.detectJava()
        if (mcVersion)
            App.loadLoaderVersions(loader, mcVersion)
        open()
    }

    function mcIndex() {
        for (var i = 0; i < App.minecraftVersions.length; ++i)
            if (App.minecraftVersions[i].id === mcVersion) return i
        return -1
    }
    function loaderIndex() {
        var list = ["Vanilla", "Forge", "Fabric", "Quilt", "NeoForge"]
        return Math.max(0, list.indexOf(loader))
    }
    function loaderVersionIndex() {
        for (var i = 0; i < App.loaderVersions.length; ++i)
            if (App.loaderVersions[i].version === loaderVersion) return i
        return -1
    }

    title: control.editing ? control.t("settings_build") : control.t("create_build")
    acceptText: control.t("save")
    rejectText: control.t("cancel")
    acceptEnabled: nameField.text.trim().length > 0 && control.mcVersion.length > 0
    width: 580

    onAccepted: {
        var data = {
            name: nameField.text,
            version: control.mcVersion,
            loader: control.loader,
            loaderVersion: loaderVersionField.text,
            ram: Math.round(ramSlider.value),
            jvmArgs: jvmField.text,
            gameArgs: gameArgsField.text,
            javaPath: javaField.text
        }
        if (control.editing)
            App.updateInstance(control.editingInstance.id, data)
        else
            App.createInstance(data)
    }

    ColumnLayout {
        width: parent.width
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: control.t("build_name"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                NobleTextField { id: nameField; Layout.fillWidth: true; placeholderText: control.t("build_name_ph") }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: control.t("mc_version"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                NobleComboBox {
                    Layout.fillWidth: true
                    model: App.minecraftVersions
                    textRole: "id"
                    currentIndex: control.mcIndex()
                    onActivated: {
                        control.mcVersion = App.minecraftVersions[index].id
                        control.loaderVersion = ""
                        control.suppressLoaderLoad = true
                        App.loadLoaderVersions(control.loader, control.mcVersion)
                        control.suppressLoaderLoad = false
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: control.t("loader"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                NobleComboBox {
                    Layout.fillWidth: true
                    model: ["Vanilla", "Forge", "Fabric", "Quilt", "NeoForge"]
                    currentIndex: control.loaderIndex()
                    onActivated: {
                        control.loader = model[index]
                        control.loaderVersion = ""
                        if (control.mcVersion)
                            App.loadLoaderVersions(control.loader, control.mcVersion)
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: control.loader !== "Vanilla"
            Text { text: control.t("loader_version"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
            RowLayout {
                Layout.fillWidth: true
                NobleTextField {
                    id: loaderVersionField
                    Layout.fillWidth: true
                    text: control.loaderVersion
                    placeholderText: control.t("latest")
                    onTextChanged: control.loaderVersion = text
                }
                NobleComboBox {
                    Layout.preferredWidth: 160
                    model: App.loaderVersions
                    textRole: "version"
                    currentIndex: control.loaderVersionIndex()
                    onActivated: {
                        var version = App.loaderVersions[index].version
                        control.loaderVersion = version
                        loaderVersionField.text = version
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: control.t("ram"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                Text { text: Math.round(ramSlider.value) + " " + control.t("mmb"); color: App.colorText; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
            }
            NobleSlider {
                id: ramSlider
                Layout.fillWidth: true
                from: 1024; to: 16384; stepSize: 512
                value: 4096
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: control.t("jvm_args"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                NobleTextField { id: jvmField; Layout.fillWidth: true; placeholderText: "-XX:+UseG1GC …" }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: control.t("game_args"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
                NobleTextField { id: gameArgsField; Layout.fillWidth: true; placeholderText: "--width 1280 --height 720" }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text { text: control.t("java_path"); color: App.colorTextDim; font.family: App.fontFamily; font.pixelSize: App.fs(10) }
            RowLayout {
                Layout.fillWidth: true
                NobleTextField { id: javaField; Layout.fillWidth: true; placeholderText: "Auto" }
                NobleButton { variant: "secondary"; iconName: "folder"; text: control.t("choose_java"); minHeight: 38; fontSize: 10; onClicked: javaDialog.open() }
            }
            NobleComboBox {
                visible: App.javaList.length > 0
                Layout.fillWidth: true
                model: App.javaList
                textRole: "label"
                placeholderText: control.t("java_detected")
                onActivated: javaField.text = App.javaList[index].path
            }
        }
    }

    FileDialog {
        id: javaDialog
        title: control.t("choose_java")
        fileMode: FileDialog.OpenFile
        nameFilters: ["Java (*.exe)"]
        onAccepted: javaField.text = App.toLocalFile(selectedFile.toString())
    }
}
