/////////////////////////////////////////////////////////
// File: ErrorPopup.qml
// Date: 2026-05-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Global error notification popup
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property string p_title: qsTr("Error")
    property string p_message: ""

    // Internals _____________________________________________
    readonly property int edgeMargin: 24
    readonly property int maxPopupWidth: 420
    readonly property int overlayZ: 2000

    parent: Overlay.overlay
    width: Math.min(maxPopupWidth, Math.max(260, parent ? parent.width - edgeMargin * 2 : maxPopupWidth))
    height: id_content.implicitHeight + topPadding + bottomPadding
    x: parent ? Math.round(parent.width - width - edgeMargin) : 0
    y: parent ? Math.round(parent.height - height - edgeMargin) : 0
    z: overlayZ
    modal: false
    focus: false
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function showError(errorTitle, errorMessage) {
        p_title = errorTitle || qsTr("Error")
        p_message = errorMessage || qsTr("An error occurred.")
        open()
        // id_autoCloseTimer.restart()
    }

    // Timer {
    //     id: id_autoCloseTimer
    //     interval: 9000
    //     repeat: false
    //     onTriggered: id_root.close()
    // }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    background: Rectangle {
        radius: 8
        color: Themes.errorPopup.colors.background
        border.width: 1
        border.color: Themes.errorPopup.colors.border
    }

    contentItem: ColumnLayout {
        id: id_content

        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: id_root.p_title
                color: Themes.errorPopup.colors.titleText
                font.pixelSize: Themes.errorPopup.fontSizes.title
                font.bold: true
                elide: Text.ElideRight
            }

            Button {
                text: qsTr("Close")
                onClicked: id_root.close()
            }
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_message
            color: Themes.errorPopup.colors.bodyText
            font.pixelSize: Themes.errorPopup.fontSizes.body
            wrapMode: Text.WordWrap
        }
    }
}
