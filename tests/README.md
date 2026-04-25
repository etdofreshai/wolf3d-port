# Tests

Behavioral tests for the Wolfenstein 3D porting effort.

The intent is to characterize original behavior without modifying `source/original/`, then run equivalent checks against the modern C + SDL3 implementation.

Initial target areas:

- game data file presence and sizes
- map header and map data parsing
- graphics/audio dictionary/header parsing
- fixed-point and math helper behavior
- gameplay state transitions where separable
- renderer-visible intermediate data before SDL output
