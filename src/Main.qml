import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    property int layoutMargin: 24
    width: 680
    height: 300
    minimumWidth: Math.max(680, rootLayout.implicitWidth + layoutMargin * 2)
    minimumHeight: Math.max(300, rootLayout.implicitHeight + layoutMargin * 2)
    leftPadding: layoutMargin; rightPadding: layoutMargin; topPadding: layoutMargin; bottomPadding: layoutMargin
    visible: true
    title: backend.windowTitle
    Material.theme: Material.Light
    Material.primary: Material.Indigo
    Material.accent: Material.Pink
    property string outputFile: ""
    property bool stretchToPage: false
    property bool includeSubdirectories: true
    property string selectedPageSize: "A4"
    property bool landscapeOrientation: false

    function localPathFromUrl(url) {
        if (!url) return "";
        var value = url.toString ? url.toString() : url;
        var prefix = "file://";
        if (value.startsWith(prefix)) {
            value = decodeURIComponent(value.substring(prefix.length));
            if (Qt.platform.os === "windows" && value.length > 2 && value[0] === "/" && value[2] === ":")
                value = value.substring(1);
        }
        return value;
    }

    ColumnLayout {
        id: rootLayout
        anchors.fill: parent
        spacing: 18

        GroupBox {
            title: qsTr("待转换的图片")
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 400
            Layout.minimumHeight: 320
            Layout.preferredHeight: 380

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 12

                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    Button { text: qsTr("添加图片…"); onClicked: imageFileDialog.open() }
                    Button { text: qsTr("导入文件夹…"); onClicked: folderDialog.open() }
                    CheckBox { checked: includeSubdirectories; text: qsTr("包含子文件夹"); onToggled: includeSubdirectories = checked }
                    Button { text: qsTr("清空列表"); enabled: backend.imageCount > 0; onClicked: backend.clearImages() }
                    Label { Layout.fillWidth: true; horizontalAlignment: Qt.AlignRight; color: Material.color(Material.Grey); text: qsTr("共 %1 张").arg(backend.imageCount) }
                }

                ListView {
                    id: imageList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 280
                    clip: true
                    spacing: 4
                    boundsBehavior: Flickable.StopAtBounds
                    model: backend.imageModel

                    cacheBuffer: 2000

                    delegate: Rectangle {
                        id: delegateRoot
                        property string path: modelData
                        width: imageList.width
                        height: 48
                        radius: 4

                        color: index % 2 === 0 ? Qt.rgba(0, 0, 0, 0.04) : Qt.rgba(0, 0, 0, 0.015)
                        border.width: 1
                        border.color: Qt.rgba(0, 0, 0, 0.08)

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: path
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                            }

                            ToolButton {
                                text: "↑"
                                enabled: index > 0
                                onClicked: backend.moveImage(index, index - 1)
                            }
                            ToolButton {
                                text: "↓"
                                enabled: index < imageList.count - 1
                                onClicked: backend.moveImage(index, index + 1)
                            }
                            ToolButton {
                                text: qsTr("移除")
                                onClicked: backend.removeImage(index)
                            }
                        }
                    }
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }

                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 40; visible: backend.imageCount === 0
                    Label { anchors.centerIn: parent; text: qsTr("尚未选择任何图片。"); color: Material.color(Material.Grey) }
                }
            }
        }

        // ... (输出设置 GroupBox 保持不变) ...
        GroupBox {
            title: qsTr("输出设置")
            Layout.fillWidth: true
            Layout.minimumWidth: 320
            Layout.minimumHeight: 200

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 12
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField { Layout.fillWidth: true; readOnly: true; placeholderText: qsTr("选择输出 PDF 文件"); text: outputFile }
                    Button { text: qsTr("浏览…"); onClicked: saveDialog.open() }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    Label { text: qsTr("边距 (mm)"); font.bold: true }
                    Slider { id: marginSlider; Layout.fillWidth: true; from: 0; to: 30; stepSize: 1; value: 10 }
                    Label { width: 40; horizontalAlignment: Qt.AlignHCenter; text: Math.round(marginSlider.value) }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    Label { text: qsTr("纸张大小"); font.bold: true }
                    ComboBox {
                        id: pageSizeCombo; Layout.fillWidth: true; textRole: "text"; valueRole: "value"
                        model: [ { text: qsTr("A3"), value: "A3" }, { text: qsTr("A4"), value: "A4" }, { text: qsTr("A5"), value: "A5" }, { text: qsTr("B5"), value: "B5" }, { text: qsTr("Letter"), value: "Letter" }, { text: qsTr("Legal"), value: "Legal" }, { text: qsTr("Tabloid"), value: "Tabloid" } ]
                        onCurrentValueChanged: selectedPageSize = currentValue || "A4"; Component.onCompleted: selectedPageSize = currentValue || "A4"
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.bottomMargin: 4; spacing: 12
                    Label { text: qsTr("纸张方向"); font.bold: true }
                    ComboBox {
                        id: orientationCombo; Layout.preferredWidth: 180; textRole: "text"; valueRole: "value"
                        model: [ { text: qsTr("纵向"), value: false }, { text: qsTr("横向"), value: true } ]
                        onCurrentValueChanged: landscapeOrientation = !!currentValue; Component.onCompleted: landscapeOrientation = !!currentValue
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: qsTr("拉伸填满页面（不保留比例）") }
                    Switch { checked: stretchToPage; onToggled: stretchToPage = checked }
                }
                Item { Layout.fillWidth: true; Layout.preferredHeight: 6 }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true; spacing: 8
            Button {
                Layout.fillWidth: true; text: backend.conversionRunning ? qsTr("正在转换…") : qsTr("开始转换")
                enabled: backend.imageCount > 0 && outputFile.length > 0 && !backend.conversionRunning
                onClicked: backend.convertToPdf(outputFile, Math.round(marginSlider.value), stretchToPage, selectedPageSize, landscapeOrientation)
            }
            ProgressBar { Layout.fillWidth: true; visible: backend.conversionRunning; indeterminate: true }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: backend.statusText }
        }
    }
    FileDialog { id: imageFileDialog; title: qsTr("选择图片文件"); nameFilters: [qsTr("图像文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff)")]; fileMode: FileDialog.OpenFiles; onAccepted: { const files = []; for (let i = 0; i < selectedFiles.length; ++i) { const localPath = localPathFromUrl(selectedFiles[i]); if (localPath.length > 0) files.push(localPath); } if (files.length > 0) backend.addImages(files); } }
    FolderDialog { id: folderDialog; title: qsTr("选择图片文件夹"); onAccepted: { const folderPath = localPathFromUrl(selectedFolder); if (folderPath.length > 0) backend.addDirectory(folderPath, includeSubdirectories); } }
    FileDialog { id: saveDialog; title: qsTr("保存 PDF"); nameFilters: [qsTr("PDF 文件 (*.pdf)")]; fileMode: FileDialog.SaveFile; defaultSuffix: "pdf"; onAccepted: { if (selectedFile) { var filePath = localPathFromUrl(selectedFile); if (!filePath.toLowerCase().endsWith(".pdf")) filePath = filePath + ".pdf"; outputFile = filePath; } } }
}
