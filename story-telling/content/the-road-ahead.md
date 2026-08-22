# The road ahead

## The challenges with V2X
- CPM defines the fields, not the danger threshold. Risk assessment is OEM-defined, not standardized.
- Warning logic uses fixed highway speed and stopping distance. Dynamic warning is future work.
- Broadcast storm is not yet addressed.
- CPM `stationID` doesn't map to "this is vehicle B ahead of us".

## The limitation at ADA
- Video-based distance estimate is inaccurate.
- No ML on the live camera feed yet.
- Needs lidar input. Vision alone isn't enough.
- Other risks not covered: blind corners, wet roads, fog.

## Room for improvement in IVI
- No 3D rendering yet. Standard in industry, should be next priority.
- No camera feed on the IVI screen yet.

![Tesla FSD visualization: rendered 3D surrounding vehicles on the highway, with a degraded-conditions warning banner](assets/tesla-fsd-visualization-reference.jpg)
