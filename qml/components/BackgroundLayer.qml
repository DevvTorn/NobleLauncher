import QtQuick
import QtQuick.Effects

Item {
    id: root
    anchors.fill: parent
    clip: true

    Rectangle {
        anchors.fill: parent
        color: App.colorBg
    }

    Image {
        id: background
        anchors.fill: parent
        source: App.backgroundSource
        visible: false
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        cache: true
    }

    MultiEffect {
        anchors.fill: parent
        source: background
        visible: background.status === Image.Ready && App.backgroundSource.length > 0
        blurEnabled: App.settings.backgroundBlur > 0
        blur: Math.min(1.0, App.settings.backgroundBlur / 80)
        blurMax: 48
        saturation: App.settings.backgroundSaturation / 100.0
        brightness: -App.settings.backgroundDarken / 450.0
    }

    Rectangle {
        anchors.fill: parent
        visible: background.status === Image.Ready && App.backgroundSource.length > 0
        color: "#000000"
        opacity: Math.min(0.85, App.settings.backgroundDarken / 130)
    }

    Rectangle {
        visible: App.backgroundSource.length === 0
        width: parent.width * 0.62
        height: width
        radius: width / 2
        x: -width * 0.25
        y: -height * 0.35
        color: App.colorAccent
        opacity: App.settings.theme === "light" ? 0.05 : 0.09
        layer.enabled: true
        layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 96 }
    }

    Rectangle {
        visible: App.backgroundSource.length === 0
        width: parent.width * 0.55
        height: width
        radius: width / 2
        x: parent.width - width * 0.7
        y: parent.height - height * 0.65
        color: App.colorAccent
        opacity: App.settings.theme === "light" ? 0.04 : 0.06
        layer.enabled: true
        layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 96 }
    }


}
