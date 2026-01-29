// SPDX-FileCopyrightText: Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko

Image {
    id: stateImg

    property bool encrypted: false
    property var encryptionInfo: ({})
    property bool hovered: ma.hovered
    property string sourceUrl: {
        if (!encrypted)
            return "image://colorimage/" + unencryptedIcon + "?";
        switch (trust) {
        case Crypto.Verified:
            return "image://colorimage/:/icons/icons/ui/shield-filled-checkmark.svg?";
        case Crypto.TOFU:
            return "image://colorimage/:/icons/icons/ui/shield-filled.svg?";
        case Crypto.Unverified:
            return "image://colorimage/:/icons/icons/ui/shield-filled-exclamation-mark.svg?";
        case Crypto.MessageUnverified:
            return Settings.mildKeyWarning ? "image://colorimage/:/icons/icons/ui/shield-filled-checkmark.svg?" : "image://colorimage/:/icons/icons/ui/shield-filled-exclamation-mark.svg?";
        default:
            return "image://colorimage/:/icons/icons/ui/shield-filled-cross.svg?";
        }
    }
    property int trust: Crypto.Unverified
    property color unencryptedColor: Nheko.theme.error
    property color unencryptedHoverColor: unencryptedColor
    property string unencryptedIcon: ":/icons/icons/ui/shield-filled-cross.svg"

    ToolTip.text: {
        var deviceId = encryptionInfo && encryptionInfo.deviceId ? encryptionInfo.deviceId : "";
        console.log("EncryptionIndicator: trust=" + trust + ", deviceId=" + deviceId + ", encrypted=" + encrypted);
        if (!encrypted)
            return qsTr("This message is not encrypted!");
        switch (trust) {
        case Crypto.Verified:
            return deviceId ? qsTr("Encrypted by verified device: %1").arg(deviceId) : qsTr("Encrypted by a verified device");
        case Crypto.TOFU:
            return deviceId ? qsTr("Encrypted by unverified device %1, but you have trusted that user so far.").arg(deviceId) : qsTr("Encrypted by an unverified device, but you have trusted that user so far.");
        case Crypto.MessageUnverified:
            var msg = qsTr("Key is from an untrusted source, possibly forwarded from another user or the online key backup. For this reason we can't verify who sent the message.");
            if (encryptionInfo) {
                if (encryptionInfo.deviceId) msg += "\nDevice ID: " + encryptionInfo.deviceId;
                if (encryptionInfo.senderKey) msg += "\nSender Key: " + encryptionInfo.senderKey;
                if (encryptionInfo.sessionId) msg += "\nSession ID: " + encryptionInfo.sessionId;
            }
            return msg;
        default:
            return deviceId ? qsTr("Encrypted by unverified device: %1").arg(deviceId) : qsTr("Encrypted by an unverified device.");
        }
    }
    ToolTip.visible: stateImg.hovered
    height: 16
    source: {
        if (encrypted) {
            switch (trust) {
            case Crypto.Verified:
                return sourceUrl + Nheko.theme.green;
            case Crypto.TOFU:
                return sourceUrl + palette.buttonText;
            case Crypto.Unverified:
            case Crypto.MessageUnverified:
                return sourceUrl + (Settings.mildKeyWarning ? Nheko.theme.orange : Nheko.theme.error);
            default:
                return sourceUrl + Nheko.theme.error;
            }
        } else {
            return sourceUrl + (stateImg.hovered ? unencryptedHoverColor : unencryptedColor);
        }
    }
    width: 16

    HoverHandler {
        id: ma

    }
}
