#include "PluginVoice.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	static constexpr const char* kHeaderVoiceRecording = "voiceRecording";

	PluginVoice::PluginVoice()
	{
		// Add all static data fields to the data map.
		add_data_field(kHeaderVoiceRecording, DataField(true));

		// Reset the plug-in:
		reset();

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginVoice::reset()
	{
		// Reset all data fields.
		data(kHeaderVoiceRecording) = "FALSE";
		// Reset the recording flag.
		m_bRecording = false;
	}

	Utilities::Name PluginVoice::get_name() const
	{
		return Utilities::Name("voice");
	}

	void PluginVoice::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kExperiment_StartAudioRecording:
		{
			// A voice recording was started.
			// Only update the data field if its value will change!
			if (!m_bRecording)
			{
				data(kHeaderVoiceRecording) = "TRUE";
				m_bRecording = true;
			}
			break;
		}
		case Events::ERevealEventTypes::kExperiment_StopAudioRecording:
		case Events::ERevealEventTypes::kExperiment_End:
		{
			// The current voice recording was stopped.
			// Only update the data field if its value will change!
			if (m_bRecording)
			{
				data(kHeaderVoiceRecording) = "FALSE";
				m_bRecording = false;
			}
			break;
		}
		}
	}

	void PluginVoice::update_internal(const f32 fDeltaTime)
	{
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
