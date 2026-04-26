# NotGate

This is an entry to Ludum Dare 59 - Signal, developed by MAII, with [raylib](https://www.raylib.com/).

## How to play
`W/A/S/D` to move. `Z` to undo. `R` to reset current level.

## Map Editing
Put your new maps or edit existing maps at `assets/maps/mapxx.txt` where `xx` is a number. The maps will be loaded in order.

+ \[space\] - nothing
+ \# - wall
+ @ - player
+ o - box
+ s - signal source
+ S - fixed signal source
+ P - portal
+ r - repeater
+ R - fixed repeater
+ n - not-gate signal source
+ N - fixed not-gate signal source

## Commands
Type `/xx` to load and jump to `mapxx.txt`.

Type `/solve` to trying to find out whether there is a solution to the map automatically.
