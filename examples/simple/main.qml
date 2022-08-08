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
            property bool fullWin: true
            property bool useTex: false
            layer.enabled: useTex
            color: "transparent"
            border.color: "green"
            border.width: 2
            width: fullWin ? parent.width : parent.width - 200
            height: fullWin ? parent.height : parent.height - 400
            x: fullWin ? 0 : 100
            y: fullWin ? 0 : 100
            z: ztimer.running ? 1 : 0
            Imgui {
                id: gui
                anchors.fill: parent
                SequentialAnimation on opacity {
                    id: opacityAnim
                    running: false
                    NumberAnimation { from: 1; to: 0; duration: 3000 }
                    NumberAnimation { from: 0; to: 1; duration: 3000 }
                }
            }
            Timer {
                id: ztimer
                repeat: false
                interval: 10000
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
                text: "Animate opacity"
                onClicked: opacityAnim.running = true
            }
            Button {
                text: "Move to top for 10 sec"
                onClicked: ztimer.running = true
            }
            Button {
                text: "Toggle layer (full size only)"
                onClicked: imguiContainer.useTex = !imguiContainer.useTex
            }
        }
    }
    Column {
        anchors.right: parent.right
        Text {
            text: "ImGui item visible: " + gui.visible
                  + "\ncovers entire window: " + imguiContainer.fullWin
                  + "\npart of an Item layer: " + imguiContainer.useTex
                  + "\nstacks on top of buttons/textedit: " + ztimer.running
        }
    }
}
