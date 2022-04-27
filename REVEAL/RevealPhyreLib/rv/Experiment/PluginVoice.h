#pragma once

#include "rv/RevealConfig.h"
#include "rv/Events/Events.h"
#include "rv/GamePlay/RevealEvents.h"

#include "ExperimentPlugin.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	//! This plug-in records when recordings of the participant's voice are made.
	//! Note that recordings are indicated even if they have actually been prevented!
	//! Reason: The plug-in analyses the event bus and not the experiment manager.
	class PluginVoice : public ExperimentPlugin
	{
	public:

		PluginVoice();

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

		bool m_bRecording;

	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
