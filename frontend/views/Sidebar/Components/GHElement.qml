/////////////////////////////////////////////////////////
// File: GHElement.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: GitHub button component for displaying
//              a clickable GitHub link to the project.
/////////////////////////////////////////////////////////

import Lymalink
import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

CustomButton {
    id: id_root
    
    // Public ________________________________________________
    property bool p_collapsed: false
    readonly property string effectiveTheme: {
        if (ctxSettings.theme === "system") {
            return Qt.styleHints.colorScheme === Qt.ColorScheme.Light ? "light" : "dark"
        }

        return ctxSettings.theme === "light" ? "light" : "dark"
    }
    property url p_iconSource: effectiveTheme === "light"
        ? "qrc:/qt/qml/Lymalink/res/img-external/GitHub_Invertocat_Black_Clearspace.png"
        : "qrc:/qt/qml/Lymalink/res/img-external/GitHub_Invertocat_White_Clearspace.png"
    property string p_linkUrl: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 42

    flat: true
    leftPadding: 2
    rightPadding: 2
    text: id_root.p_collapsed ? "GH" : qsTr("GitHub")

    CustomTooltip {
        p_alwaysVisible: true
        p_active: id_root.hovered
        p_delay: 300
        p_text: qsTr("View project on GitHub")
    }

    onClicked: {
        Qt.openUrlExternally(p_linkUrl)
    }

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.fillWidth: id_root.p_collapsed ? true : false
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: id_root.p_iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Label {
            Layout.fillWidth: true
            visible: !id_root.p_collapsed
            text: qsTr("Project on GitHub")
            color: Themes.general.colors.linkText
            font.pixelSize: Themes.general.fontSizes.link
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: 8
        color: id_root.down
            ? Themes.general.colors.linkBackgroundPressed
            : (id_root.hovered
                ? Themes.general.colors.linkBackgroundHover
                : Themes.general.colors.linkBackground)
    }
}
