# Prompt — spawn 2 project-researchers, requirement debate (round 2)

> Saved verbatim on 2026-07-08 on user request.

spawn 2 project-researchers
* read `project_goals.md`, note Future features,
* read  `m1-coorperative-awareness.md`'s section 1, pay special attention to  ### Milestone 1 development goals, the diagram in ### System Design. ### ECU responsibility, ### Input constraints
* study and debate on project feasibility, implementation tools, then enummerate the requirements, optimize the requirements. Make sure to provide precise information as required in skills

* for Perception Track, clarify if CUDA should be used, or Python can cut it.
* for display track: this place C on B is not needed. ghost C on A's BEV is needed as the development environment also allow so. Suggest the framework and programming language to use.
* use SKILL
* save this prompt

## Addendum (same day, display-track re-debate)

Don't want to use js and websocket on an embedded project. The GUI, displaying 3 cars should be running on a virtual ECU. According to the system diagram, it is IVI ECU.

* focus on using {slint + rust}, or {react} or {Dart + cpp and flutter framework} which is highly referred.
* when analysing development tool, tech stack for GUI, focus on 3D , 2D capability, then aesthetic.
* the GUI only needs to display some button, central view window, If there is a warning message from ADA, then the central view window switch to 2 display 2D, or 3D view of 3 cars.
* central view window may also display camera live feed. (put this on future requirements)

2 Project_researchers should research and re-debate on the display track.
