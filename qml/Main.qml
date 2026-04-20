import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    width: 1440
    height: 900
    visible: true
    title: "Synera: Synergy Auto-Arena (QML)"

    property int selectedEquipIndex: -1

    function unitColor(unit) {
        if (!unit || !unit.name)
            return "transparent"
        return unit.owner === "Enemy" ? "#ca4b4b" : "#4f77d1"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Frame {
            Layout.fillWidth: true
            RowLayout {
                anchors.fill: parent
                spacing: 12
                Label { text: "HP: " + game.playerHp }
                Label { text: "Gold: " + game.gold }
                Label { text: "Level: " + game.level }
                Label { text: "Population: " + game.deployedCount + "/" + game.populationCap }
                Label { text: "Round: " + game.round }
                Label { text: "Phase: " + game.phase }
                Label { text: "Prep Left: " + game.prepSecondsLeft + "s" }
                Item { Layout.fillWidth: true }
                Button { text: "New Game"; onClicked: game.newGame() }
                Button { text: "Start Combat"; onClicked: game.startCombat() }
                Button { text: "Refresh Shop (-2)"; onClicked: game.refreshShop() }
                Button { text: "Upgrade Cap"; onClicked: game.upgradePopulation() }
                Button { text: "Save"; onClicked: game.saveGame("savegame.json") }
                Button { text: "Load"; onClicked: game.loadGame("savegame.json") }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Frame {
                Layout.preferredWidth: 760
                Layout.fillHeight: true

                Grid {
                    id: boardGrid
                    columns: game.cols
                    rows: game.rows
                    spacing: 3

                    Repeater {
                        model: game.boardCells
                        delegate: Rectangle {
                            required property var modelData
                            width: 86
                            height: 86
                            radius: 4
                            color: modelData.playerHalf ? "#1f2b45" : "#3b2323"
                            border.width: 1
                            border.color: "#6d7a8f"

                            DropArea {
                                anchors.fill: parent
                                onDropped: function(drop) {
                                    if (drop.source && drop.source.fromArea !== undefined) {
                                        game.moveUnit(drop.source.fromArea, drop.source.fromIndex, "board", modelData.index)
                                    }
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 4
                                radius: 4
                                color: unitColor(modelData.unit)
                                visible: modelData.unit && modelData.unit.name

                                property string fromArea: "board"
                                property int fromIndex: modelData.index

                                Drag.active: unitMouse.drag.active
                                Drag.supportedActions: Qt.MoveAction

                                MouseArea {
                                    id: unitMouse
                                    anchors.fill: parent
                                    drag.target: parent
                                    onReleased: {
                                        parent.x = 4
                                        parent.y = 4
                                    }
                                }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Label { text: modelData.unit.name; color: "white"; font.pixelSize: 10 }
                                    Label { text: "★" + modelData.unit.star + "  ATK:" + modelData.unit.atk; color: "white"; font.pixelSize: 9 }
                                    Label { text: "HP " + modelData.unit.hp + "/" + modelData.unit.maxHp; color: "white"; font.pixelSize: 9 }
                                    Label { text: "MP " + modelData.unit.mana + "/" + modelData.unit.maxMana; color: "white"; font.pixelSize: 9 }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: {
                                        if (selectedEquipIndex >= 0 && game.phase === "Prep") {
                                            game.equipUnit(selectedEquipIndex, "board", modelData.index)
                                            selectedEquipIndex = -1
                                        }
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
                    Label { text: "Bench (Drag to board / board to bench)" }
                    Row {
                        spacing: 4
                        Repeater {
                            model: game.benchCells
                            delegate: Rectangle {
                                required property var modelData
                                width: 84
                                height: 84
                                radius: 4
                                color: "#2a2a2a"
                                border.color: "#666"

                                DropArea {
                                    anchors.fill: parent
                                    onDropped: function(drop) {
                                        if (drop.source && drop.source.fromArea !== undefined) {
                                            game.moveUnit(drop.source.fromArea, drop.source.fromIndex, "bench", modelData.index)
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    radius: 4
                                    color: unitColor(modelData.unit)
                                    visible: modelData.unit && modelData.unit.name

                                    property string fromArea: "bench"
                                    property int fromIndex: modelData.index

                                    Drag.active: benchMouse.drag.active
                                    Drag.supportedActions: Qt.MoveAction

                                    MouseArea {
                                        id: benchMouse
                                        anchors.fill: parent
                                        drag.target: parent
                                        onReleased: {
                                            parent.x = 4
                                            parent.y = 4
                                        }
                                    }

                                    Column {
                                        anchors.centerIn: parent
                                        Label { text: modelData.unit.name; color: "white"; font.pixelSize: 10 }
                                        Label { text: "★" + modelData.unit.star; color: "white"; font.pixelSize: 9 }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: {
                                            if (selectedEquipIndex >= 0 && game.phase === "Prep") {
                                                game.equipUnit(selectedEquipIndex, "bench", modelData.index)
                                                selectedEquipIndex = -1
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
                    Label { text: "Shop (cost 3 each)" }
                    Row {
                        spacing: 4
                        Repeater {
                            model: game.shopSlots
                            delegate: Rectangle {
                                required property var modelData
                                width: 120
                                height: 76
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

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    RowLayout {
                        anchors.fill: parent
                        spacing: 10

                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 260
                            Label { text: "Equipment Pool (click then right-click unit to equip)" }
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: game.equipmentSlots
                                delegate: Rectangle {
                                    required property var modelData
                                    width: parent.width
                                    height: 42
                                    color: selectedEquipIndex === modelData.index ? "#5b7" : "#444"
                                    border.color: "#888"
                                    Text {
                                        anchors.centerIn: parent
                                        color: "white"
                                        text: modelData.name + "  (+ATK " + modelData.atkBonus + ", +HP " + modelData.hpBonus + ", x" + modelData.speed.toFixed(1) + ", ManaΔ " + modelData.manaDelta + ")"
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: selectedEquipIndex = modelData.index
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Label { text: "Synergy Summary" }
                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: game.traitSummary
                                delegate: Rectangle {
                                    required property var modelData
                                    width: parent.width
                                    height: 34
                                    color: "#262626"
                                    border.color: "#555"
                                    Text {
                                        anchors.centerIn: parent
                                        color: "#e6f0ff"
                                        text: modelData.trait + " [" + modelData.count + "]  threshold " + modelData.threshold + "  " + modelData.effect
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
