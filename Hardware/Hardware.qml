pragma Singleton

import QtQuick 2.4

Item {
    property bool leftFrontDoorOpen: false
    property bool rightFrontDoorOpen: false
    property bool leftRearDoorOpen: false
    property bool rightRearDoorOpen: false
    property bool engineProblem: false
    property bool batteryProblem: true
    property bool handbrakeOn: false
    property bool seatbeltOn: true
    property bool frontLightsOn: true
    property bool frontFogLightsOn: false
    property bool frontLongRangeLightsOn: false
    property bool rearFogLightsOn: false
    property bool leftTurnLightsOn: false
    property bool rightTurnLightsOn: false
    property bool sideLightsOn: false

    property real speed: HardwareClass.speed
    onSpeedChanged:console.log("speed:", speed)
    property real maxSpeed: 240
    property real fuelValue: 0.90
    property real maxFuelValue: 1.0
    property real minFuelValue: 0.0

    property real rpm: HardwareClass.rpm
    onRpmChanged:console.log("rpm:", rpm)
    property real maxRpm: 8
    property string gear: "D"
    property real oilTempValue: 0.36
    property real maxOilTempValue: 1.0
    property real minOilTempValue: 0.0

    property int outsideTemperature: +27
    property string temperatureUnit: "C"

    Behavior on rpm {
        NumberAnimation { duration: 200 }
    }

    Behavior on speed {
        NumberAnimation { duration: 300 }
    }

    Connections {
        target: HardwareClass
        onDataReceived: { // (targetId, value)
            console.log("targetId:", targetId, "value:", value)

            switch (targetId) {
                case "81": speed = value; break;
                case "37": rpm = value; break;
                default: break;
            }
        }
    }
}
