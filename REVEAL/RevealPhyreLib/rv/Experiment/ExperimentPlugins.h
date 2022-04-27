#pragma once

// This header is meant to coalesce all plug-in headers!
// Use it to include "all" plug-ins available in the system.

#include "PluginController.h"
#include "PluginHMD.h"
#include "PluginLocomotion.h"
#include "PluginActivity.h"
#include "PluginVoice.h"
#include "PluginHands.h"
#include "PluginCollectionCounter.h"

// It can be useful in implementation files to create globals.
// This way, all plug-ins register themselves automatically.

// Only create plug-in instances with the experiment enabled!
#ifdef ENABLE_EXPERIMENT

#ifdef CreatePluginGlobals
namespace rv
{
namespace Experiment
{

	// For each header file, also create a default global instance.
	PluginController g_ExperimentPluginController;
	PluginHMD g_ExperimentPluginHMD;
	PluginLocomotion g_ExperimentPluginLocomotion;
	PluginActivity g_ExperimentPluginActivity;
	PluginVoice g_ExperimentPluginVoice;
	PluginHands g_ExperimentPluginHands;
	PluginCollectionCounter g_ExperimentPluginCollectionCounter;

}
}
#endif
#undef CreatePluginGlobals

#endif // ENABLE_EXPERIMENT
