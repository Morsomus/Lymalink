/////////////////////////////////////////////////////////
// File: ErrorImage.qml
// Date: 2026-05-10
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Error indicator in image form
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import app.themes 1.0


Column {
    id: id_root

    // Public ________________________________________________
    property int p_size: 32

    // Internals _____________________________________________
    spacing: 6
    anchors.horizontalCenter: parent.horizontalCenter

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    Image {
        width: id_root.p_size
        height: id_root.p_size
        source: "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00032_ED.png"
        fillMode: Image.PreserveAspectFit
        anchors.horizontalCenter: parent.horizontalCenter
        smooth: true
        mipmap: true

        Text {
            text: qsTr("Error")
            color: Themes.errorImage.colors.errorImageText
            font.pixelSize: Themes.errorImage.fontSizes.errorImageText
            anchors.horizontalCenter: parent.horizontalCenter
            visible: parent.status === Image.Error
        }
    }
}
