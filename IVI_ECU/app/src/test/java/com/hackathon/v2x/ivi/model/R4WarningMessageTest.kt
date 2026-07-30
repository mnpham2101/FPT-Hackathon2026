package com.hackathon.v2x.ivi.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class R4WarningMessageTest {
    @Test
    fun decodesAdaR4WarningAndMapsToSceneGeometry() {
        val payload = """
            {
              "schemaVersion":1,
              "type":"warning",
              "warningType":"nlos_obstruction",
              "riskState":"warning",
              "object":{
                "id":"v2x:1201:7",
                "class":"vehicle",
                "source":"v2x_relayed",
                "position":{"x":25.0,"y":1.2,"confidence":0.9},
                "distance":25.0,
                "speed":15.2,
                "confidence":0.95,
                "state":"tracked",
                "timestamps":{"measured":1,"received":1,"lastUpdated":1}
              },
              "trackedObjects":[
                {
                  "id":"own:B",
                  "class":"vehicle",
                  "source":"own_sensor",
                  "position":{"x":18.0,"y":0.0,"confidence":0.5},
                  "distance":18.0,
                  "speed":0.0,
                  "confidence":0.5,
                  "state":"tracked",
                  "timestamps":{"measured":1,"received":1,"lastUpdated":1}
                },
                {
                  "id":"v2x:1201:7",
                  "class":"vehicle",
                  "source":"v2x_relayed",
                  "position":{"x":25.0,"y":1.2,"confidence":0.9},
                  "distance":25.0,
                  "speed":15.2,
                  "confidence":0.95,
                  "state":"tracked",
                  "timestamps":{"measured":1,"received":1,"lastUpdated":1}
                }
              ],
              "geometry":{
                "ego":{"x":0.0,"y":0.0},
                "b":{"x":18.0,"y":0.0},
                "c":{"x":43.0,"y":1.2}
              }
            }
        """.trimIndent()

        val message = R4WarningMessage.decode(payload)
        val scene = message.toSceneGeometry()

        assertEquals("warning", message.riskState)
        assertEquals(18.0f, scene.vehicleB.x)
        assertEquals(43.0f, scene.vehicleC?.x)
        assertNotNull(scene.vehicleCSnapshot)
        assertEquals(R3Snapshot.SOURCE_V2X_RELAYED, scene.vehicleCSnapshot?.source)
    }
}
