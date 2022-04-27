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

	//! This plug-in records the local controller matrix in tracking space.
	//! There is no relation to the game or the VRPlayer, it's just the tracking.
	//! [NOTE] Just a duplication of PluginHMD. Not very nice software engineering!
	//! To avoid this, the plug-in base class could offer more common functionality.
	class PluginHands : public ExperimentPlugin
	{
	public:

		PluginHands();

		// Registers special command interpreters for this experiment plug-in.
		// These allow starting and stopping the HMD recording respectively.
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

		// Resets the recording interval and recording flag back to default.
		void reset_helpers();

	private:

		f32 m_interval;
		f32 m_defaultInterval;
		f32 m_lastRecordingDelay;
		bool m_recording;
		bool m_autoRecord;

	};

	//! Hands plug-in command interpreter for the "start_hands_recording" command. @ref CommandList "Commands."
	//! This command starts recording the player's hand's (actually the controller's) tracking-space matrix in the specified interval.
	//! @ingroup CommandBlockSystem
	class CI_start_hands_recording : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Hands plug-in command interpreter for the "stop_hands_recording" command. @ref CommandList "Commands."
	//! This command stops recording the player's hand's (actually the controller's) tracking-space matrix.
	//! @ingroup CommandBlockSystem
	class CI_stop_hands_recording : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
