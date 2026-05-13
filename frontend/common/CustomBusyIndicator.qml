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

    property bool running: true
    property int indicatorSize: 128
    property int speed: 1200
    property string imageSource: indicatorSize <= 128
        ? "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00006_ED.png"
        : "qrc:/qt/qml/Lymalink/res/img/BlankBackground_MFC_Glow_00006_.png"

    // Internals
    visible: id_root.running
    width:  indicatorSize
    height: indicatorSize
    color:  "transparent"

    // Rotating image
    Image {
        id: id_spinnerImage

        visible: id_root.running
        anchors.centerIn: parent
        width:  parent.width
        height: parent.height
        source: id_root.imageSource
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
        duration:  id_root.speed
        direction: RotationAnimation.Clockwise
        loops:     Animation.Infinite
        running:   id_root.running
    }
}