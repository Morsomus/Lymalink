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

Button {
    id: id_root
    
    property bool collapsed: false
    property url iconSource: "qrc:/qt/qml/Lymalink/res/img-external/GitHub_Invertocat_White_Clearspace.png"
    property string linkUrl: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 42

    flat: true
    text: id_root.collapsed ? "GH" : qsTr("GitHub")

    CustomTooltip {
        p_alwaysVisible: true
        active: id_root.hovered
        delay: 300
        text: qsTr("View project on GitHub")
    }

    onClicked: {
        Qt.openUrlExternally(linkUrl)
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.fillWidth: id_root.collapsed ? true : false
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: id_root.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Label {
            Layout.fillWidth: true
            visible: !id_root.collapsed
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
