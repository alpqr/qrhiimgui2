import QtQuick
import ImguiTextureExample

Rectangle {
    color: "lightGray"

    Rectangle {
        color: "green"
        width: 300
        height: 300
        x: 100
        y: 100
        NumberAnimation on rotation { from: 0; to: 360; duration: 3000; loops: -1 }
        Text { text: "This is a Qt Quick scene"; anchors.centerIn: parent }
    }

    Imgui {
        anchors.fill: parent
    }
}
