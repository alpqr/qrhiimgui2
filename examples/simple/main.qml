import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ImguiExample

Item {
    Rectangle {
        color: "red"
        anchors.fill: parent
        Rectangle {
            id: imguiContainer
            property bool fullWin: false
            property bool useTex: false
            layer.enabled: useTex
            color: "transparent"
            border.color: "green"
            border.width: 2
            width: fullWin ? parent.width : parent.width - 200
            height: fullWin ? parent.height : parent.height - 400
            x: fullWin ? 0 : 100
            y: fullWin ? 0 : 100
            Imgui {
                id: gui
                anchors.fill: parent
            }
        }
        Rectangle {
            border.width: 2
            border.color: "black"
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
        RowLayout {
            y: 20
            Button {
                text: "Toggle visibility"
                onClicked: gui.visible = !gui.visible
            }
            Button {
                text: "Toggle size"
                onClicked: imguiContainer.fullWin = !imguiContainer.fullWin
            }
            Button {
                text: "Toggle layer"
                onClicked: imguiContainer.useTex = !imguiContainer.useTex
            }
        }
    }
}
