/////////////////////////////////////////////////////////
// File: MarkdownDocumentPopup.qml
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Popup for displaying markdown documents
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property string p_title: qsTr("Document")
    property string p_text: ""
    property string p_emptyText: qsTr("No document available.")
    property Component p_extraComponentContent: null
    property string p_documentTitle: ""
    property bool p_adaptiveHeight: false
    property int p_minPopupHeight: 360
    property int p_maxPopupHeight: 720

    // Internals _____________________________________________
    readonly property int edgeMargin: 48
    readonly property int maxPopupWidth: 820
    readonly property int adaptiveDocumentHeight: Math.max(120, Math.min(420, id_documentText.contentHeight + id_documentText.topPadding + id_documentText.bottomPadding))
    readonly property int adaptivePopupHeight: Math.max(p_minPopupHeight, Math.min(p_maxPopupHeight, id_content.implicitHeight + topPadding + bottomPadding))

    parent: Overlay.overlay
    width: Math.min(maxPopupWidth, parent ? parent.width - edgeMargin : maxPopupWidth)
    height: Math.min(p_adaptiveHeight ? adaptivePopupHeight : p_maxPopupHeight, parent ? parent.height - edgeMargin : p_maxPopupHeight)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    // Replace popup content before showing requested document
    function openDocument(documentTitle, documentText) {
        p_title = documentTitle || qsTr("Document")
        p_text = documentText || ""
        id_documentText.text = p_text === "" ? p_emptyText : p_text
        open()
    }

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.close()
    }

    Overlay.modal: Rectangle {
        color: Themes.confirmationPopup.colors.overlay
    }

    background: Rectangle {
        radius: 8
        color: Themes.confirmationPopup.colors.background
        border.width: 1
        border.color: Themes.confirmationPopup.colors.border
    }

    contentItem: ColumnLayout {
        id: id_content

        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: id_root.p_title
                color: Themes.confirmationPopup.colors.titleText
                font.pixelSize: Themes.confirmationPopup.fontSizes.title
                font.bold: true
                elide: Text.ElideRight
            }

            CustomButton {
                text: qsTr("Close")
                onClicked: id_root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Themes.confirmationPopup.colors.border
        }

        // Optional content shown between popup heading and document
        Loader {
            Layout.fillWidth: true
            active: id_root.p_extraComponentContent !== null
            visible: active
            sourceComponent: id_root.p_extraComponentContent
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_documentTitle
            visible: text.length > 0
            color: Themes.confirmationPopup.colors.titleText
            font.pixelSize: Themes.confirmationPopup.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        ScrollView {
            id: id_documentScrollView

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: id_root.p_adaptiveHeight ? id_root.adaptiveDocumentHeight : -1
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: CustomScrollBar {
                id: id_verticalScrollBar

                policy: ScrollBar.AsNeeded
            }

            Component.onCompleted: {
                contentItem.boundsBehavior = Flickable.StopAtBounds
                id_verticalScrollBar.parent = id_documentScrollView
                id_verticalScrollBar.anchors.top = id_documentScrollView.top
                id_verticalScrollBar.anchors.bottom = id_documentScrollView.bottom
                id_verticalScrollBar.anchors.right = id_documentScrollView.right
            }

            TextArea {
                id: id_documentText

                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WordWrap
                textFormat: TextEdit.MarkdownText
                color: Themes.confirmationPopup.colors.bodyText
                selectedTextColor: Themes.confirmationPopup.colors.buttonText
                selectionColor: Themes.confirmationPopup.colors.buttonBorderHover
                font.pixelSize: Themes.confirmationPopup.fontSizes.body

                background: Item {}
            }
        }
    }
}
