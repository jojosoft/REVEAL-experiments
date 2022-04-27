#pragma once

#include <string>

#include "rv/RevealConfig.h"
#include "rv/Events/Events.h"
#include "rv/Events/CommandBlocks.h"
#include "rv/GamePlay/RevealEvents.h"
#include "rv/Json/JsonDecl.h"
#include "rv/Json/JsonHelpers.h"
#include "rv/GamePlay/GameStates/GameStatesReveal.h"

#include "ExperimentPlugin.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	//! This plug-in continuously records indicators of user activity between user-defined markers.
	//! The gameplay should trigger the marker command regularly in order to control the accumulation.
	//! An alternative is to provide the autoMarkerIntervalSeconds parameter for automatic markers.
	//! Position-related activity is simply the travelled head distance in local tracking space.
	//! Rotation-related activity is the travelled distance of the tip of the normalised gaze vector.
	//! The plug-in starts recording as soon as the experiment starts and waits for issued markers.
	//! For each marker, a new line is written to the output file with the accumulated values since reset.
	//! After that, all values are reset and once again summed up each frame until the next issued marker.
	class PluginActivity : public ExperimentPlugin
	{
	public:

		PluginActivity();

		// Registers special command interpreters for this experiment plug-in.
		// These allow control over when markers are issued an which name they get.
		virtual void register_interpreters(Events::CommandBlockManager& rCBManager) const override;

		// Configure the plug-in from a JSON object.
		virtual void configure_from_json(const Json::Value& jsonData) override;

		// Resets the plug-in's data fields and internal variables.
		// The data fields are set back to their initial values.
		virtual void reset() override;

		// Return the unique name of the plug-in used as an identifier.
		virtual Utilities::Name get_name() const override;

	protected:

		// Updates the specific plug-in logic and any plug-in data dependent on it.
		virtual void update_internal(const f32 fDeltaTime) override;

		// Updates any plug-in data that is dependent on certain events.
		virtual void handle_event(const Events::Event& evt) override;

	private:

		// Returns the current tracking matrix for the HMD.
		m4 get_head_matrix() const;

		// Resets all accumulated values to zero.
		void reset_helpers();

		// Resets the auto marker system.
		void reset_auto_markers();

	private:

		f32 m_positionTravelled;
		f32 m_rotationTravelled;
		u32 m_numberBaseTurns;

		bool m_bIsMonitoring;
		Utilities::Name m_nextMarkerName;
		m4 m_lastHMDMatrix;

		f32 m_autoMarkerInterval;
		f32 m_lastAutoMarkerAge;
		u32 m_nextAutoMarkerName;

	};

	//! Activity plug-in command interpreter for the "issue_activity_marker" command. @ref CommandList "Commands."
	//! This command associates accumulated activity data since the last marker with the given marker name.
	//! A new line with this information is written to the output file.
	//! @ingroup CommandBlockSystem
	class CI_issue_activity_marker : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
