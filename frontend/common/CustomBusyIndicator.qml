/////////////////////////////////////////////////////////
// File: CustomBusyIndicator.qml
// Date: 2026-05-10
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Custom BusyIndicator
/////////////////////////////////////////////////////////

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: id_root

    // Public ________________________________________________
    property bool p_running: true
    property int p_indicatorSize: 128
    property int p_speed: 1200
    property string p_imageSource: p_indicatorSize <= 128
        ? "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00006_ED.png"
        : "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00006_.png"

    // Internals _____________________________________________
    visible: id_root.p_running
    width: p_indicatorSize
    height: p_indicatorSize
    color: "transparent"

    /////////////////////////////////////////////////////////////////////
    ////////////////////////////// PUBLIC ///////////////////////////////
    /////////////////////////////////////////////////////////////////////

    // Rotating image
    Image {
        id: id_spinnerImage

        visible: id_root.p_running
        anchors.centerIn: parent
        width:  parent.width
        height: parent.height
        source: id_root.p_imageSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true

        transform: Rotation {
            id: id_rotation
            
            origin.x: id_spinnerImage.width  / 2
            origin.y: id_spinnerImage.height / 2
            angle: 0
        }
    }

    // Spin animation
    RotationAnimation {
        id: id_spinAnimation

        target:    id_spinnerImage
        from:      0
        to:        360
        duration:  id_root.p_speed
        direction: RotationAnimation.Clockwise
        loops:     Animation.Infinite
        running:   id_root.p_running
    }
}
