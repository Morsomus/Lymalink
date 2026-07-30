/////////////////////////////////////////////////////////
// File: ConflictResolutionPopup.qml
// Date: 2026-06-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Popup for resolving import conflicts
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: id_root

    // Public ________________________________________________
    property var p_conflicts: []
    property string p_title: qsTr("Resolve Conflicts")
    property string p_description: ""
    property string p_mergeText: qsTr("Merge")
    property string p_replaceText: qsTr("Replace")
    property string p_cancelText: qsTr("Cancel")
    property string p_confirmText: qsTr("Import")
    property int p_popupWidth: 720
    property int p_popupHeight: 620

    property var conflictModes: ({})
    property int conflictModeRevision: 0
    property bool closeHandled: false

    signal canceled()
    signal confirmed(var decisions)

    parent: Overlay.overlay
    width: Math.min(p_popupWidth, parent ? parent.width - 48 : p_popupWidth)
    height: Math.min(id_content.implicitHeight + topPadding + bottomPadding, parent ? parent.height - 48 : p_popupHeight)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    Overlay.modal: Rectangle {
        color: Themes.conflictResolutionPopup.colors.overlay
    }

    background: Rectangle {
        radius: 8
        color: Themes.conflictResolutionPopup.colors.background
        border.width: 1
        border.color: Themes.conflictResolutionPopup.colors.border
    }

    function openConflicts(conflicts) {
        p_conflicts = conflicts || []
        conflictModes = {}
        closeHandled = false
        ++conflictModeRevision
        open()
    }

    function setConflictMode(appId, mode) {
        conflictModes[appId] = mode === "replace" ? "replace" : "merge"
        ++conflictModeRevision
    }

    function setAllConflictModes(mode) {
        const normalizedMode = mode === "replace" ? "replace" : "merge"
        for (let i = 0; i < id_root.p_conflicts.length; ++i) {
            id_root.conflictModes[id_root.p_conflicts[i].id] = normalizedMode
        }
        ++id_root.conflictModeRevision
    }

    function conflictModeCount(mode) {
        id_root.conflictModeRevision
        let count = 0
        for (let i = 0; i < id_root.p_conflicts.length; ++i) {
            if (id_root.conflictMode(id_root.p_conflicts[i].id) === mode) {
                ++count
            }
        }
        return count
    }

    function bulkConflictCheckState(mode) {
        const count = id_root.conflictModeCount(mode)
        if (count === 0) {
            return Qt.Unchecked
        }
        return count === id_root.p_conflicts.length ? Qt.Checked : Qt.PartiallyChecked
    }

    function conflictMode(appId) {
        id_root.conflictModeRevision
        return id_root.conflictModes[appId] === "replace" ? "replace" : "merge"
    }

    function decisions() {
        const values = []
        for (let i = 0; i < id_root.p_conflicts.length; ++i) {
            values.push({
                id: id_root.p_conflicts[i].id,
                mode: id_root.conflictMode(id_root.p_conflicts[i].id)
            })
        }
        return values
    }

    function reset() {
        id_root.p_conflicts = []
        id_root.conflictModes = {}
        id_root.closeHandled = false
        ++id_root.conflictModeRevision
    }

    onClosed: {
        if (!id_root.closeHandled && id_root.p_conflicts.length > 0) {
            id_root.closeHandled = true
            id_root.canceled()
        }
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////// COMPONENTS /////////////////////////////
    /////////////////////////////////////////////////////////////////////

    component C_ActionButton: CustomButton {
        Layout.fillWidth: true
        implicitHeight: 40
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    contentItem: ColumnLayout {
        id: id_content

        spacing: 12

        Label {
            Layout.fillWidth: true
            text: id_root.p_title
            color: Themes.conflictResolutionPopup.colors.titleText
            font.pixelSize: Themes.conflictResolutionPopup.fontSizes.title
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: id_root.p_description
            color: Themes.conflictResolutionPopup.colors.bodyText
            font.pixelSize: Themes.conflictResolutionPopup.fontSizes.body
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: 39
            spacing: 8
            visible: id_root.p_conflicts.length > 1

            Item {
                Layout.fillWidth: true
            }

            CustomCheckBox {
                Layout.preferredWidth: 80
                text: qsTr("%1").arg(id_root.p_mergeText)
                checkState: id_root.bulkConflictCheckState("merge")
                onClicked: id_root.setAllConflictModes("merge")
            }

            CustomCheckBox {
                Layout.preferredWidth: 80
                text: qsTr("%1").arg(id_root.p_replaceText)
                checkState: id_root.bulkConflictCheckState("replace")
                onClicked: id_root.setAllConflictModes("replace")
            }
        }

        ListView {
            id: id_conflictList

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, Math.max(120, contentHeight))
            clip: true
            spacing: 8
            model: id_root.p_conflicts
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: CustomScrollBar {
                policy: ScrollBar.AlwaysOn
            }

            delegate: Rectangle {
                id: id_conflictRow

                width: Math.max(0, ListView.view.width - 30)
                implicitHeight: id_conflictRowLayout.implicitHeight + 18
                radius: 6
                color: Themes.conflictResolutionPopup.colors.rowBackground
                border.width: 1
                border.color: Themes.conflictResolutionPopup.colors.rowBorder

                ColumnLayout {
                    id: id_conflictRowLayout

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 9
                    }
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Label {
                            Layout.fillWidth: true
                            text: "%1 (%2)".arg(modelData.name.length > 0 ? modelData.name : modelData.currentName).arg(modelData.id)
                            color: Themes.conflictResolutionPopup.colors.rowTitleText
                            font.pixelSize: Themes.conflictResolutionPopup.fontSizes.rowTitle
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 8

                            CustomCheckBox {
                                Layout.preferredWidth: 80
                                text: id_root.p_mergeText
                                checked: id_root.conflictMode(modelData.id) !== "replace"
                                onClicked: id_root.setConflictMode(modelData.id, "merge")
                            }

                            CustomCheckBox {
                                Layout.preferredWidth: 80
                                text: id_root.p_replaceText
                                checked: id_root.conflictMode(modelData.id) === "replace"
                                onClicked: id_root.setConflictMode(modelData.id, "replace")
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Current: %1 total, %2 unlocked - Import: %3 total, %4 unlocked")
                            .arg(modelData.currentAchievementCount)
                            .arg(modelData.currentUnlockedCount)
                            .arg(modelData.importedAchievementCount)
                            .arg(modelData.importedUnlockedCount)
                        color: Themes.conflictResolutionPopup.colors.bodyText
                        font.pixelSize: Themes.conflictResolutionPopup.fontSizes.body
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            C_ActionButton {
                text: id_root.p_cancelText
                onClicked: {
                    id_root.closeHandled = true
                    id_root.canceled()
                    id_root.close()
                }
            }

            C_ActionButton {
                text: id_root.p_confirmText
                onClicked: {
                    id_root.closeHandled = true
                    id_root.confirmed(id_root.decisions())
                    id_root.close()
                }
            }
        }
    }
}
