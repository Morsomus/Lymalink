/////////////////////////////////////////////////////////
// File: CustomTooltip.qml
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom Tooltip
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Window

Popup {
    id: id_root

    // Public ________________________________________________
    property bool p_active: false
    property bool p_alwaysVisible: false
    property string p_text: ""
    property int p_delay: 300

    // Internals _____________________________________________
    readonly property int edgeMargin: 8
    readonly property int targetGap: 8
    readonly property int overlayZ: 1000
    readonly property int maxTextWidth: 280

    closePolicy: Popup.NoAutoClose
    padding: 8
    width: boundedWidth()
    x: boundedX()
    y: boundedY()
    z: overlayZ

    function shouldShow(): bool {
        return p_active && p_text !== "" && ctxSettings.showTooltips ||  p_active && p_text !== "" && p_alwaysVisible
    }

    function windowItem() {
        const window = parent ? parent.Window.window : null
        return window ? window.contentItem : null
    }

    function parentPosition() {
        const item = windowItem()
        return parent && item ? parent.mapToItem(item, 0, 0) : Qt.point(0, 0)
    }

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(value, maximum))
    }

    function boundedWidth() {
        const item = windowItem()
        const maxWindowWidth = item ? Math.max(0, item.width - edgeMargin * 2 - leftPadding - rightPadding) : maxTextWidth
        const textWidth = Math.ceil(id_textMetrics.advanceWidth)
        const contentWidth = Math.min(textWidth, maxTextWidth, maxWindowWidth)
        return contentWidth + leftPadding + rightPadding
    }

    function boundedX() {
        const item = windowItem()
        if (!parent || !item) {
            return 0
        }

        const parentPos = parentPosition()
        const preferred = Math.round((parent.width - width) / 2)
        const minimum = edgeMargin - parentPos.x
        const maximum = item.width - width - edgeMargin - parentPos.x
        return Math.round(clamp(preferred, minimum, Math.max(minimum, maximum)))
    }

    function boundedY() {
        const item = windowItem()
        if (!parent || !item) {
            return 0
        }

        const parentPos = parentPosition()
        const above   = -height - targetGap
        const below   = parent.height + targetGap
        const minimum = edgeMargin - parentPos.y
        const maximum = item.height - height - edgeMargin - parentPos.y
        if (parentPos.y + above >= edgeMargin) {
            return Math.round(above)
        }
        if (parentPos.y + below + height <= item.height - edgeMargin) {
            return Math.round(below)
        }
            
        return Math.round(clamp(above, minimum, Math.max(minimum, maximum)))
    }

    function update() {
        if (shouldShow()) {
            id_showTimer.restart()
        } else {
            id_showTimer.stop()
            close()
        }
    }

    Connections {
        target: ctxSettings
        function onSignalConfigChanged() {
            id_root.update()
        }
    }

    onP_activeChanged: {
        update()
    }

    onP_textChanged: {
        update()
    }

    Timer {
        id: id_showTimer
        interval: id_root.p_delay
        repeat: false
        onTriggered: if (id_root.shouldShow()) id_root.open()
    }

    TextMetrics {
        id: id_textMetrics

        text: id_root.p_text
        font.pixelSize: Themes.general.fontSizes.tooltip
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    contentItem: Label {
        text: id_root.p_text
        color: Themes.general.colors.titleText
        font.pixelSize: Themes.general.fontSizes.tooltip
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        maximumLineCount: 4
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: Themes.general.colors.tooltipBackground
        radius: 6
        border.color: Themes.general.colors.border
        border.width: 2
    }
}
