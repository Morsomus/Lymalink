/////////////////////////////////////////////////////////
// File: GHElement.qml
// Date: 2026-05-02
// Author: Morsomus
// Copyright: see /LICENSE
// Description: GitHub button component for displaying
//              a clickable GitHub link to the project.
/////////////////////////////////////////////////////////

import app.themes 1.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    
    property bool collapsed: false
    property url iconSource: "qrc:/qt/qml/Lymalink/res/img-external/GitHub_Invertocat_White_Clearspace.png"
    property string linkUrl: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 42

    flat: true
    text: root.collapsed ? "GH" : qsTr("GitHub")
    
    ToolTip.visible: hovered
    ToolTip.delay: 300
    ToolTip.text: qsTr("View project on GitHub")

    onClicked: {
        Qt.openUrlExternally(linkUrl)
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            Layout.fillWidth: root.collapsed ? true : false
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Label {
            Layout.fillWidth: true
            visible: !root.collapsed
            text: qsTr("Project on GitHub")
            color: Themes.general.colors.linkText
            font.pixelSize: Themes.general.fontSizes.link
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: 8
        color: root.down
            ? Themes.general.colors.linkBackgroundPressed
            : (root.hovered
                ? Themes.general.colors.linkBackgroundHover
                : Themes.general.colors.linkBackground)
    }
}
