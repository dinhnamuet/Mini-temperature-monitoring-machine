import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

import usbdev
ApplicationWindow {
    id: root
    visible: true
    width: screen.width * 0.8
    height: screen.height * 0.8
    title: qsTr("Dinhnamuet MicroControlers Programming Application")
    property string devfs: ""
    header: Row {
        width: parent.width
        height: parent.height * 0.15
        Frame {
            id: logo
            width: parent.height
            height: parent.height
            Image {
                anchors.fill: parent
                source: "image/intins.png"
            }
        }
        Frame {
            width: parent.width - logo.width
            height: parent.height
            Image {
                anchors.centerIn: parent
                height: parent.height
                source: "image/onepiece.png"
            }
        }
    }
    UsbDev {
        id: stm32
    }
    Frame {
        anchors.fill: parent
        Column {
            width: parent.width
            height: parent.height
            spacing: 5
            anchors.horizontalCenter: parent.horizontalCenter
            Frame {
                width: parent.width
                height: parent.height * 0.1
                Column {
                    anchors.fill: parent
                    spacing: 2
                    Text {
                        height: parent.height * 0.3
                        text: "Device File"
                    }
                    ComboBox {
                        id: stm32ComboBox
                        width: parent.width
                        height: parent.height * 0.7
                        editable: false
                        model: listModel
                        ListModel {
                            id: listModel
                        }
                        Component.onCompleted: {
                            var stm32Devices = stm32.findStm32Devices();
                            for (var i = 0; i < stm32Devices.length; i++) {
                                listModel.append({"text": stm32Devices[i]});
                            }
                        }
                        onCurrentIndexChanged: {
                            devfs = (listModel.get(currentIndex).text);
                        }
                    }
                }
            }
            Frame {
                width: parent.width
                height: parent.height * 0.1
                Column {
                    anchors.fill: parent
                    spacing: 2
                    Text {
                        height: parent.height * 0.3
                        text: "Hex File"
                    }
                    Row {
                        id: hex
                        width: parent.width
                        height: parent.height * 0.7
                        Frame {
                            width: parent.width * 0.8
                            height: parent.height
                            Rectangle {
                                anchors.fill: parent
                                width: parent.width
                                Text {
                                    id: url
                                    anchors.centerIn: parent
                                }
                            }
                        }

                        Frame {
                            width: parent.width * 0.2
                            height: parent.height
                            Button {
                                text: "Open Browser"
                                anchors.centerIn: parent
                                width: parent.width
                                flat: true
                                onClicked: fileDialog.open()
                            }
                        }
                    }
                }
            }
            Button {
                width: parent.width
                height: parent.height * 0.1
                text: "Download Program to Device!"
                highlighted: true
                onClicked: {
                    if (devfs != "" && fileDialog.selectedFile != "") {
                        progress.indeterminate = true
                        progress.visible = true
                        progress_txt.color = "Black"
                        progress_txt.text = "Wait for erase memory!"
                        stm32.startUpdateFirmware(devfs, fileDialog.selectedFile)
                    } else {
                        progress_txt.text = "Device or Binary not found, Please try again!"
                        progress_txt.color = "Red"
                        progress_txt.font.italic = true
                    }
                }
            }

            Frame {
                width: parent.width
                height: parent.height * 0.1
                Column {
                    anchors.fill: parent
                    Text {
                        id: progress_txt
                        visible: true
                        text: "Status"
                    }

                    ProgressBar {
                        id: progress
                        visible: false
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.7
                        from: 0
                        to: 100
                        value: stm32.getProgress()
                    }
                }
            }
            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width * 0.5
                height: parent.height * 0.5
                source: "image/embedded.png"
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select a File"
        currentFile: "file:/home/nam/usb_dfu/sw_backend/Blynk_led.hex"
        onAccepted: {
            url.text = fileDialog.selectedFile
        }
        onRejected: {
            console.log("User canceled")
        }
    }

    Connections {
        target: stm32
        function onProgressChanged(prog) {
            progress.value = prog
            progress_txt.text = "Updating: " + prog + "%"
        }
        function onUpdateCompleted() {
            progress.value = 100
            progress_txt.color = "green"
            progress_txt.text = "Update Firmware Successfully!"
        }
        function onEraseCompleted() {
            progress_txt.text = "Wait 2s!"
            progress.indeterminate = false
            progress_txt.font.italic = true
        }
    }

    Timer {
        interval: 2000
        running: true
        repeat: true
        onTriggered:  {
            var stm32Devices = stm32.findStm32Devices();
            for (var i = 0; i < stm32Devices.length; i++) {
                var deviceExists = false;

                for (var j = 0; j < listModel.count; j++) {
                    if (listModel.get(j).text === stm32Devices[i]) {
                        deviceExists = true;
                        break;
                    }
                }

                if (!deviceExists) {
                    listModel.append({"text": stm32Devices[i]});
                }
            }
        }
    }

}
