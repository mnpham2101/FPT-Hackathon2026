# Poject goals:

This project aims to implements V2X features that allow communication between vehicles to prevent accidents, optimize traffic. The system must be developed and run on cloud platform to remove dependency on hardware.

# Feature for milestone 1: 

* vehicles establish connection, send and receive message over v2x.
* v2x network updates the presence of vehicle /obstruction, aware of a hazard it cannot see, by relaying another vehicle's perception.
* scenario: Three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's view of C is **blocked by B**, so A's own camera can never detect C. Vehicle B *can* see C, detects it, and **broadcasts that perception to A over V2X**. The result: both A and B display vehicle C and its relative position, even though A never sees C directly.
* GUI **could** display in 3D resembles of 3 vehicles. 

# Future features:

* front end applications running on multiple processes. Home application defines outer frames and buttons, and watch for user commands, orchestrate other apps. 
* when the events requires notification of obstruction such as present of vehicle C in milestone1, another program, sleeping, is waken up and displayed the 3 vehicles.
* switching between 2D and 3D is allowable. 
* custom appearance is allowable. Themes are provided. 
* extremely fast UI behaviors and C2X services. Benchmark tests could run a media app at foreground, and provides 2D or 3D display or notification messages of vehicleC/obstruction/arrival of v2x messages. Reduced performance benchmark criteria could be debated over technical feasibility, trade-offs.
* live object, vehicles detection over live video feed. VehicleC or obstructions comes very fast ( vehicleB travels upto 120km/h, must be able to detect objects within safe distance to stop and has enough time to broadcast message to vehicleA within safe distance behind it ) 
* multiple hidden obstructions detection.
* if mulple obstructions is detected, v2x messages provides single message of obstruction. No broadcast storm. 
* user can opt for less v2x message. 
* user can opt for v2x message with certain critical level. 
* v2x messages include other hazard warning: slippery roads, falling rocks, road holes, road condition, presence of children, police, speed limit, no horn rules, or other road rules, traffic conditions.  