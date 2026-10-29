import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import QtQuick3D
import Noble 1.0
import "../I18n.js" as I18n

Item {
    id: root
    property bool showAngleControls: true
    property string animationMode: "static"
    property bool slim: false

    property real manualYaw: 0
    property real manualPitch: 0
    property real zoom: 0
    property real spinAngle: 0
    property real phase: 0
    Behavior on zoom { NumberAnimation { duration: App.animationsEnabled ? 110 : 1; easing.type: Easing.OutCubic } }
    property real yawVelocity: 0
    property real pitchVelocity: 0
    property real lastDragTime: 0

    readonly property real armSwing: {
        if (animationMode === "walk")
            return Math.sin(phase * Math.PI * 2) * 30
        if (animationMode === "idle")
            return Math.sin(phase * Math.PI * 2) * 2.5
        return 0
    }
    readonly property real legSwing: animationMode === "walk" ? -armSwing : 0
    readonly property real headBob: animationMode === "idle" ? Math.sin(phase * Math.PI * 2) * 1.2 : 0

    function resetCamera() {
        manualYaw = 0
        manualPitch = 0
        zoom = 0
        yawVelocity = 0
        pitchVelocity = 0
    }

    function setView(degrees) {
        manualYaw = degrees
        manualPitch = 0
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: App.settings.theme === "light" ? "#ececf1" : "#141418"
        border.width: 1
        border.color: App.colorBorder
        clip: true

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.9
            height: width
            radius: width / 2
            color: App.colorAccent
            opacity: 0.09
            layer.enabled: true
            layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 96 }
        }

        View3D {
            id: view
            anchors.fill: parent

            environment: SceneEnvironment {
                clearColor: App.settings.theme === "light" ? "#ececf1" : "#141418"
                backgroundMode: SceneEnvironment.Color
                antialiasingMode: SceneEnvironment.MSAA
                antialiasingQuality: SceneEnvironment.High
            }

            PerspectiveCamera {
                id: camera
                position: Qt.vector3d(0, 17.5, 84 - root.zoom)
                eulerRotation.x: -2 + root.manualPitch * 0.35
                fieldOfView: 30
            }

            DirectionalLight {
                eulerRotation.x: -32
                eulerRotation.y: -38
                brightness: 1.35
            }
            DirectionalLight {
                eulerRotation.x: 15
                eulerRotation.y: 145
                brightness: 0.55
                color: "#ffdfe2"
            }

            Node {
                id: modelRoot
                eulerRotation.y: root.manualYaw + root.spinAngle
                eulerRotation.x: root.manualPitch

                Node {
                    position: Qt.vector3d(0, 12, 0)
                    Model {
                        geometry: SkinMesh { part: "body"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(0, 6, 0)
                    }
                }

                Node {
                    id: headPivot
                    position: Qt.vector3d(0, 24, 0)
                    Model {
                        geometry: SkinMesh { part: "head"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(0, 4, 0)
                        eulerRotation.y: root.headBob
                    }
                }

                Node {
                    id: rightArmPivot
                    position: Qt.vector3d(-4, 24, 0)
                    eulerRotation.x: root.armSwing
                    Model {
                        geometry: SkinMesh { part: "rightArm"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(root.slim ? -1.5 : -2, -6, 0)
                    }
                }
                Node {
                    id: leftArmPivot
                    position: Qt.vector3d(4, 24, 0)
                    eulerRotation.x: -root.armSwing
                    Model {
                        geometry: SkinMesh { part: "leftArm"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(root.slim ? 1.5 : 2, -6, 0)
                    }
                }
                Node {
                    id: rightLegPivot
                    position: Qt.vector3d(-2, 12, 0)
                    eulerRotation.x: root.legSwing
                    Model {
                        geometry: SkinMesh { part: "rightLeg"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(0, -6, 0)
                    }
                }
                Node {
                    id: leftLegPivot
                    position: Qt.vector3d(2, 12, 0)
                    eulerRotation.x: -root.legSwing
                    Model {
                        geometry: SkinMesh { part: "leftLeg"; slim: root.slim }
                        materials: skinMaterial
                        position: Qt.vector3d(0, -6, 0)
                    }
                }
            }

            PrincipledMaterial {
                id: skinMaterial
                baseColorMap: Texture {
                    source: App.skinSource
                    magFilter: Texture.Nearest
                    minFilter: Texture.Nearest
                    generateMipmaps: false
                    flipV: true
                }
                roughness: 0.92
                metalness: 0.0
                specularAmount: 0.08
                cullMode: Material.BackFaceCulling
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real lastX: 0
                property real lastY: 0
                onPressed: (mouse) => {
                    lastX = mouse.x
                    lastY = mouse.y
                    root.lastDragTime = Date.now()
                    root.yawVelocity = 0
                    root.pitchVelocity = 0
                    inertiaTimer.stop()
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const now = Date.now()
                    const dt = Math.max(1, now - root.lastDragTime) / 1000
                    const yawDelta = (mouse.x - lastX) * 0.65
                    const pitchDelta = (mouse.y - lastY) * 0.35
                    root.manualYaw += yawDelta
                    root.manualPitch = Math.max(-26, Math.min(26, root.manualPitch + pitchDelta))
                    root.yawVelocity = yawDelta / dt
                    root.pitchVelocity = pitchDelta / dt
                    root.lastDragTime = now
                    lastX = mouse.x
                    lastY = mouse.y
                }
                onReleased: {
                    if (Math.abs(root.yawVelocity) > 0.5 || Math.abs(root.pitchVelocity) > 0.5)
                        inertiaTimer.restart()
                }
            }

            WheelHandler {
                onWheel: (event) => {
                    root.zoom = Math.max(-8, Math.min(42, root.zoom + event.angleDelta.y / 120 * 4))
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.35)
            width: skinLabel.implicitWidth + 18
            height: 28
            Text {
                id: skinLabel
                anchors.centerIn: parent
                text: App.skinName + (App.selectedAccount.provider === "elyby" ? " · Ely.by" : "")
                color: "#f4f4f7"
                font.family: App.fontFamily
                font.pixelSize: App.fs(11)
            }
        }

        Timer {
            interval: 33
            repeat: true
            running: App.animationsEnabled && Window.active
                     && (root.animationMode === "walk" || root.animationMode === "spin")
            onTriggered: {
                if (root.animationMode === "walk")
                    root.phase = (root.phase + 0.033 / 0.9) % 1
                else
                    root.spinAngle = (root.spinAngle + 360 * 0.033 / 7.0) % 360
            }
        }
    }

    Timer {
        id: inertiaTimer
        interval: 33
        repeat: true
        onTriggered: {
            if (Math.abs(root.yawVelocity) < 0.4 && Math.abs(root.pitchVelocity) < 0.4) {
                stop()
                return
            }
            root.manualYaw += root.yawVelocity * 0.033
            root.manualPitch = Math.max(-26, Math.min(26, root.manualPitch + root.pitchVelocity * 0.033))
            root.yawVelocity *= 0.86
            root.pitchVelocity *= 0.86
        }
    }

    RowLayout {
        visible: root.showAngleControls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        spacing: 4

        Rectangle {
            radius: 10
            color: Qt.rgba(0, 0, 0, 0.38)
            implicitHeight: 38
            implicitWidth: controlsRow.implicitWidth + 8

            RowLayout {
                id: controlsRow
                anchors.centerIn: parent
                spacing: 2
                NobleButton {
                    variant: "icon"; iconName: "rotate"; minHeight: 34; implicitWidth: 34
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_reset")
                    onClicked: root.resetCamera()
                }
                NobleButton {
                    variant: "icon"; iconName: "chevron-left"; minHeight: 34; implicitWidth: 34
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_left")
                    onClicked: root.setView(90)
                }
                NobleButton {
                    variant: "icon"; iconName: "eye"; minHeight: 34; implicitWidth: 34
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_front")
                    onClicked: root.setView(0)
                }
                NobleButton {
                    variant: "icon"; iconName: "chevron-right"; minHeight: 34; implicitWidth: 34
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_right")
                    onClicked: root.setView(-90)
                }
                NobleButton {
                    variant: "icon"; iconName: "walk"; minHeight: 34; implicitWidth: 34
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_walk")
                    onClicked: root.animationMode = root.animationMode === "walk" ? "static" : "walk"
                }
                NobleButton {
                    variant: "icon"; iconName: "rotate"; minHeight: 34; implicitWidth: 34
                    property int spinTick: 0
                    ToolTip.visible: hovered; ToolTip.text: i18nFront.t("skin_spin")
                    onClicked: { root.spinAngle = 0; root.animationMode = root.animationMode === "spin" ? "static" : "spin" }
                }
            }
        }
    }

    QtObject {
        id: i18nFront
        function t(key) { return I18n.t(App.lang, key) }
    }
}
