import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 560
    height: 320
    minimumWidth: 280
    minimumHeight: 160
    visible: true
    title: qsTr("Stopwatch")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool editing: false
    property bool clean: false
    // Every shortcut but the editor's own keys stops while typing a time, so
    // Space and digits go into the field.
    readonly property bool keysLive: !editing
    // Key names stay as they are in every language; the descriptions are
    // translated.
    readonly property var keyRows: [
        ["Space", qsTr("Start or pause")],
        ["E", qsTr("Set the time (or click it)")],
        ["↑ ↓", qsTr("Add or take off a minute")],
        ["Shift ↑ ↓", qsTr("Add or take off an hour")],
        ["R", qsTr("Reset to zero")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["H", qsTr("Hide the buttons")],
        ["F", qsTr("Full screen")],
        ["?", qsTr("Show these keys")],
        ["Q", qsTr("Quit (the time is kept)")]
    ]

    function beginEdit() {
        helpVisible = false
        editor.text = stopwatch.display
        editing = true
        editor.forceActiveFocus()
        editor.selectAll()
    }

    function endEdit() {
        editing = false
        timeArea.forceActiveFocus()
    }

    function resetWithUndo() {
        if (stopwatch.seconds === 0 && !stopwatch.running)
            return
        stopwatch.reset()
        toast.visible = true
        toastTimer.restart()
    }

    Shortcut { enabled: win.keysLive; sequences: ["?", "Shift+?", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { enabled: win.keysLive; sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { enabled: win.keysLive; sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: stopwatch.toggle() }
    Shortcut { enabled: win.keysLive; sequences: ["E", "Return", "Enter"]; context: Qt.ApplicationShortcut; onActivated: win.beginEdit() }
    Shortcut { enabled: win.keysLive; sequence: "R"; context: Qt.ApplicationShortcut; onActivated: win.resetWithUndo() }
    Shortcut { enabled: win.keysLive; sequence: "Up"; context: Qt.ApplicationShortcut; onActivated: stopwatch.adjust(60) }
    Shortcut { enabled: win.keysLive; sequence: "Down"; context: Qt.ApplicationShortcut; onActivated: stopwatch.adjust(-60) }
    Shortcut { enabled: win.keysLive; sequence: "Shift+Up"; context: Qt.ApplicationShortcut; onActivated: stopwatch.adjust(3600) }
    Shortcut { enabled: win.keysLive; sequence: "Shift+Down"; context: Qt.ApplicationShortcut; onActivated: stopwatch.adjust(-3600) }
    Shortcut { enabled: win.keysLive; sequence: "H"; context: Qt.ApplicationShortcut; onActivated: win.clean = !win.clean }
    Shortcut { enabled: win.keysLive; sequence: "F"; context: Qt.ApplicationShortcut; onActivated: win.visibility = win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
    Shortcut { enabled: win.keysLive; sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: stopwatch.undo() }
    Shortcut { enabled: win.keysLive; sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; onActivated: stopwatch.redo() }
    Shortcut { enabled: win.keysLive && win.helpVisible; sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = false }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction. The digits themselves always read left
    // to right.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Math.max(12, Math.min(win.width, win.height) * 0.05)
            spacing: 8

            Item {
                id: timeArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                focus: true

                // No animation anywhere: the text changes once a second and
                // the scene graph draws only then.
                Text {
                    id: time
                    anchors.fill: parent
                    visible: !win.editing
                    text: stopwatch.display
                    color: stopwatch.running ? "#f2f2f2" : theme.accent
                    font.pixelSize: 900
                    font.features: { "tnum": 1 }
                    minimumPixelSize: 10
                    fontSizeMode: Text.Fit
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    LayoutMirroring.enabled: false

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.IBeamCursor
                        onClicked: win.beginEdit()
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 520)
                    visible: win.editing
                    spacing: 6

                    TextField {
                        id: editor
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignHCenter
                        font.pixelSize: Math.max(18, Math.min(timeArea.height * 0.32, 64))
                        font.features: { "tnum": 1 }
                        inputMethodHints: Qt.ImhPreferNumbers
                        onAccepted: if (stopwatch.setTime(text)) win.endEdit()
                        Keys.onEscapePressed: win.endEdit()
                    }

                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: stopwatch.error ? "#ff6b6b" : "#9a9a9a"
                        text: stopwatch.error ? stopwatch.error
                                              : qsTr("Type a start time, like 1:30:00 or 45m. Enter sets it, Esc cancels.")
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: !win.clean && !win.editing
                spacing: 8

                Button {
                    highlighted: true
                    text: stopwatch.running ? qsTr("Pause") : (stopwatch.seconds > 0 ? qsTr("Resume") : qsTr("Start"))
                    onClicked: stopwatch.toggle()
                    focusPolicy: Qt.NoFocus
                }
                Button {
                    flat: true
                    text: qsTr("Set time")
                    onClicked: win.beginEdit()
                    focusPolicy: Qt.NoFocus
                }
                Button {
                    flat: true
                    text: qsTr("Reset")
                    enabled: stopwatch.running || stopwatch.seconds > 0
                    onClicked: win.resetWithUndo()
                    focusPolicy: Qt.NoFocus
                }
                Label {
                    text: qsTr("? for keys")
                    color: "#6f6f6f"
                    Layout.leftMargin: 4
                }
            }
        }

        Rectangle {
            id: toast
            visible: false
            anchors.horizontalCenter: parent.horizontalCenter
            // At the top, clear of the buttons it would otherwise cover.
            anchors.top: parent.top
            anchors.topMargin: 12
            width: toastLabel.implicitWidth + 28
            height: toastLabel.implicitHeight + 16
            radius: Math.min(4, theme.radius)
            color: "#26262a"

            Label {
                id: toastLabel
                anchors.centerIn: parent
                text: qsTr("Reset. Ctrl+Z brings the time back.")
            }

            Timer { id: toastTimer; interval: 4000; onTriggered: toast.visible = false }
        }

        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#e60e0e10"

            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            GridLayout {
                anchors.centerIn: parent
                columns: 2
                columnSpacing: 18
                rowSpacing: 4

                Repeater {
                    model: win.keyRows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        text: modelData[0]
                        color: theme.accent
                        font.bold: true
                        Layout.row: index
                        Layout.column: 0
                        Layout.alignment: Qt.AlignRight
                    }
                }
                Repeater {
                    model: win.keyRows
                    delegate: Label {
                        required property var modelData
                        required property int index
                        text: modelData[1]
                        Layout.row: index
                        Layout.column: 1
                    }
                }
            }
        }
    }
}
