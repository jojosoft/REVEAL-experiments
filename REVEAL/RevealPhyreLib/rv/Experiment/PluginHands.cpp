#include "PluginHands.h"

#include <algorithm>

#ifdef ENABLE_EXPERIMENT

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	// IMPORTANT: The matrix is represented column-major!
	static constexpr u32 kHeaderHandsMatrixColumns = 4;
	static constexpr u32 kHeaderHandsMatrixRows = 4;
	static constexpr const char* kHeaderHandsMatrix[kHeaderHandsMatrixColumns][kHeaderHandsMatrixRows] =
	{
		{ "HandsMatrixC0R0", "HandsMatrixC0R1", "HandsMatrixC0R2", "HandsMatrixC0R3" },
		{ "HandsMatrixC1R0", "HandsMatrixC1R1", "HandsMatrixC1R2", "HandsMatrixC1R3" },
		{ "HandsMatrixC2R0", "HandsMatrixC2R1", "HandsMatrixC2R2", "HandsMatrixC2R3" },
		{ "HandsMatrixC3R0", "HandsMatrixC3R1", "HandsMatrixC3R2", "HandsMatrixC3R3" }
	};

	namespace JsonFieldName
	{
		// This is a mandatory value that defines the time in seconds between records.
		static constexpr const char* kPluginHandsInterval = "recordIntervalSeconds";
		// This is an optional value that defines whether the recording should start automatically.
		// [NOTE] By default, the plug-in only starts recording if the corresponding command is executed.
		static constexpr const char* kPluginHandsAutoStart = "autoStart";
	};

	CI_start_hands_recording g_CI_start_hands_recording;
	CI_stop_hands_recording g_CI_stop_hands_recording;

	PluginHands::PluginHands()
		: m_defaultInterval(0.04f), m_autoRecord(false)
	{
		// Add all static data fields to the data map.
		auto registerHeader = [this](const char* header) { add_data_field(header); };
		auto headerSequence = &kHeaderHandsMatrix[0][0];
		auto headerCount = kHeaderHandsMatrixColumns * kHeaderHandsMatrixRows;
		std::for_each(headerSequence, headerSequence + headerCount, registerHeader);

		// Reset the helper variables.
		reset_helpers();

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginHands::register_interpreters(Events::CommandBlockManager& rCBManager) const
	{
		rCBManager.register_command_interpreter(Utilities::Name("start_hands_recording"), &g_CI_start_hands_recording);
		rCBManager.register_command_interpreter(Utilities::Name("stop_hands_recording"), &g_CI_stop_hands_recording);
	}

	void PluginHands::configure_from_json(const Json::Value& jsonData)
	{
		RV_ASSERT(jsonData.HasMember(JsonFieldName::kPluginHandsInterval) && "No record interval provided!");
		// A 32 bit floating point value indicating the record interval in seconds.
		m_interval = m_defaultInterval = jsonData[JsonFieldName::kPluginHandsInterval].GetFloat();
		if (jsonData.HasMember(JsonFieldName::kPluginHandsAutoStart))
		{
			// [OPTIONAL] A boolean indicating whether the recording should start when the experiment starts.
			m_recording = m_autoRecord = jsonData[JsonFieldName::kPluginHandsAutoStart].GetBool();
		}
		else
		{
			// By default, only start to record data if the start command is executed.
			m_recording = m_autoRecord = false;
		}
	}

	void PluginHands::reset()
	{
		// Reset all data fields.
		auto resetHeader = [this](const char* header) { data(header).reset(); };
		auto headerSequence = &kHeaderHandsMatrix[0][0];
		auto headerCount = kHeaderHandsMatrixColumns * kHeaderHandsMatrixRows;
		std::for_each(headerSequence, headerSequence + headerCount, resetHeader);
		// Reset all helper variables:
		reset_helpers();
	}

	Utilities::Name PluginHands::get_name() const
	{
		return Utilities::Name("hands");
	}

	void PluginHands::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kExperiment_StartHandsRecording:
		{
			// A negative interval value indicates that the default interval should be used.
			m_interval = evt.fUserArg >= 0.0f ? evt.fUserArg : m_defaultInterval;
			// Reset all data fields for the new recording:
			auto fieldResetter = [this](const char* header) { data(header).reset(); };
			auto headerSequence = &kHeaderHandsMatrix[0][0];
			auto headerCount = kHeaderHandsMatrixColumns * kHeaderHandsMatrixRows;
			std::for_each(headerSequence, headerSequence + headerCount, fieldResetter);
			// Reset the last record delay and start the recording!
			m_lastRecordingDelay = 0.0f;
			m_recording = true;
			break;
		}
		case Events::ERevealEventTypes::kExperiment_StopHandsRecording:
		{
			m_recording = false;
			break;
		}
		}
	}

	void PluginHands::reset_helpers()
	{
		m_interval = m_defaultInterval;
		m_recording = m_autoRecord;
		m_lastRecordingDelay = 0.0f;
	}

	void PluginHands::update_internal(const f32 fDeltaTime)
	{
		// Only check the age of the first data field, as they are only written together.
		f32 nextInterval = m_interval - m_lastRecordingDelay;
		if (m_recording && data(kHeaderHandsMatrix[0][0]).older_than(nextInterval))
		{
			// Remember the difference between the perfect and the actual point in time for this recording.
			// In correspondance with its value, the next recording will be unblocked earlier.
			// This will keep the overall frame rate of the recording linear and consistent!
			m_lastRecordingDelay = data(kHeaderHandsMatrix[0][0]).get_age() - nextInterval;
			// Just write the whole tracking matrix, which should make it unambiguous.
			// Quaternions and Euler angles might produce problems later on...
			auto trackingHandsMatrix = GamePlay::g_globalGameState.player().get_controller_track_matrix();
			for (int c = 0; c < kHeaderHandsMatrixColumns; c++)
			{
				for (int r = 0; r < kHeaderHandsMatrixRows; r++)
				{
					data(kHeaderHandsMatrix[c][r]) = std::to_string(trackingHandsMatrix.getElem(c, r).getAsFloat());
				}
			}
		}
	}

	rv::result_t CI_start_hands_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StartHandsRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
		rCmdOut.m_event.fUserArg = -1.0f;
		if (rCommandJson.HasMember(JsonFieldName::kPluginHandsInterval))
		{
			// The caller provided a new recording interval!
			rCmdOut.m_event.fUserArg = rCommandJson[JsonFieldName::kPluginHandsInterval].GetFloat();
		}

		return Result::kNoError;
	}

	const char* CI_start_hands_recording::description() const
	{
		return "Starts recording the player's hand's (actually the controller's) tracking-space matrix in the specified interval.";
	}

	const char** CI_start_hands_recording::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"recordIntervalSeconds", "Optional: If this argument is not provided, the default interval that the plug-in was configured with is used."
		};
		numArgsOut = 1;
		return kArgs;
	}

	rv::result_t CI_stop_hands_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StopHandsRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_stop_hands_recording::description() const
	{
		return "Stops recording the player's hand's (actually the controller's) tracking-space matrix.";
	}

	const char** CI_stop_hands_recording::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
