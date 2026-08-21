# The road ahead

## The challenges with V2X
- CPM standardizes what fields a perceived object carries, not what counts as dangerous — ETSI leaves risk assessment to the receiving OEM. With no universal agreement on safe-distance thresholds across roads, traffic rules, or driving styles, two vehicles can react very differently to the same CPM.
- Warning is based on a fixed highway speed and stopping distance. Dynamic warning adds complexity.
- The broadcast-storm issue is not yet addressed.
- Difficulty telling which CPM comes from which vehicle, needed to accurately map traffic conditions. CPM message `stationID` doesn't tell us that station is actually vehicle B ahead of us 

## The limitation at ADA
- Our distance-to-next-vehicle calculation from video is inaccurate.
- Machine learning is not yet applied to the live camera feed.
- The calculation needs input from other sensors, such as lidar, instead of relying on computer vision alone.
- There are other risk categories, such as unseen obstructions at corners, slippery roads, and foggy conditions.

## Room for improvement in IVI
- True 3D rendering is not yet implemented in this milestone and should be a priority to portray road conditions — it is already a common feature in industry.
- Camera feed should be displayed on the IVI screen.

![Tesla FSD visualization: rendered 3D surrounding vehicles on the highway, with a degraded-conditions warning banner](assets/tesla-fsd-visualization-reference.jpg)
