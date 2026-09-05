.lib "jolt.dll"

; The compiler auto-links jolt.dll only when one of these is referenced

JoltInit(gravity#):"_JoltInit@4"
JoltShutdown():"_JoltShutdown@0"
JoltIsActive%():"_JoltIsActive@0"
JoltVersion$():"_JoltVersion@0"
