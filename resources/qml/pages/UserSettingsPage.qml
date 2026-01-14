// SPDX-FileCopyrightText: Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import ".."
import "../dialogs"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import im.nheko

Rectangle {
    id: userSettingsDialog

    property int collapsePoint: 600
    property bool collapsed: width < collapsePoint
    color: palette.window

    ScrollView {
        id: scroll

        ScrollBar.horizontal.visible: false
        anchors.fill: parent
        anchors.topMargin: (collapsed? backButton.height : 0)+Nheko.paddingLarge
        leftPadding: collapsed? Nheko.paddingMedium : Nheko.paddingLarge
        bottomPadding: Nheko.paddingLarge
        contentWidth: availableWidth

        ColumnLayout {
            id: grid

            spacing: Nheko.paddingMedium

            width: scroll.availableWidth
            anchors.fill: parent
            anchors.leftMargin: userSettingsDialog.collapsed ? 0 : (userSettingsDialog.width-userSettingsDialog.collapsePoint) * 0.4 + Nheko.paddingLarge
            anchors.rightMargin: anchors.leftMargin


            Repeater {
                model: UserSettingsModel

                delegate: GridLayout {
                    Layout.preferredWidth: scroll.availableWidth
                    columns: collapsed? 1 : 2
                    rows: collapsed? 2: 1
                    required property var model
                    id: r

                    Label {
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                        color: palette.text
                        text: model.name
                        //Layout.column: 0
                        Layout.columnSpan: (model.type == UserSettingsModel.SectionTitle && !userSettingsDialog.collapsed) ? 2 : 1
                        //Layout.row: model.index
                        //Layout.minimumWidth: implicitWidth
                        Layout.leftMargin: model.type == UserSettingsModel.SectionTitle ? 0 : Nheko.paddingMedium
                        Layout.topMargin: model.type == UserSettingsModel.SectionTitle ? Nheko.paddingLarge : 0
                        font.pointSize: 1.1 * fontMetrics.font.pointSize

                        HoverHandler {
                            id: hovered
                            enabled: model.description ?? false
                        }
                        ToolTip.visible: hovered.hovered && model.description
                        ToolTip.text: model.description ?? ""
                        ToolTip.delay: Nheko.tooltipDelay
                        wrapMode: Text.Wrap
                    }

                    DelegateChooser {
                        id: chooser

                        roleValue: model.type
                        Layout.alignment: Qt.AlignRight

                        Layout.columnSpan: (model.type == UserSettingsModel.SectionTitle && !userSettingsDialog.collapsed) ? 2 : 1
                        Layout.preferredHeight: child.height
                        Layout.preferredWidth: child.implicitWidth
                        Layout.maximumWidth: model.type == UserSettingsModel.SectionTitle ? Number.POSITIVE_INFINITY : 400
                        Layout.fillWidth: model.type == UserSettingsModel.SectionTitle || model.type == UserSettingsModel.Options || model.type == UserSettingsModel.Number
                        Layout.rightMargin: model.type == UserSettingsModel.SectionTitle ? 0 : Nheko.paddingMedium

                        DelegateChoice {
                            roleValue: UserSettingsModel.Toggle
                            ToggleButton {
                                checked: model.value
                                onClicked: model.value = checked
                                enabled: model.enabled
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Options
                            Item {
                                anchors.right: parent.right
                                height: child.implicitHeight
                                width: Math.min(child.implicitWidth, scroll.availableWidth - Nheko.paddingMedium)

                                ComboBox {
                                    id: child
                                    anchors.fill: parent
                                    model: r.model.values
                                    currentIndex: r.model.value
                                    onActivated: {
                                        r.model.value = currentIndex
                                        userSettingsDialog.isDirty = true
                                    }
                                    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted

                                    // Disable built-in wheel handling to prevent 'hover' capture
                                    wheelEnabled: false

                                    // Manual wheel handling only when focused
                                    WheelHandler {
                                        enabled: child.activeFocus
                                        onWheel: (event)=> {
                                            if (event.angleDelta.y > 0) child.decrementCurrentIndex();
                                            else child.incrementCurrentIndex();
                                        }
                                    }
                                }
                                
                                // Click to focus
                                MouseArea {
                                    anchors.fill: parent
                                    propagateComposedEvents: true
                                    enabled: !child.activeFocus
                                    onClicked: {
                                        child.forceActiveFocus()
                                        child.togglePopup()
                                    }
                                    onWheel: (wheel)=> {
                                        wheel.accepted = false
                                    }
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Integer

                            SpinBox {
                                anchors.right: parent.right
                                from: model.valueLowerBound
                                to: model.valueUpperBound
                                stepSize: model.valueStep
                                value: model.value
                                onValueChanged: model.value = value
                                editable: true

                                WheelHandler{} // suppress scrolling changing values
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.Double

                            SpinBox {
                                id: spinbox

                                readonly property double div: 100
                                readonly property int decimals: 2

                                anchors.right: parent.right
                                from: model.valueLowerBound * div
                                to: model.valueUpperBound * div
                                stepSize: model.valueStep * div
                                value: model.value * div
                                onValueModified: model.value = value/div
                                editable: true

                                property real realValue: value / div

                                validator: DoubleValidator {
                                    bottom: Math.min(spinbox.from/spinbox.div, spinbox.to/spinbox.div)
                                    top:  Math.max(spinbox.from/spinbox.div, spinbox.to/spinbox.div)
                                }

                                textFromValue: function(value, locale) {
                                    return Number(value / spinbox.div).toLocaleString(locale, 'f', spinbox.decimals)
                                }

                                valueFromText: function(text, locale) {
                                    return Number.fromLocaleString(locale, text) * spinbox.div
                                }

                                WheelHandler{} // suppress scrolling changing values
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ReadOnlyText
                            TextEdit {
                                color: palette.text
                                text: model.value
                                readOnly: true
                                textFormat: Text.PlainText
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SectionTitle
                            Item {
                                width: grid.width
                                height: fontMetrics.lineSpacing
                                Rectangle {
                                    anchors.topMargin: Nheko.paddingSmall
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    color: palette.buttonText
                                    height: 1
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.KeyStatus
                            Text {
                                color: model.good ? "green" : Nheko.theme.error
                                text: model.value ? qsTr("CACHED") : qsTr("NOT CACHED")
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.SessionKeyImportExport
                            RowLayout {
                                Button {
                                    text: qsTr("IMPORT")
                                    onClicked: UserSettingsModel.importSessionKeys()
                                }
                                Button {
                                    text: qsTr("EXPORT")
                                    onClicked: UserSettingsModel.exportSessionKeys()
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.XSignKeysRequestDownload
                            RowLayout {
                                Button {
                                    text: qsTr("DOWNLOAD")
                                    onClicked: UserSettingsModel.downloadCrossSigningSecrets()
                                }
                                Button {
                                    text: qsTr("REQUEST")
                                    onClicked: UserSettingsModel.requestCrossSigningSecrets()
                                }
                            }
                        }
                        DelegateChoice {
                            roleValue: UserSettingsModel.ConfigureHiddenEvents
                            Button {
                                text: qsTr("CONFIGURE")
                                onClicked: {
                                    var dialog = hiddenEventsDialog.createObject();
                                    dialog.show();
                                    destroyOnClose(dialog);
                                }

                                Component {
                                    id: hiddenEventsDialog

                                    HiddenEventsDialog {}
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: UserSettingsModel.ManageIgnoredUsers
                            Button {
                                text: qsTr("MANAGE")
                                onClicked: {
                                    var dialog = ignoredUsersDialog.createObject();
                                    dialog.show();
                                    destroyOnClose(dialog);
                                }

                                Component {
                                    id: ignoredUsersDialog

                                    IgnoredUsers {}
                                }
                            }
                        }

                        DelegateChoice {
                            Text {
                                text: model.value
                            }
                        }
                    }
                }
            }
        }
    }

    property bool isDirty: false

    Dialog {
        id: confirmationDialog
        title: qsTr("Unsaved Changes")
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        anchors.centerIn: parent
        modal: true
        
        Label {
            text: qsTr("You have unsaved changes. Do you want to save them?")
            color: palette.text
        }
        
        onAccepted: { // Save
            isDirty = false
            console.log("Settings saved (implicitly applied)")
            mainWindow.pop()
        }
        onDiscarded: { // Discard
            isDirty = false
            mainWindow.pop()
        }
        onRejected: { // Cancel
            // Do nothing
        }
    }

    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: {
            if (isDirty) {
               confirmationDialog.open()
            } else {
               mainWindow.pop()
            }
        }
    }

    RowLayout {
        id: headerLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Nheko.paddingMedium
        spacing: Nheko.paddingMedium
        z: 3

        ImageButton {
            id: backButton // Restore ID for layout reference compatibility
            width: Nheko.avatarSize
            height: Nheko.avatarSize
            image: ":/icons/icons/ui/angle-arrow-left.svg"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Back")
            onClicked: {
                 if (isDirty) {
                     confirmationDialog.open()
                 } else {
                     mainWindow.pop()
                 }
            }
        }
        
        Label {
            text: qsTr("User Settings")
            Layout.fillWidth: true
            horizontalAlignment: Qt.AlignHCenter
            font.pointSize: fontMetrics.font.pointSize * 1.2
            color: palette.text
        }

        Button {
            text: qsTr("Save")
            enabled: isDirty
            onClicked: {
                isDirty = false
            }
        }
    }
}

