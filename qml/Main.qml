import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: rootWindow
    width: 1440
    height: 900
    visible: true
    title: "Synera: Synergy Auto-Arena (QML)"

    QtObject {
        id: ui
        property int selectedEquipIndex: -1
    }
    property string actionTip: "Drag units between bench and board."
    property int selectedBoardIndex: -1
    property bool showRanges: true
    property var battleFeed: []

    function selectedBoardUnit() {
        if (selectedBoardIndex < 0 || selectedBoardIndex >= game.boardCells.length)
            return null
        const cell = game.boardCells[selectedBoardIndex]
        if (!cell || !cell.unit || !cell.unit.name)
            return null
        return cell.unit
    }

    function inSelectedRange(cellData) {
        if (!showRanges)
            return false
        const selected = selectedBoardUnit()
        if (!selected)
            return false
        const dx = cellData.x - selected.x
        const dy = cellData.y - selected.y
        return Math.sqrt(dx * dx + dy * dy) <= selected.range + 0.0001
    }

    function boardIndexAt(windowX, windowY) {
        for (let i = 0; i < boardRepeater.count; ++i) {
            const item = boardRepeater.itemAt(i)
            if (!item)
                continue
            const p = item.mapFromItem(contentItem, windowX, windowY)
            if (p.x >= 0 && p.y >= 0 && p.x <= item.width && p.y <= item.height)
                return item.modelData.index
        }
        return -1
    }

    function benchIndexAt(windowX, windowY) {
        for (let i = 0; i < benchRepeater.count; ++i) {
            const item = benchRepeater.itemAt(i)
            if (!item)
                continue
            const p = item.mapFromItem(contentItem, windowX, windowY)
            if (p.x >= 0 && p.y >= 0 && p.x <= item.width && p.y <= item.height)
                return item.modelData.index
        }
        return -1
    }

    function unitColor(unit) {
        if (!unit || !unit.name)
            return "transparent"
        return unit.owner === "Enemy" ? "#ca4b4b" : "#4f77d1"
    }

    Connections {
        target: game
        function onCombatLog(line) {
            const next = battleFeed.slice(0)
            next.unshift(line)
            if (next.length > 60)
                next.pop()
            battleFeed = next
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Frame {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Flow {
                    Layout.fillWidth: true
                    spacing: 10
                    Label { text: "HP: " + game.playerHp }
                    Label { text: "Gold: " + game.gold }
                    Label { text: "Level: " + game.level }
                    Label { text: "Population: " + game.deployedCount + "/" + game.populationCap }
                    Label { text: "Round: " + game.round }
                    Label { text: "Phase: " + game.phase }
                    CheckBox { text: "Show ranges"; checked: showRanges; onToggled: showRanges = checked }
                }

                Label {
                    Layout.fillWidth: true
                    text: "Hint: " + actionTip
                    color: "#b8f2c8"
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        const u = selectedBoardUnit()
                        if (!u)
                            return "Select a board unit to inspect range/role"
                        return "Selected: " + u.name + " (Range " + u.range + ", ATK " + u.atk + ")"
                    }
                    color: "#fff3cd"
                    elide: Text.ElideRight
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { text: "New Game"; onClicked: game.newGame() }
                    Button { text: "Start Combat"; onClicked: game.startCombat() }
                    Button { text: "Refresh Shop (-2)"; onClicked: game.refreshShop() }
                    Button { text: "Upgrade Cap"; onClicked: game.upgradePopulation() }
                    Button { text: "Save"; onClicked: game.saveGame("savegame.json") }
                    Button { text: "Load"; onClicked: game.loadGame("savegame.json") }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Frame {
                Layout.preferredWidth: 760
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter

                Grid {
                    id: boardGrid
                    anchors.centerIn: parent
                    columns: game.cols
                    rows: game.rows
                    spacing: 3

                    Repeater {
                        id: boardRepeater
                        model: game.boardCells
                        delegate: Rectangle {
                            id: boardCell
                            required property var modelData
                            width: 86
                            height: 86
                            radius: 4
                            color: modelData.playerHalf ? "#1f2b45" : "#3b2323"
                            border.width: inSelectedRange(modelData) ? 2 : 1
                            border.color: inSelectedRange(modelData) ? "#ffd54f" : "#6d7a8f"

                            Rectangle {
                                id: boardPiece
                                x: 4
                                y: 4
                                width: parent.width - 8
                                height: parent.height - 8
                                radius: 4
                                color: unitColor(modelData.unit)
                                visible: !!(modelData.unit && modelData.unit.name)
                                border.width: modelData.unit && modelData.unit.state === "Attacking" ? 3
                                               : (modelData.unit && modelData.unit.state === "Casting" ? 3 : 1)
                                border.color: modelData.unit && modelData.unit.state === "Attacking" ? "#ff6e6e"
                                               : (modelData.unit && modelData.unit.state === "Casting" ? "#c38cff" : "#ffffff44")

                                property string fromArea: "board"
                                property int fromIndex: modelData.index
                                property bool isDragging: false

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Label { text: modelData.unit && modelData.unit.name ? modelData.unit.name : ""; color: "white"; font.pixelSize: 10 }
                                    Label { text: modelData.unit && modelData.unit.name ? "★" + modelData.unit.star + "  ATK:" + modelData.unit.atk : ""; color: "white"; font.pixelSize: 9 }
                                    Label { text: modelData.unit && modelData.unit.name ? "HP " + modelData.unit.hp + "/" + modelData.unit.maxHp : ""; color: "white"; font.pixelSize: 9 }
                                    Label { text: modelData.unit && modelData.unit.name ? "MP " + modelData.unit.mana + "/" + modelData.unit.maxMana : ""; color: "white"; font.pixelSize: 9 }
                                    Label {
                                        text: modelData.unit && modelData.unit.lastAction ? modelData.unit.lastAction : ""
                                        color: "#fff59d"
                                        font.pixelSize: 9
                                        visible: text.length > 0
                                    }
                                }

                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: function() {
                                        if (ui.selectedEquipIndex >= 0 && game.phase === "Prep") {
                                            game.equipUnit(ui.selectedEquipIndex, "board", modelData.index)
                                            ui.selectedEquipIndex = -1
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                enabled: boardPiece.visible
                                drag.target: boardPiece
                                drag.threshold: 8
                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                onPressed: {
                                    boardCell.z = 1000
                                    boardPiece.z = 1000
                                    boardPiece.isDragging = true
                                    selectedBoardIndex = modelData.index
                                }
                                onPositionChanged: {
                                    if (!boardPiece.isDragging)
                                        return
                                }
                                onCanceled: {
                                    boardPiece.x = 4
                                    boardPiece.y = 4
                                    boardPiece.z = 0
                                    boardPiece.isDragging = false
                                    boardCell.z = 0
                                }
                                onReleased: {
                                    const center = boardPiece.mapToItem(contentItem, boardPiece.width / 2, boardPiece.height / 2)
                                    const boardIdx = boardIndexAt(center.x, center.y)
                                    const benchIdx = benchIndexAt(center.x, center.y)

                                    boardPiece.x = 4
                                    boardPiece.y = 4
                                    boardPiece.z = 0
                                    boardPiece.isDragging = false
                                    boardCell.z = 0

                                    if (boardIdx >= 0) {
                                        game.moveUnit(boardPiece.fromArea, boardPiece.fromIndex, "board", boardIdx)
                                    } else if (benchIdx >= 0) {
                                        game.moveUnit(boardPiece.fromArea, boardPiece.fromIndex, "bench", benchIdx)
                                    }

                                }
                            }

                            Label {
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                anchors.margins: 2
                                text: modelData.x + "," + modelData.y
                                color: "#d0d8e8"
                                font.pixelSize: 9
                            }

                            Label {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.margins: 2
                                text: modelData.unit && modelData.unit.range ? "R" + modelData.unit.range : ""
                                color: "#ffe082"
                                font.pixelSize: 9
                                visible: !!(modelData.unit && modelData.unit.name)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Frame {
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label { text: "Bench (Drag to board / board to bench)" }

                        RowLayout {
                            id: benchSlotsRow
                            Layout.fillWidth: true
                            spacing: 4
                            Repeater {
                                id: benchRepeater
                                model: game.benchCells
                                delegate: Rectangle {
                                    id: benchCell
                                    required property var modelData
                                    Layout.preferredWidth: 84
                                    Layout.preferredHeight: 84
                                    radius: 4
                                    color: "#2a2a2a"
                                    border.color: "#666"

                                    Rectangle {
                                        id: benchPiece
                                        x: 4
                                        y: 4
                                        width: parent.width - 8
                                        height: parent.height - 8
                                        radius: 4
                                        color: unitColor(modelData.unit)
                                        visible: !!(modelData.unit && modelData.unit.name)

                                        property string fromArea: "bench"
                                        property int fromIndex: modelData.index
                                        property bool isDragging: false

                                        Column {
                                            anchors.centerIn: parent
                                            Label { text: modelData.unit && modelData.unit.name ? modelData.unit.name : ""; color: "white"; font.pixelSize: 10 }
                                            Label { text: modelData.unit && modelData.unit.name ? "★" + modelData.unit.star : ""; color: "white"; font.pixelSize: 9 }
                                        }

                                        TapHandler {
                                            acceptedButtons: Qt.RightButton
                                            onTapped: function() {
                                                if (ui.selectedEquipIndex >= 0 && game.phase === "Prep") {
                                                    game.equipUnit(ui.selectedEquipIndex, "bench", modelData.index)
                                                    ui.selectedEquipIndex = -1
                                                }
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton
                                        enabled: benchPiece.visible
                                        drag.target: benchPiece
                                        drag.threshold: 8
                                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        onPressed: {
                                            benchCell.z = 1000
                                            benchPiece.z = 1000
                                            benchPiece.isDragging = true
                                            selectedBoardIndex = -1
                                        }
                                        onPositionChanged: {
                                            if (!benchPiece.isDragging)
                                                return
                                        }
                                        onCanceled: {
                                            benchPiece.x = 4
                                            benchPiece.y = 4
                                            benchPiece.z = 0
                                            benchPiece.isDragging = false
                                            benchCell.z = 0
                                        }
                                        onReleased: {
                                            const center = benchPiece.mapToItem(contentItem, benchPiece.width / 2, benchPiece.height / 2)
                                            const boardIdx = boardIndexAt(center.x, center.y)
                                            const benchIdx = benchIndexAt(center.x, center.y)

                                            benchPiece.x = 4
                                            benchPiece.y = 4
                                            benchPiece.z = 0
                                            benchPiece.isDragging = false
                                            benchCell.z = 0

                                            if (boardIdx >= 0) {
                                                game.moveUnit(benchPiece.fromArea, benchPiece.fromIndex, "board", boardIdx)
                                            } else if (benchIdx >= 0) {
                                                game.moveUnit(benchPiece.fromArea, benchPiece.fromIndex, "bench", benchIdx)
                                            }

                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Label { text: "Shop (cost 3 each)" }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Repeater {
                                model: game.shopSlots
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 76
                                    radius: 4
                                    color: "#334"
                                    border.color: "#6c7"

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Label { text: modelData.unit.name; color: "white" }
                                        Label { text: modelData.unit.traits.join("/"); color: "#d9e7ff"; font.pixelSize: 10 }
                                        Label { text: "ATK " + modelData.unit.atk + "  HP " + modelData.unit.maxHp; color: "#c7d5f2"; font.pixelSize: 10 }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: game.buyUnit(modelData.index)
                                    }
                                }
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        TabBar {
                            id: infoTabs
                            Layout.fillWidth: true
                            TabButton { text: "Equipment" }
                            TabButton { text: "Synergy" }
                            TabButton { text: "Battle Feed" }
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: infoTabs.currentIndex

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ListView {
                                    anchors.fill: parent
                                    model: game.equipmentSlots
                                    clip: true
                                    delegate: Rectangle {
                                        required property var modelData
                                        width: ListView.view ? ListView.view.width : 320
                                        height: 42
                                        color: ui.selectedEquipIndex === modelData.index ? "#5b7" : "#444"
                                        border.color: "#888"
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: 6
                                            anchors.right: parent.right
                                            anchors.rightMargin: 6
                                            color: "white"
                                            text: modelData.name + "  (+ATK " + modelData.atkBonus + ", +HP " + modelData.hpBonus + ", x" + modelData.speed.toFixed(1) + ", ManaΔ " + modelData.manaDelta + ")"
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: ui.selectedEquipIndex = modelData.index
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ListView {
                                    anchors.fill: parent
                                    model: game.traitSummary
                                    clip: true
                                    delegate: Rectangle {
                                        required property var modelData
                                        width: ListView.view ? ListView.view.width : 320
                                        height: 34
                                        color: "#262626"
                                        border.color: "#555"
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: 6
                                            anchors.right: parent.right
                                            anchors.rightMargin: 6
                                            color: "#e6f0ff"
                                            text: modelData.trait + " [" + modelData.count + "]  threshold " + modelData.threshold + "  " + modelData.effect
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                ListView {
                                    anchors.fill: parent
                                    model: battleFeed
                                    clip: true
                                    delegate: Rectangle {
                                        required property var modelData
                                        width: ListView.view ? ListView.view.width : 320
                                        height: 30
                                        color: "#1f1f1f"
                                        border.color: "#474747"
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: 8
                                            anchors.right: parent.right
                                            anchors.rightMargin: 8
                                            color: "#d7f9ff"
                                            text: modelData
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
