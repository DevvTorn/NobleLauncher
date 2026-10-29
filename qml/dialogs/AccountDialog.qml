import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../I18n.js" as I18n

NobleDialog {
    id: control
    property string mode: "password"
    property bool showAccounts: true

    function t(key) { return I18n.t(App.lang, key) }

    title: control.t("accounts")
    acceptText: ""
    rejectText: control.t("close")
    showFooter: true
    showAccept: false
    width: 540

    ColumnLayout {
        width: parent.width
        spacing: 10

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(190, Math.max(56, App.accounts.length * 56))
            spacing: 4
            clip: true
            model: App.accounts
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 50
                radius: 9
                color: modelData.active ? App.colorAccentSoft : App.colorSurfaceAlt
                border.width: 1
                border.color: modelData.active ? App.colorAccent : App.colorBorder
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Rectangle {
                        width: 32; height: 32; radius: 16; clip: true
                        color: App.colorSurface
                        Image {
                            anchors.fill: parent
                            source: modelData.provider === "elyby" ? App.avatarSource : "qrc:/assets/steve.png"
                            fillMode: Image.PreserveAspectCrop
                            smooth: false
                            sourceSize.width: 64
                            sourceSize.height: 64
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: modelData.name
                            color: App.colorText
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(12)
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: (modelData.provider === "elyby" ? control.t("elyby") : control.t("offline"))
                                  + (modelData.active ? " · " + control.t("active") : "")
                            color: modelData.active ? App.colorAccent : App.colorTextDim
                            font.family: App.fontFamily
                            font.pixelSize: App.fs(9)
                        }
                    }
                    NobleButton {
                        visible: !modelData.active
                        variant: "secondary"
                        text: control.t("select")
                        minHeight: 28; fontSize: 9
                        onClicked: App.selectAccount(modelData.id)
                    }
                    NobleButton {
                        visible: modelData.provider === "elyby"
                        variant: "icon"; iconName: "refresh"; minHeight: 28; implicitWidth: 28
                        onClicked: App.refreshAccount(modelData.id)
                    }
                    NobleButton {
                        variant: "icon"; iconName: "trash"; minHeight: 28; implicitWidth: 28
                        onClicked: App.removeAccount(modelData.id)
                    }
                }
            }
        }

        Text {
            visible: App.accounts.length === 0
            Layout.fillWidth: true
            text: control.t("no_account")
            color: App.colorTextDim
            font.family: App.fontFamily
            font.pixelSize: App.fs(11)
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: App.colorBorder }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [{ id: "password", key: "ely_password" }, { id: "oauth", key: "ely_oauth" }, { id: "offline", key: "offline" }]
                delegate: Rectangle {
                    required property var modelData
                    implicitHeight: 32; implicitWidth: modeLabel.implicitWidth + 24
                    radius: 8
                    color: control.mode === modelData.id ? App.colorAccent : "transparent"
                    border.width: 1
                    border.color: control.mode === modelData.id ? App.colorAccent : App.colorBorder
                    Text {
                        id: modeLabel
                        anchors.centerIn: parent
                        text: control.t(modelData.key)
                        color: control.mode === modelData.id ? "#ffffff" : App.colorTextDim
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(10)
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: control.mode = modelData.id }
                }
            }
            Item { Layout.fillWidth: true }
            Text {
                text: App.oauthStatus
                visible: App.oauthStatus.length > 0
                color: App.colorAccent
                font.family: App.fontFamily
                font.pixelSize: App.fs(9)
            }
            BusyIndicator { visible: App.accountsBusy; running: App.accountsBusy; width: 22; height: 22 }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: control.mode === "password"
            NobleTextField {
                id: usernameField
                Layout.fillWidth: true
                placeholderText: control.t("email")
            }
            NobleTextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: control.t("password")
                echoMode: TextInput.Password
                onAccepted: App.addElyByPassword(usernameField.text, passwordField.text)
            }
            NobleButton {
                Layout.fillWidth: true
                text: App.accountsBusy ? control.t("logging_in") : control.t("login")
                iconName: "play"
                busy: App.accountsBusy
                onClicked: App.addElyByPassword(usernameField.text, passwordField.text)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: control.mode === "oauth"
            Text {
                Layout.fillWidth: true
                text: control.t("oauth_hint")
                color: App.colorTextDim
                font.family: App.fontFamily
                font.pixelSize: App.fs(10)
                wrapMode: Text.WordWrap
            }
            NobleButton {
                Layout.fillWidth: true
                text: control.t("ely_oauth")
                iconName: "external"
                busy: App.accountsBusy
                onClicked: App.addElyByOAuth()
            }
            NobleButton {
                Layout.fillWidth: true
                visible: App.accountsBusy
                variant: "secondary"
                text: control.t("cancel")
                onClicked: App.cancelOAuth()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: control.mode === "offline"
            NobleTextField {
                id: offlineField
                Layout.fillWidth: true
                placeholderText: control.t("offline_nick_hint")
                onAccepted: App.addOfflineAccount(text)
            }
            NobleButton {
                Layout.fillWidth: true
                text: control.t("add_offline")
                iconName: "plus"
                onClicked: App.addOfflineAccount(offlineField.text)
            }
        }

        Text {
            Layout.fillWidth: true
            visible: App.envMissing.length > 0
            text: App.envMissing.length > 0 ? "Ely.by OAuth: " + App.envMissing : ""
            color: App.colorWarn
            font.family: App.fontFamily
            font.pixelSize: App.fs(9)
            wrapMode: Text.WordWrap
        }
    }
}
