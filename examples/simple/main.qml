import QtQuick
import QtQuick.Controls
import ImguiExample

Item {
    Rectangle {
        color: "red"
        anchors.fill: parent
        Imgui {
            id: gui
            anchors.fill: parent
        }
        Rectangle {
            border.width: 2
            border.color: black
            y: 50
            width: 400
            height: 50
            clip: true
            TextEdit {
                id: textEdit
                anchors.fill: parent
                text: "TextEdit to test focus"
                font.pointSize: 20
                color: "blue"
            }
        }
        Button {
            y: 20
            text: "Toggle"
            onClicked: gui.visible = !gui.visible
        }
    }
}
