import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 40
    implicitWidth: 220
    property string placeholderText: ""
    leftPadding: 12
    rightPadding: 36
    topPadding: 0
    bottomPadding: 0
    font.family: App.fontFamily
    font.pixelSize: App.fs(14)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: selectedText.length > 0 ? selectedText : placeholderText
    readonly property string selectedText: currentIndex >= 0 && model && model[currentIndex] !== undefined
                                           ? modelText(model[currentIndex]) : ""

    function modelText(item) {
        if (item === undefined || item === null)
            return ""
        if (typeof item === "string")
            return item
        if (typeof item === "number")
            return String(item)
        if (control.textRole.length > 0 && item[control.textRole] !== undefined)
            return String(item[control.textRole])
        if (item.name !== undefined)
            return String(item.name)
        if (item.label !== undefined)
            return String(item.label)
        return ""
    }

    contentItem: Text {
        width: control.availableWidth
        height: control.availableHeight
        text: control.selectedText.length > 0 ? control.selectedText
              : (control.displayText.length > 0 ? control.displayText : control.placeholderText)
        font: control.font
        color: control.enabled ? App.colorText : App.colorTextDim
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.55
    }

    indicator: Icon {
        x: control.width - width - 12
        y: (control.height - height) / 2
        name: "chevron-down"
        size: 16
        color: control.enabled ? App.colorTextDim : App.colorBorder
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 140 } }
    }

    background: Rectangle {
        radius: 10
        color: control.enabled
               ? (control.hovered || control.popup.visible ? App.colorSurfaceAlt : App.colorSurface)
               : App.colorSurface
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        border.color: control.activeFocus || control.popup.visible ? App.colorAccent : App.colorBorder
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    ToolTip.visible: hovered && contentItem.text.length > 0
    ToolTip.text: contentItem.text
    ToolTip.delay: 700

    delegate: ItemDelegate {
        id: delegateItem
        width: control.width - 12
        height: 36
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.modelText(modelData !== undefined ? modelData : model)
            color: delegateItem.highlighted ? "#ffffff" : App.colorText
            font.family: App.fontFamily
            font.pixelSize: App.fs(13)
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            width: delegateItem.availableWidth
        }
        background: Rectangle {
            radius: 8
            color: delegateItem.highlighted ? App.colorAccent
                 : (delegateItem.hovered ? App.colorSurfaceAlt : "transparent")
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 320)
        padding: 6
        background: Rectangle {
            radius: 12
            color: App.colorSurface
            border.width: 1
            border.color: App.colorBorder
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
