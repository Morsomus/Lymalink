/////////////////////////////////////////////////////////
// File: TargetAchievementEditPopup.qml
// Date: 2026-05-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Popup for manual achievement lock/unlock
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property int p_appId: 0
    property string p_achievementKey: ""
    property string p_achievementName: ""
    property string p_achievementDescription: ""
    property bool p_currentlyUnlocked: false

    signal confirmed(int appId, string achievementKey, bool unlock, var unlockTimestamp)

    // Internals _____________________________________________
    readonly property bool unlockAction: !p_currentlyUnlocked
    readonly property bool validDateTime: !unlockAction || isDateTimeValid()

    parent: Overlay.overlay
    width: Math.min(420, parent ? parent.width - 48 : 420)
    height: id_content.implicitHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    Shortcut {
        sequence: "Backspace"
        enabled: id_root.opened
        onActivated: id_root.close()
    }

    Overlay.modal: Rectangle {
        color: Themes.targetAchievementEditPopup.colors.overlay
    }

    function pad(value) {
        return value < 10 ? "0" + value : "" + value
    }

    function setDateTime(date) {
        id_dayInput.text = pad(date.getDate())
        id_monthInput.text = pad(date.getMonth() + 1)
        id_yearInput.text = "" + date.getFullYear()
        id_hourInput.text = pad(date.getHours())
        id_minuteInput.text = pad(date.getMinutes())
    }

    function timestamp() {
        const date = new Date(
            Number(id_yearInput.text),
            Number(id_monthInput.text) - 1,
            Number(id_dayInput.text),
            Number(id_hourInput.text),
            Number(id_minuteInput.text),
            0,
            0
        )
        return Math.floor(date.getTime() / 1000)
    }

    function isDateTimeValid() {
        const day = Number(id_dayInput.text)
        const month = Number(id_monthInput.text)
        const year = Number(id_yearInput.text)
        const hour = Number(id_hourInput.text)
        const minute = Number(id_minuteInput.text)
        const date = new Date(year, month - 1, day, hour, minute, 0, 0)
        return id_dayInput.text.length > 0
            && id_monthInput.text.length > 0
            && id_yearInput.text.length === 4
            && id_hourInput.text.length > 0
            && id_minuteInput.text.length > 0
            && date.getFullYear() === year
            && date.getMonth() === month - 1
            && date.getDate() === day
            && date.getHours() === hour
            && date.getMinutes() === minute
    }

    function configure(appId, achievementKey, achievementName, achievementDescription, currentlyUnlocked) {
        id_root.p_appId = appId
        id_root.p_achievementKey = achievementKey
        id_root.p_achievementName = achievementName
        id_root.p_achievementDescription = achievementDescription
        id_root.p_currentlyUnlocked = currentlyUnlocked
        id_root.setDateTime(new Date())
    }

    function confirm() {
        if (!id_root.validDateTime) {
            return
        }

        id_root.confirmed(
            id_root.p_appId,
            id_root.p_achievementKey,
            id_root.unlockAction,
            id_root.unlockAction ? id_root.timestamp() : 0
        )
        id_root.close()
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_DateInput: CustomTextField {
        id: id_input

        property int digits: 2
        property string label: ""

        Layout.fillWidth: true
        implicitHeight: 38
        horizontalAlignment: TextInput.AlignHCenter
        selectByMouse: true
        maximumLength: digits
        inputMethodHints: Qt.ImhDigitsOnly
        validator: IntValidator {
            bottom: 0
            top: 9999
        }
        color: Themes.targetAchievementEditPopup.colors.buttonText
        placeholderTextColor: Themes.targetAchievementEditPopup.colors.bodyText
        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.input

        background: Rectangle {
            radius: 6
            color: Themes.targetAchievementEditPopup.colors.inputBackground
            border.width: 1
            border.color: id_input.activeFocus
                ? Themes.targetAchievementEditPopup.colors.inputBorderFocus
                : Themes.targetAchievementEditPopup.colors.inputBorder
        }
    }

    component C_ActionButton: CustomButton {
        id: id_button

        property bool danger: false

        Layout.fillWidth: true
        implicitHeight: 40

        contentItem: Label {
            text: id_button.text
            color: id_button.danger
                ? Themes.targetAchievementEditPopup.colors.dangerText
                : Themes.targetAchievementEditPopup.colors.buttonText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.button
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: id_button.danger
                ? id_button.down
                    ? Themes.targetAchievementEditPopup.colors.dangerBackgroundPressed
                    : id_button.hovered
                        ? Themes.targetAchievementEditPopup.colors.dangerBackgroundHover
                        : Themes.targetAchievementEditPopup.colors.dangerBackground
                : id_button.down
                    ? Themes.targetAchievementEditPopup.colors.buttonBackgroundPressed
                    : id_button.hovered
                        ? Themes.targetAchievementEditPopup.colors.buttonBackgroundHover
                        : Themes.targetAchievementEditPopup.colors.buttonBackground
            border.width: 1
            border.color: id_button.danger
                ? id_button.hovered
                    ? Themes.targetAchievementEditPopup.colors.dangerBorderHover
                    : Themes.targetAchievementEditPopup.colors.dangerBorder
                : id_button.hovered
                    ? Themes.targetAchievementEditPopup.colors.buttonBorderHover
                    : Themes.targetAchievementEditPopup.colors.buttonBorder

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

    background: Rectangle {
        radius: 8
        color: Themes.targetAchievementEditPopup.colors.background
        border.width: 1
        border.color: Themes.targetAchievementEditPopup.colors.border
    }

    contentItem: ColumnLayout {
        id: id_content

        spacing: 12

        Label {
            Layout.fillWidth: true
            text: id_root.unlockAction ? qsTr("Unlock Achievement") : qsTr("Lock Achievement")
            color: Themes.targetAchievementEditPopup.colors.titleText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_achievementName
            color: Themes.targetAchievementEditPopup.colors.titleText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.button
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_achievementDescription
            color: Themes.targetAchievementEditPopup.colors.bodyText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.body
            wrapMode: Text.WordWrap
            visible: text.length > 0
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: id_root.unlockAction
                ? qsTr("Confirm unlocking this achievement. Selected date will be stored as unlock time.")
                : qsTr("Confirm locking this achievement. Stored unlock time will be cleared.")
            color: Themes.targetAchievementEditPopup.colors.bodyText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.body
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: id_root.unlockAction

            Label {
                Layout.fillWidth: true
                text: qsTr("Date and time")
                color: Themes.targetAchievementEditPopup.colors.labelText
                font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("YYYY")
                        color: Themes.targetAchievementEditPopup.colors.labelText
                        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                        horizontalAlignment: Text.AlignHCenter
                    }

                    C_DateInput {
                        id: id_yearInput
                        digits: 4
                        placeholderText: qsTr("YYYY")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("MM")
                        color: Themes.targetAchievementEditPopup.colors.labelText
                        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                        horizontalAlignment: Text.AlignHCenter
                    }

                    C_DateInput {
                        id: id_monthInput
                        placeholderText: qsTr("MM")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("DD")
                        color: Themes.targetAchievementEditPopup.colors.labelText
                        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                        horizontalAlignment: Text.AlignHCenter
                    }

                    C_DateInput {
                        id: id_dayInput
                        placeholderText: qsTr("DD")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("HH")
                        color: Themes.targetAchievementEditPopup.colors.labelText
                        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                        horizontalAlignment: Text.AlignHCenter
                    }

                    C_DateInput {
                        id: id_hourInput
                        placeholderText: qsTr("HH")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("mm")
                        color: Themes.targetAchievementEditPopup.colors.labelText
                        font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.label
                        horizontalAlignment: Text.AlignHCenter
                    }

                    C_DateInput {
                        id: id_minuteInput
                        placeholderText: qsTr("mm")
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Enter valid day, month, year, hour, and minute.")
            color: Themes.targetAchievementEditPopup.colors.dangerText
            font.pixelSize: Themes.targetAchievementEditPopup.fontSizes.body
            wrapMode: Text.WordWrap
            visible: id_root.unlockAction && !id_root.validDateTime
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            C_ActionButton {
                text: qsTr("Cancel")
                onClicked: id_root.close()
            }

            C_ActionButton {
                text: qsTr("Confirm")
                danger: !id_root.unlockAction
                enabled: id_root.validDateTime
                opacity: enabled ? 1.0 : 0.45
                onClicked: id_root.confirm()
            }
        }
    }
}
