#include "PluginHMD.h"

#include <algorithm>

#ifdef ENABLE_EXPERIMENT

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	// IMPORTANT: The matrix is represented column-major!
	static constexpr u32 kHeaderHMDMatrixColumns = 4;
	static constexpr u32 kHeaderHMDMatrixRows = 4;
	static constexpr const char* kHeaderHMDMatrix[kHeaderHMDMatrixColumns][kHeaderHMDMatrixRows] =
	{
		{ "HMDMatrixC0R0", "HMDMatrixC0R1", "HMDMatrixC0R2", "HMDMatrixC0R3" },
		{ "HMDMatrixC1R0", "HMDMatrixC1R1", "HMDMatrixC1R2", "HMDMatrixC1R3" },
		{ "HMDMatrixC2R0", "HMDMatrixC2R1", "HMDMatrixC2R2", "HMDMatrixC2R3" },
		{ "HMDMatrixC3R0", "HMDMatrixC3R1", "HMDMatrixC3R2", "HMDMatrixC3R3" }
	};

	namespace JsonFieldName
	{
		// This is a mandatory value that defines the time in seconds between records.
		static constexpr const char* kPluginHMDInterval = "recordIntervalSeconds";
		// This is an optional value that defines whether the recording should start automatically.
		// [NOTE] By default, the plug-in only starts recording if the corresponding command is executed.
		static constexpr const char* kPluginHMDAutoStart = "autoStart";
	};

	CI_start_hmd_recording g_CI_start_hmd_recording;
	CI_stop_hmd_recording g_CI_stop_hmd_recording;

	PluginHMD::PluginHMD()
		: m_defaultInterval(0.04f), m_autoRecord(false)
	{
		// Add all static data fields to the data map.
		auto registerHeader = [this](const char* header) { add_data_field(header); };
		auto headerSequence = &kHeaderHMDMatrix[0][0];
		auto headerCount = kHeaderHMDMatrixColumns * kHeaderHMDMatrixRows;
		std::for_each(headerSequence, headerSequence + headerCount, registerHeader);

		// Reset the helper variables.
		reset_helpers();

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginHMD::register_interpreters(Events::CommandBlockManager& rCBManager) const
	{
		rCBManager.register_command_interpreter(Utilities::Name("start_hmd_recording"), &g_CI_start_hmd_recording);
		rCBManager.register_command_interpreter(Utilities::Name("stop_hmd_recording"), &g_CI_stop_hmd_recording);
	}

	void PluginHMD::configure_from_json(const Json::Value& jsonData)
	{
		RV_ASSERT(jsonData.HasMember(JsonFieldName::kPluginHMDInterval) && "No record interval provided!");
		// A 32 bit floating point value indicating the record interval in seconds.
		m_interval = m_defaultInterval = jsonData[JsonFieldName::kPluginHMDInterval].GetFloat();
		if (jsonData.HasMember(JsonFieldName::kPluginHMDAutoStart))
		{
			// [OPTIONAL] A boolean indicating whether the recording should start when the experiment starts.
			m_recording = m_autoRecord = jsonData[JsonFieldName::kPluginHMDAutoStart].GetBool();
		}
		else
		{
			// By default, only start to record data if the start command is executed.
			m_recording = m_autoRecord = false;
		}
	}

	void PluginHMD::reset()
	{
		// Reset all data fields.
		auto resetHeader = [this](const char* header) { data(header).reset(); };
		auto headerSequence = &kHeaderHMDMatrix[0][0];
		auto headerCount = kHeaderHMDMatrixColumns * kHeaderHMDMatrixRows;
		std::for_each(headerSequence, headerSequence + headerCount, resetHeader);
		// Reset all helper variables:
		reset_helpers();
	}

	Utilities::Name PluginHMD::get_name() const
	{
		return Utilities::Name("HMD");
	}

	void PluginHMD::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kExperiment_StartHMDRecording:
		{
			// A negative interval value indicates that the default interval should be used.
			m_interval = evt.fUserArg >= 0.0f ? evt.fUserArg : m_defaultInterval;
			// Reset all data fields for the new recording:
			auto fieldResetter = [this](const char* header) { data(header).reset(); };
			auto headerSequence = &kHeaderHMDMatrix[0][0];
			auto headerCount = kHeaderHMDMatrixColumns * kHeaderHMDMatrixRows;
			std::for_each(headerSequence, headerSequence + headerCount, fieldResetter);
			// Reset the last record delay and start the recording!
			m_lastRecordingDelay = 0.0f;
			m_recording = true;
			break;
		}
		case Events::ERevealEventTypes::kExperiment_StopHMDRecording:
		{
			m_recording = false;
			break;
		}
		}
	}

	void PluginHMD::reset_helpers()
	{
		m_interval = m_defaultInterval;
		m_recording = m_autoRecord;
		m_lastRecordingDelay = 0.0f;
	}

	void PluginHMD::update_internal(const f32 fDeltaTime)
	{
		// Only check the age of the first data field, as they are only written together.
		f32 nextInterval = m_interval - m_lastRecordingDelay;
		if (m_recording && data(kHeaderHMDMatrix[0][0]).older_than(nextInterval))
		{
			// Remember the difference between the perfect and the actual point in time for this recording.
			// In correspondance with its value, the next recording will be unblocked earlier.
			// This will keep the overall frame rate of the recording linear and consistent!
			m_lastRecordingDelay = data(kHeaderHMDMatrix[0][0]).get_age() - nextInterval;
			// Just write the whole tracking matrix, which should make it unambiguous.
			// Quaternions and Euler angles might produce problems later on...
			auto trackingHMDMatrix = GamePlay::g_globalGameState.player().get_camera_track_matrix();
			for (int c = 0; c < kHeaderHMDMatrixColumns; c++)
			{
				for (int r = 0; r < kHeaderHMDMatrixRows; r++)
				{
					data(kHeaderHMDMatrix[c][r]) = std::to_string(trackingHMDMatrix.getElem(c, r).getAsFloat());
				}
			}
		}
	}

	rv::result_t CI_start_hmd_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StartHMDRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
		rCmdOut.m_event.fUserArg = -1.0f;
		if (rCommandJson.HasMember(JsonFieldName::kPluginHMDInterval))
		{
			// The caller provided a new recording interval!
			rCmdOut.m_event.fUserArg = rCommandJson[JsonFieldName::kPluginHMDInterval].GetFloat();
		}

		return Result::kNoError;
	}

	const char* CI_start_hmd_recording::description() const
	{
		return "Starts recording the player's HMD's tracking-space matrix and optionally sets the record interval.";
	}

	const char** CI_start_hmd_recording::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"recordIntervalSeconds", "Optional: If this argument is not provided, the default interval that the plug-in was configured with is used."
		};
		numArgsOut = 1;
		return kArgs;
	}

	rv::result_t CI_stop_hmd_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StopHMDRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_stop_hmd_recording::description() const
	{
		return "Stops recording the player's HMD's tracking-space matrix.";
	}

	const char** CI_stop_hmd_recording::arguments(u32& numArgsOut) const
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
