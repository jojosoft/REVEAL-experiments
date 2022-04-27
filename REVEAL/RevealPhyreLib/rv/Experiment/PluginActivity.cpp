#include "PluginActivity.h"

#include <limits>

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	static constexpr const char* kHeaderActivityMarker = "activityMarker";
	static constexpr const char* kHeaderActivityPositionTravelled = "activityPosition";
	static constexpr const char* kHeaderActivityRotationTravelled = "activityRotation";
	static constexpr const char* kHeaderActivityBaseTurns = "activityBaseTurns";

	namespace JsonFieldName
	{
		// This is an optional value that defines the interval in which automatic markers are issued.
		// No automatic markers will be issued if this value is not set in the plug-in's configuration.
		static constexpr const char* kPluginActivityAutoMarkerInterval = "autoMarkerIntervalSeconds";
	};

	CI_issue_activity_marker g_CI_issue_activity_marker;

	PluginActivity::PluginActivity()
		: m_lastHMDMatrix(m4::identity()), m_bIsMonitoring(false),
		m_autoMarkerInterval(std::numeric_limits<float>::infinity())
	{
		// Add all static data fields to the data map.
		add_data_field(kHeaderActivityMarker);
		add_data_field(kHeaderActivityPositionTravelled);
		add_data_field(kHeaderActivityRotationTravelled);
		add_data_field(kHeaderActivityBaseTurns);

		// Reset the auto marker system and helper variables.
		reset_helpers();
		reset_auto_markers();

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginActivity::register_interpreters(Events::CommandBlockManager& rCBManager) const
	{
		rCBManager.register_command_interpreter(Utilities::Name("issue_activity_marker"), &g_CI_issue_activity_marker);
	}

	void PluginActivity::configure_from_json(const Json::Value& jsonData)
	{
		if (jsonData.HasMember(JsonFieldName::kPluginActivityAutoMarkerInterval))
		{
			// [OPTIONAL] A 32 bit floating point value indicating the minimum record interval in seconds.
			m_autoMarkerInterval = jsonData[JsonFieldName::kPluginActivityAutoMarkerInterval].GetFloat();
		}
		else
		{
			// By default, no automatic markers are issued.
			m_autoMarkerInterval = std::numeric_limits<float>::infinity();
		}
	}

	void PluginActivity::reset()
	{
		// Reset all data fields.
		data(kHeaderActivityMarker).reset();
		data(kHeaderActivityPositionTravelled).reset();
		data(kHeaderActivityRotationTravelled).reset();
		data(kHeaderActivityBaseTurns).reset();
		// Reset everything including the last HMD matrix and the monitoring state.
		reset_helpers();
		reset_auto_markers();
		m_lastHMDMatrix = m4::identity();
		m_bIsMonitoring = false;
	}

	Utilities::Name PluginActivity::get_name() const
	{
		return Utilities::Name("activity");
	}

	void PluginActivity::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kGamePlay_OnStepRotate:
		{
			// The player used the controller to rotate their base position.
			m_numberBaseTurns++;
			break;
		}
		case Events::ERevealEventTypes::kExperiment_IssueActivityMarker:
		{
			// Store the marker's name for the next update.
			m_nextMarkerName = Utilities::Name(evt.uUserArg);
			break;
		}
		case Events::ERevealEventTypes::kExperiment_End:
		{
			// Output one last marker with the remaining data.
			m_nextMarkerName = Utilities::Name("End");
			break;
		}
		}
	}

	void PluginActivity::update_internal(const f32 fDeltaTime)
	{
		if (m_bIsMonitoring)
		{
			// Add the position and rotation distances travelled since last frame to the accumulated variables:
			m4 currentHMDMatrix(get_head_matrix());
			{
				using namespace Vectormath::Aos;
				m_positionTravelled += dist(Point3(currentHMDMatrix.getTranslation()), Point3(m_lastHMDMatrix.getTranslation()));
				// For the rotation, just calculate the Euclidian distance between the current and the last sample point.
				// This is enough precision for the small changes in rotation that will on average occur between frames.
				// The sample point is calculated by normalising the point indicated by the gaze vector on -Z (0, 0, -1).
				v3 lastRotationSample = normalize(m_lastHMDMatrix.getUpper3x3() * v3(0, 0, -1));
				v3 currentRotationSample = normalize(currentHMDMatrix.getUpper3x3() * v3(0, 0, -1));
				m_rotationTravelled += dist(Point3(currentRotationSample), Point3(lastRotationSample));
				// [NOTE] Some helpful debug output to validate the calculations....
				//RV_DEBUG_PRINTF("[Activity] Position: %.3f	Rotation: %.3f\n", m_positionTravelled, m_rotationTravelled);
				//RV_DEBUG_PRINTF("[Activity] (%.5f, %.5f, %.5f)\n", lastRotationSample[0].getAsFloat(), lastRotationSample[1].getAsFloat(), lastRotationSample[2].getAsFloat());
			}

			// With auto markers enabled, internally issue a new marker in the given interval!
			m_lastAutoMarkerAge += fDeltaTime;
			if (m_lastAutoMarkerAge >= m_autoMarkerInterval)
			{
				// Subtract the auto marker interval from the age instead of resetting the age.
				// This will keep the overall frame rate of the recording linear and consistent!
				m_lastAutoMarkerAge -= m_autoMarkerInterval;
				// Generate the marker name for this auto marker based on the auto marker counter:
				m_nextMarkerName = Utilities::Name(std::string("Auto").append(std::to_string(m_nextAutoMarkerName++)).c_str());
			}

			// Check if a new marker was issued that waits to be written to the output file:
			if (m_nextMarkerName != Utilities::Name::kInvalidHash)
			{
				// Write the data accumulated until now since the last marker.
				data(kHeaderActivityMarker) = m_nextMarkerName.get_message();
				data(kHeaderActivityPositionTravelled) = std::to_string(m_positionTravelled);
				data(kHeaderActivityRotationTravelled) = std::to_string(m_rotationTravelled);
				data(kHeaderActivityBaseTurns) = std::to_string(m_numberBaseTurns);

				// Reset the next marker variable and all activity variables.
				reset_helpers();
			}

			// Now remember the current HMD matrix for the next update...
			m_lastHMDMatrix = currentHMDMatrix;
		}
		else
		{
			// This is the first time for this recording that an update happens.
			// Copy the HMD matrix once without an analysis to make it the starting point!
			m_lastHMDMatrix = get_head_matrix();
			m_bIsMonitoring = true;
		}
	}

	m4 PluginActivity::get_head_matrix() const
	{
		return GamePlay::g_globalGameState.player().get_camera_track_matrix();
	}

	void PluginActivity::reset_helpers()
	{
		m_nextMarkerName = Utilities::Name::kInvalidHash;
		m_positionTravelled = m_rotationTravelled = 0.0f;
		m_numberBaseTurns = 0;
	}

	void PluginActivity::reset_auto_markers()
	{
		m_lastAutoMarkerAge = 0.0f;
		m_nextAutoMarkerName = 1;
	}

	rv::result_t CI_issue_activity_marker::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		if (rCommandJson.HasMember("marker"))
		{
			rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_IssueActivityMarker;
			rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
			rCmdOut.m_event.uUserArg = Utilities::Name(rCommandJson["marker"].GetString()).get_hash();
		}
		else
		{
			RV_DEBUG_PRINTF("[COMMAND: issue_activity_marker] No \"marker\" (name) was specified.");
			return Result::kParseError;
		}

		return Result::kNoError;
	}

	const char* CI_issue_activity_marker::description() const
	{
		return "Associates accumulated activity data since the last marker with the given marker name and writes it to the output file.";
	}

	const char** CI_issue_activity_marker::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"marker", "The name of the marker the accumulated data since the last marker should be associated with."
		};
		numArgsOut = 1;
		return kArgs;
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
