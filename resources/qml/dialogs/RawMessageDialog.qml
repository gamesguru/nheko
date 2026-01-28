// SPDX-FileCopyrightText: Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import im.nheko

ApplicationWindow {
    id: rawMessageRoot

    property alias rawMessage: rawMessageView.text

    height: 420
    width: 420
    color: palette.window
    flags: Qt.Tool | Qt.WindowStaysOnTopHint | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: rawMessageRoot.close()
    }

    ScrollView {
        id: scrollView
        anchors.margins: Nheko.paddingMedium
        anchors.fill: parent
        padding: Nheko.paddingMedium

        TextArea {
            id: rawMessageView

            font: Nheko.monospaceFont()
            color: palette.text
            readOnly: true
            textFormat: Text.PlainText
            selectByMouse: true

            width: Math.max(scrollView.availableWidth, implicitWidth)
            height: Math.max(scrollView.availableHeight, implicitHeight)

            background: Rectangle {
                color: palette.base
            }
        }
    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Ok
        onAccepted: rawMessageRoot.close()
    }

}
