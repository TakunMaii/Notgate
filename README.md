# Repeater and NotGate

This is an entry to Ludum Dare 59.

## How to play
`W/A/S/D` to move. `Z` to undo.

## Map Editing
Put or edit your map at `assets/maps/mapxx.txt` where `xx` is a number. The maps will be loaded in order.

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