#pragma once

// Enables the experiment and adds the corresponding item to the main menu.
// All related code is excluded when disabled, except for some parts of the event system.
// [NOTE] The experiment branch is no longer non-destructive, so this is not needed any more.
#define ENABLE_EXPERIMENT

// Enables the experiment room, which will then become the new starting location.
#define ENABLE_EXPERIMENT_ROOM

// Starts the experiment with the save game in experiment_data.json if available.
// Press the triangle button and R2 to overwrite this save on the file system.
#define USE_EXPERIMENT_SAVEGAME

// [NOTE] Managed in GameStateExperiment, needs trial_map_dae to be loaded as an asset!
// Enables a map that the user can see their location on in real-time.
//#define ENABLE_MAP
