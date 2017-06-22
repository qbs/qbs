import QtQuick 2.6
import QtQuick.Window 2.2
import "stuff.js" as Stuff

Window {
    visible: true
    width: 640
    height: 480
    title: Stuff.title()

    MainForm {
        anchors.fill: parent
        mouseArea.onClicked: {
            console.log(qsTr('Clicked on background. Text: "' + textEdit.text + '"'))
        }
    }
}
