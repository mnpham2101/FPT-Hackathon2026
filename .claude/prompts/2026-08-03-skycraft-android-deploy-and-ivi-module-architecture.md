# Skycraft Android deployment methods, and the IVI ECU module architecture diagram

Saved 2026-08-03.

## Prompt

project researcher reads car-sky-platform html again.

research all methods that an android app can be deployed on Skycraft node?
can android apk be built as image, img file to be deployed on car-sky as one the example in the document?

what is AAOS? Invoke project-architecture to make an architural diagram, draw.io, and svg. Make document, include the architecture and very briefly explain the roles of each module, what input it takes and what output it provides. Just project architure diagram, that includes the modules on IVI-ECU, namely:
- AAOS
- module that deserializes message from ADA ECU.
- module to open socket, listen for incoming message.
- module that raise events
- module that performs UI behavior logic, displaying warning screen of vehicles
- anything that needs added.
Project architecture needs to read deploy-ivi-hmi-walkthrough to clarify what INSIDE IVI-ECU needs to be implemented to provide delivery for final verification tests

project-architecture can refer to

## Follow-up

save the prompt
