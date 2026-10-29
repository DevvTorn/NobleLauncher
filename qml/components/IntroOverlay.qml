import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    property bool playing: false
    property bool videoFailed: false
    visible: opacity > 0.001
    opacity: playing ? 1 : 0
    z: 1000
    enabled: playing

    Behavior on opacity { NumberAnimation { duration: App.animationsEnabled ? 460 : 1; easing.type: Easing.InOutQuad } }

    function play() {
        playing = true
        videoFailed = false
        player.stop()
        if (App.reducedMotion) {
            videoFailed = true
            reducedTimer.restart()
            return
        }
        if (player.source.toString().length === 0)
            player.source = "qrc:/assets/logo.mp4"
        player.play()
        guardTimer.restart()
    }

    function finish() {
        if (!playing)
            return
        player.pause()
        playing = false
        releaseTimer.restart()
    }

    function useFallback() {
        if (videoFailed)
            return
        videoFailed = true
        player.stop()
        if (playing)
            fallbackTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        color: "#050507"
    }

    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: !root.videoFailed && root.playing
    }

    MediaPlayer {
        id: player
        source: ""
        videoOutput: video
        autoPlay: false
        audioOutput: AudioOutput {
            muted: !App.settings.introSound
            volume: 0.8
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia)
                root.finish()
            else if (mediaStatus === MediaPlayer.InvalidMedia)
                root.useFallback()
        }
        onErrorOccurred: (error, errorString) => root.useFallback()
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: root.videoFailed
        opacity: root.videoFailed ? 1 : 0

        Image {
            source: "qrc:/assets/logo.png"
            sourceSize.width: 124
            sourceSize.height: 124
            Layout.preferredWidth: 112
            Layout.preferredHeight: 112
            Layout.alignment: Qt.AlignHCenter
            NumberAnimation on scale {
                running: root.videoFailed && App.animationsEnabled
                loops: Animation.Infinite
                from: 0.94
                to: 1.02
                duration: 1400
                easing.type: Easing.InOutSine
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "NOBLE"
            color: "#f4f4f7"
            font.family: App.fontFamily
            font.pixelSize: App.fs(26)
            font.weight: Font.Black
            font.letterSpacing: 6
        }
    }

    Timer {
        id: guardTimer
        interval: 12000
        onTriggered: root.finish()
    }

    Timer {
        id: fallbackTimer
        interval: 1800
        onTriggered: root.finish()
    }

    Timer {
        id: reducedTimer
        interval: 650
        onTriggered: root.finish()
    }

    Timer {
        id: releaseTimer
        interval: 700
        onTriggered: {
            player.stop()
            player.source = ""
        }
    }
}
