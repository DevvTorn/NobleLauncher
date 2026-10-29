import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property int maxToasts: 4

    ListModel { id: toasts }

    function push(message, type) {
        if (!message || message.indexOf("__") === 0)
            return
        while (toasts.count >= root.maxToasts)
            toasts.remove(0)
        toasts.append({ message: message, type: type || "info", life: type === "error" ? 5200 : 3600 })
    }

    Connections {
        target: App
        function onToast(message, type) { root.push(message, type) }
        function onAchievementUnlocked(achievement) {
            var lang = App.lang
            var title = lang === "en" ? achievement.titleEn : achievement.titleRu
            root.push("Достижение: «" + title + "» (+" + achievement.points + ")", "achievement")
        }
    }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        spacing: 10

        Repeater {
            model: toasts
            delegate: Rectangle {
                id: toast
                required property int index
                required property string message
                required property string type
                required property int life
                width: Math.min(420, Math.max(260, label.implicitWidth + 54))
                height: Math.max(50, label.implicitHeight + 22)
                radius: 12
                color: type === "error" ? Qt.darker(App.colorError, 2.4)
                     : type === "success" || type === "achievement" ? Qt.darker(App.colorSuccess, 2.6)
                     : type === "warn" ? Qt.darker(App.colorWarn, 3.2)
                     : App.colorSurface
                border.width: 1
                border.color: type === "error" ? App.colorError
                             : type === "success" || type === "achievement" ? App.colorSuccess
                             : type === "warn" ? App.colorWarn
                             : App.colorBorder
                opacity: 0
                x: 30
                scale: 0.96

                Component.onCompleted: {
                    enterAnimation.start()
                    lifeTimer.interval = life
                    lifeTimer.start()
                }

                NumberAnimation {
                    id: enterAnimation
                    target: toast
                    properties: "opacity,x,scale"
                    to: 1
                    from: 0
                    duration: App.animationsEnabled ? 220 : 1
                    easing.type: Easing.OutCubic
                }
                Timer {
                    id: lifeTimer
                    onTriggered: exitAnimation.start()
                }
                SequentialAnimation {
                    id: exitAnimation
                    NumberAnimation { target: toast; property: "opacity"; to: 0; duration: App.animationsEnabled ? 180 : 1 }
                    ScriptAction { script: toasts.remove(toast.index) }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10
                    Icon {
                        name: toast.type === "achievement" ? "trophy"
                            : toast.type === "success" ? "check"
                            : toast.type === "error" ? "alert"
                            : toast.type === "warn" ? "alert" : "console"
                        size: 20
                        color: toast.type === "error" ? App.colorError
                             : toast.type === "success" || toast.type === "achievement" ? App.colorSuccess
                             : toast.type === "warn" ? App.colorWarn : App.colorAccent
                    }
                    Text {
                        id: label
                        Layout.fillWidth: true
                        text: toast.message
                        color: toast.type === "info" || toast.type === "achievement" ? App.colorText : "#ffffff"
                        font.family: App.fontFamily
                        font.pixelSize: App.fs(13)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: exitAnimation.start()
                }
            }
        }
    }
}
