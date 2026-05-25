/////////////////////////////////////////////////////////
// File: ConfirmationPopup.qml
// Date: 2026-05-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Multipurpose confirmation popup
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property string p_title: qsTr("Confirm")
    property string p_description: ""
    property string p_cancelText: qsTr("Cancel")
    property string p_confirmText: qsTr("Confirm")
    property int p_popupWidth: 340
    property bool p_verificationMode: false
    property bool p_pathSelectionMode: false
    property bool p_pathSelectionFolder: false
    property string p_pathDialogTitle: qsTr("Select Location")
    property string p_pathPlaceholderText: qsTr("Select path")
    property var p_pathNameFilters: []
    property bool p_confirmDanger: false

    // Internals _____________________________________________
    readonly property bool verificationValid: !p_verificationMode || (id_verificationInput.text.length >= 6
        && id_verificationInput.text === id_verificationConfirmInput.text)
    readonly property bool pathSelectionValid: !p_pathSelectionMode || id_pathInput.text.length > 0
    readonly property bool canConfirm: verificationValid && pathSelectionValid

    signal canceled()
    signal confirmed(string verificationText)

    parent: Overlay.overlay
    width: Math.min(p_popupWidth, parent ? parent.width - 48 : p_popupWidth)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    onOpened: {
        if (p_verificationMode) {
            id_verificationInput.forceActiveFocus()
        }
    }

    onClosed: {
        id_verificationInput.text = ""
        id_verificationConfirmInput.text = ""
        id_pathInput.text = ""
    }

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.cancel()
    }

    Overlay.modal: Rectangle {
        color: Themes.confirmationPopup.colors.overlay
    }

    function cancel() {
        id_root.canceled()
        id_root.close()
    }

    function confirm() {
        if (!id_root.canConfirm) {
            return
        }

        id_root.confirmed(id_root.confirmationValue())
        id_root.close()
    }

    function confirmationValue() {
        if (id_root.p_pathSelectionMode) {
            return id_pathInput.text
        }

        return id_root.p_verificationMode ? id_verificationInput.text : ""
    }

    function fileUrlToPath(fileUrl) {
        return decodeURIComponent(fileUrl.toString().replace("file://", ""))
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_ActionButton: Button {
        id: id_button

        property bool danger: false

        Layout.fillWidth: true
        implicitHeight: 40

        contentItem: Label {
            text: id_button.text
            color: id_button.danger
                ? Themes.confirmationPopup.colors.dangerText
                : Themes.confirmationPopup.colors.buttonText
            font.pixelSize: Themes.confirmationPopup.fontSizes.button
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: id_button.danger
                ? id_button.down
                    ? Themes.confirmationPopup.colors.dangerBackgroundPressed
                    : id_button.hovered
                        ? Themes.confirmationPopup.colors.dangerBackgroundHover
                        : Themes.confirmationPopup.colors.dangerBackground
                : id_button.down
                    ? Themes.confirmationPopup.colors.buttonBackgroundPressed
                    : id_button.hovered
                        ? Themes.confirmationPopup.colors.buttonBackgroundHover
                        : Themes.confirmationPopup.colors.buttonBackground
            border.width: 1
            border.color: id_button.danger
                ? id_button.hovered
                    ? Themes.confirmationPopup.colors.dangerBorderHover
                    : Themes.confirmationPopup.colors.dangerBorder
                : id_button.hovered
                    ? Themes.confirmationPopup.colors.buttonBorderHover
                    : Themes.confirmationPopup.colors.buttonBorder

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    FileDialog {
        id: id_pathFileDialog

        title: id_root.p_pathDialogTitle
        fileMode: FileDialog.OpenFile
        nameFilters: id_root.p_pathNameFilters
        onAccepted: id_pathInput.text = id_root.fileUrlToPath(selectedFile)
    }

    FolderDialog {
        id: id_pathFolderDialog

        title: id_root.p_pathDialogTitle
        onAccepted: id_pathInput.text = id_root.fileUrlToPath(selectedFolder)
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

        Label {
            Layout.fillWidth: true
            text: id_root.p_title
            color: Themes.confirmationPopup.colors.titleText
            font.pixelSize: Themes.confirmationPopup.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_description
            color: Themes.confirmationPopup.colors.bodyText
            font.pixelSize: Themes.confirmationPopup.fontSizes.body
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: id_root.p_verificationMode

            TextField {
                id: id_verificationInput

                Layout.fillWidth: true
                placeholderText: qsTr("Passcode")
                echoMode: TextInput.Password
                selectByMouse: true
                color: Themes.confirmationPopup.colors.buttonText
                placeholderTextColor: Themes.confirmationPopup.colors.bodyText
                font.pixelSize: Themes.confirmationPopup.fontSizes.button

                background: Rectangle {
                    radius: 6
                    color: Themes.confirmationPopup.colors.buttonBackground
                    border.width: 1
                    border.color: id_verificationInput.activeFocus
                        ? Themes.confirmationPopup.colors.buttonBorderHover
                        : Themes.confirmationPopup.colors.buttonBorder
                }
            }

            TextField {
                id: id_verificationConfirmInput

                Layout.fillWidth: true
                placeholderText: qsTr("Confirm passcode")
                echoMode: TextInput.Password
                selectByMouse: true
                color: Themes.confirmationPopup.colors.buttonText
                placeholderTextColor: Themes.confirmationPopup.colors.bodyText
                font.pixelSize: Themes.confirmationPopup.fontSizes.button

                background: Rectangle {
                    radius: 6
                    color: Themes.confirmationPopup.colors.buttonBackground
                    border.width: 1
                    border.color: id_verificationConfirmInput.activeFocus
                        ? Themes.confirmationPopup.colors.buttonBorderHover
                        : Themes.confirmationPopup.colors.buttonBorder
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Minimum 6 characters. Both fields must match.")
                color: Themes.confirmationPopup.colors.bodyText
                font.pixelSize: Themes.confirmationPopup.fontSizes.body
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: id_root.p_pathSelectionMode

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: id_pathInput

                    Layout.fillWidth: true
                    readOnly: true
                    selectByMouse: true
                    placeholderText: id_root.p_pathPlaceholderText
                    color: Themes.confirmationPopup.colors.buttonText
                    placeholderTextColor: Themes.confirmationPopup.colors.bodyText
                    font.pixelSize: Themes.confirmationPopup.fontSizes.button

                    background: Rectangle {
                        radius: 6
                        color: Themes.confirmationPopup.colors.buttonBackground
                        border.width: 1
                        border.color: id_pathInput.activeFocus
                            ? Themes.confirmationPopup.colors.buttonBorderHover
                            : Themes.confirmationPopup.colors.buttonBorder
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: id_root.p_pathSelectionFolder
                            ? id_pathFolderDialog.open()
                            : id_pathFileDialog.open()
                    }
                }

                C_ActionButton {
                    Layout.fillWidth: false
                    Layout.preferredWidth: 92
                    text: qsTr("Browse")
                    onClicked: id_root.p_pathSelectionFolder
                        ? id_pathFolderDialog.open()
                        : id_pathFileDialog.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            C_ActionButton {
                id: id_cancelButton

                text: id_root.p_cancelText
                onClicked: id_root.cancel()
            }

            C_ActionButton {
                id: id_confirmButton

                text: id_root.p_confirmText
                danger: id_root.p_confirmDanger
                enabled: id_root.canConfirm
                opacity: enabled ? 1.0 : 0.45
                onClicked: id_root.confirm()
            }
        }
    }
}
