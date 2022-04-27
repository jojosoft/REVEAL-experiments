#include "ExperimentManager.h"

#include <ctime>
#include <Phyre.h>
#include <Framework/PhyreFramework.h>
#include <fstream>
#include <audioin.h>
#include <user_service.h>
#include <AudioFile/AudioFile.cpp>

#include "rv/GamePlay/RevealEvents.h"
#include "rv/GamePlay/GameStates/GameStatesReveal.h"
#include "rv/GamePlay/vr_player.h"
#include "rv/Json/JsonDecl.h"
#include "rv/Json/JsonHelpers.h"
#include "rv/UI/rv_UIManager.h"
#include "rv/Utilities/string.h"
#include "rv/Utilities/rv_types.h"
#include "rv/FileSystem/FileReader.h"
#include "rv/Framework/PaperArtifactRenderer.h"
#include "rv/Input/rv_input_utilities.h"

#ifdef ENABLE_EXPERIMENT

// The plug-in register has to be included in this implementation file!
// Right after the initialisation of the static plug-in register.
namespace rv
{
namespace Experiment
{

	ExperimentManager::PluginRegister ExperimentManager::m_sAvailablePlugins;

}
}
// Create plug-in globals. The define will be undefined automatically.
#define CreatePluginGlobals
#include "ExperimentPlugins.h"

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	std::ostream& operator <<(std::ostream& s, const ConditionValue& v)
	{
		switch (v.type)
		{
		case ConditionValue::kInteger:
			s << v.integer;
			break;
		case ConditionValue::kString:
			s << v.stringHash.get_message();
			break;
		default:
			s << "NA";
			break;
		}
		return s;
	}

	namespace JsonFieldName
	{
		// This is an optional value that defines what to record in case a value is undefined.
		// [NOTE] By default, this adapts the R standard of NA for any undefined values.
		static constexpr const char* kExperimentUndefinedValue = "undefinedValue";
		// This is an optional array that contains conditions relevant to each line of the output.
		static constexpr const char* kExperimentConditions = "conditions";
		static constexpr const char* kExperimentConditionsName = "name";
		static constexpr const char* kExperimentConditionsValue = "value";
		// This is an optional array that contains triggers which react on the participant number.
		// The rotate interval defines how many participants in a row will be assigned one command block.
		// Commands are rotated endlessly over the whole range of possible integers (participant numbers).
		static constexpr const char* kExperimentTriggers = "triggers";
		static constexpr const char* kExperimentTriggersName = "name";
		static constexpr const char* kExperimentTriggersCommandBlocks = "commandBlocks";
		static constexpr const char* kExperimentTriggersRotateInterval = "participantRotateInterval";
		// This is an optional array that contains activated plug-ins and their configuration.
		static constexpr const char* kExperimentPlugins = "plugins";
		static constexpr const char* kExperimentPluginName = "name";
		// This is an optional value that indicates whether audio recordings should be possible.
		// [NOTE] Audio commands will not work if this value is not explicitly configured to be true!
		static constexpr const char* kExperimentAudioRecording = "enableAudioRecording";
	};

	CI_set_experiment_condition g_CI_set_experiment_condition;
	CI_increment_experiment_condition g_CI_increment_experiment_condition;
	std::vector<ExperimentArgs> g_CI_experiment_args_bank;
	CI_experiment_trigger g_CI_experiment_trigger;
	CI_start_experiment g_CI_start_experiment;
	CI_end_experiment g_CI_end_experiment;
	CI_abort_experiment g_CI_abort_experiment;
	CI_start_audio_recording g_CI_start_audio_recording;
	CI_stop_audio_recording g_CI_stop_audio_recording;
	CI_start_controller_check g_CI_start_controller_check;

	ExperimentManager::ExperimentManager()
	{
		m_sAvailablePlugins.reserve(m_sAvailablePlugins.size());
	}

	void ExperimentManager::register_interpreters(Events::CommandBlockManager& rCBManager)
	{
		rCBManager.register_command_interpreter(Utilities::Name("set_experiment_condition"), &g_CI_set_experiment_condition);
		rCBManager.register_command_interpreter(Utilities::Name("increment_experiment_condition"), &g_CI_increment_experiment_condition);
		rCBManager.register_command_interpreter(Utilities::Name("experiment_trigger"), &g_CI_experiment_trigger);
		rCBManager.register_command_interpreter(Utilities::Name("start_experiment"), &g_CI_start_experiment);
		rCBManager.register_command_interpreter(Utilities::Name("end_experiment"), &g_CI_end_experiment);
		rCBManager.register_command_interpreter(Utilities::Name("abort_experiment"), &g_CI_abort_experiment);
		rCBManager.register_command_interpreter(Utilities::Name("start_audio_recording"), &g_CI_start_audio_recording);
		rCBManager.register_command_interpreter(Utilities::Name("stop_audio_recording"), &g_CI_stop_audio_recording);
		rCBManager.register_command_interpreter(Utilities::Name("start_controller_check"), &g_CI_start_controller_check);
		// Also give all available plug-ins the opportunity to register their own commands.
		for (auto plugin : m_sAvailablePlugins)
		{
			plugin.second->register_interpreters(rCBManager);
		}
	}

	void ExperimentManager::init()
	{
		// Make sure that the invalid participant number is outside the range of usable numbers!
		static_assert(kMaximumParticipantNumber < kInvalidParticipantNumber, "Maximum participant number outside valid range!");

		// Register as event observer on the gameplay and experiment channel.
		Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kGameplayChannel, this);
		Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kExperimentChannel, this);

		// Reset the experiment manager.
		reset();
	}

	void ExperimentManager::configure_from_json_file(const char* pstrFilePath, Memory::MemAllocator& rAllocator)
	{
		FileSystem::FileReader reader;
		result_t loadResult = reader.load(pstrFilePath, rAllocator, 1, 16);
		RV_VERIFY(rv::Result::kNoError == loadResult);

		Json::Document doc;
		u32 size = static_cast<u32>(reader.block().size());
		memtype_t* data = reader.block_mutable().data_mutable();
		loadResult = Json::parse_json_data_inplace(data, size, doc);
		RV_VERIFY(Result::kNoError == loadResult);
		configure_from_json(doc.GetObject());
	}

	void ExperimentManager::configure_from_json(const Json::Value& jsonData)
	{
		RV_ASSERT(!m_isRunning);
		if (jsonData.HasMember(JsonFieldName::kExperimentUndefinedValue))
		{
			// [OPTIONAL] The string to write into the output file if a value is undefined.
			m_undefinedValue = jsonData[JsonFieldName::kExperimentUndefinedValue].GetString();
		}
		else
		{
			// This supports statistical analysis with R by default.
			// https://www.rdocumentation.org/packages/base/versions/3.5.0/topics/NA
			m_undefinedValue = "NA";
		}
		// Clear the condition value map.
		m_conditionDefaults.clear();
		if (jsonData.HasMember(JsonFieldName::kExperimentConditions))
		{
			// [OPTIONAL] Names of all experiment conditions to consider.
			// Supply objects in the "conditions" array that contain at least a "name" field.
			// Their initial value will be undefined if no default "value" was specified.
			auto conditions = jsonData[JsonFieldName::kExperimentConditions].GetArray();
			for (Json::Value::ConstValueIterator it = conditions.Begin(); it != conditions.End(); ++it)
			{
				auto condition = it->GetObject();
				const char* name = condition[JsonFieldName::kExperimentConditionsName].GetString();
				ConditionValue defaultValue;
				if (condition.HasMember(JsonFieldName::kExperimentConditionsValue))
				{
					// The default value is optional. It can be either a string or a signed 32 bit integer.
					// This is to make things easier, since all other types would have to be implemented separately.
					// Just try to keep the experiment design simple, there is no need to have a float condition.
					defaultValue = ConditionValue(condition[JsonFieldName::kExperimentConditionsValue]);
				}
				// Now add the new condition to the map:
				add_experiment_condition(name, defaultValue);
			}
		}
		// Clear the map of triggers.
		m_triggers.clear();
		if (jsonData.HasMember(JsonFieldName::kExperimentTriggers))
		{
			// [OPTIONAL] Triggers that the experiment manager can execute when requested.
			// On execution, one of the given command blocks is chosen depending on the participant number.
			auto triggers = jsonData[JsonFieldName::kExperimentTriggers].GetArray();
			for (Json::Value::ConstValueIterator it = triggers.Begin(); it != triggers.End(); ++it)
			{
				auto trigger = it->GetObject();
				const char* name = trigger[JsonFieldName::kExperimentTriggersName].GetString();
				u32 prI = trigger[JsonFieldName::kExperimentTriggersRotateInterval].GetUint();
				Trigger currentTrigger(prI);
				// The command blocks represent the different possibilities that will be chosen from.
				auto commandBlocks = trigger[JsonFieldName::kExperimentTriggersCommandBlocks].GetArray();
				for (Json::Value::ConstValueIterator it = commandBlocks.Begin(); it != commandBlocks.End(); ++it)
				{
					currentTrigger.commands.push_back(Utilities::Name(it->GetString()));
				}
				// Now add the new trigger to the map:
				add_experiment_trigger(name, currentTrigger);
			}
		}
		// Disable all active plug-ins.
		auto& act(m_activePlugins);
		std::vector<Utilities::Name> dis(act.size());
		std::for_each(act.cbegin(), act.cend(), [&dis](const ExperimentPlugin* p) { dis.push_back(Utilities::Name(p->get_name())); });
		for (auto it : dis)
		{
			disable_plugin(it);
		}
		if (jsonData.HasMember(JsonFieldName::kExperimentPlugins))
		{
			// [OPTIONAL] Configuration objects for all experiment plug-ins that should be active.
			auto plugins = jsonData[JsonFieldName::kExperimentPlugins].GetArray();
			for (Json::Value::ConstValueIterator it = plugins.Begin(); it != plugins.End(); ++it)
			{
				Utilities::Name pluginName(it->GetObject()[JsonFieldName::kExperimentPluginName].GetString());
				auto pPlugin = enable_plugin(pluginName);
				if (pPlugin)
				{
					// If the plug-in is available and successfully loaded, configure it.
					pPlugin->configure_from_json(*it);
				}
			}
		}
		if (jsonData.HasMember(JsonFieldName::kExperimentAudioRecording))
		{
			// [OPTIONAL] Whether commands for audio recording should be processed.
			m_enableAudioRecording = jsonData[JsonFieldName::kExperimentAudioRecording].GetBool();
		}
		else
		{
			// For privacy reasons, audio recording is disabled by default.
			m_enableAudioRecording = false;
		}
	}

	void ExperimentManager::set_participant(const participant_number_t number)
	{
		RV_ASSERT(!m_isRunning);
		m_currentParticipant = number;
	}

	void ExperimentManager::add_experiment_condition(const char* conditionName, const ConditionValue& value)
	{
		RV_ASSERT(!m_isRunning);
		// Make sure that this conditions does not already exist.
		Utilities::Name condition(conditionName);
		RV_ASSERT(m_conditionDefaults.find(condition) == m_conditionDefaults.end() && "Experiment conditions have to have unique names!");
		m_conditionDefaults.insert(std::make_pair(condition, value));
	}

	void ExperimentManager::remove_experiment_condition(const char* conditionName)
	{
		RV_ASSERT(!m_isRunning);
		// Make sure that this conditions does not already exist.
		Utilities::Name condition(conditionName);
		RV_ASSERT(m_conditionDefaults.find(condition) != m_conditionDefaults.end() && "The requested experiment condition was not found!");
		m_conditionDefaults.erase(condition);
	}

	void ExperimentManager::add_experiment_trigger(const char* triggerName, const Trigger& trigger)
	{
		RV_ASSERT(!m_isRunning);
		// Make sure that this trigger does not already exist.
		Utilities::Name triggerHash(triggerName);
		RV_ASSERT(m_triggers.find(triggerHash) == m_triggers.end() && "Experiment triggers have to have unique names!");
		m_triggers.insert(std::make_pair(triggerHash, trigger));
	}

	void ExperimentManager::remove_experiment_trigger(const char* triggerName)
	{
		RV_ASSERT(!m_isRunning);
		// Make sure that this conditions does not already exist.
		Utilities::Name trigger(triggerName);
		RV_ASSERT(m_triggers.find(trigger) != m_triggers.end() && "The requested experiment trigger was not found!");
		m_triggers.erase(trigger);
	}

	void ExperimentManager::register_plugin(Utilities::Name pluginName, ExperimentPlugin* pPlugin)
	{
		// Check if another instance of this plug-in is already registered.
		auto itPlugin = m_sAvailablePlugins.find(pluginName);
		if (itPlugin == m_sAvailablePlugins.end())
		{
			// Insert the new plug-in into the static map.
			m_sAvailablePlugins.insert(std::make_pair(pluginName, pPlugin));
		}
		else
		{
			// Replace the pointer to the existing plug-in instance with the new one.
			itPlugin->second = pPlugin;
		}
		m_sAvailablePlugins.insert(std::make_pair(pluginName, pPlugin));
	}

	ExperimentPlugin* ExperimentManager::enable_plugin(Utilities::Name pluginName)
	{
		RV_ASSERT(!m_isRunning);
		auto itPlugin = m_sAvailablePlugins.find(pluginName);
		// Check if a plug-in with this name is available.
		if (itPlugin != m_sAvailablePlugins.end())
		{
			// Check that this plug-in is not already active.
			if (std::find(m_activePlugins.begin(), m_activePlugins.end(), itPlugin->second) == m_activePlugins.end())
			{
				// Register the plug-in as an event observer.
				Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kGameplayChannel, itPlugin->second);
				Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kExperimentChannel, itPlugin->second);
				// Add the pointer to the plug-in to the vector of active plug-ins.
				m_activePlugins.push_back(itPlugin->second);
			}
			else
			{
				RV_DEBUG_PRINTF("[ExperimentManager] Warning: The plug-in with name \"%s\" was already active.", pluginName.get_message());
			}
			return itPlugin->second;
		}
		else
		{
			RV_DEBUG_PRINTF("[ExperimentManager] There is no registered plug-in with name \"%s\" available!", pluginName.get_message());
			return nullptr;
		}
	}

	ExperimentPlugin* ExperimentManager::disable_plugin(Utilities::Name pluginName)
	{
		RV_ASSERT(!m_isRunning);
		auto itPlugin = m_sAvailablePlugins.find(pluginName);
		// Check if a plug-in with this name is available.
		if (itPlugin != m_sAvailablePlugins.end())
		{
			// Check that this plug-in is currently active.
			auto itActivePlugin = std::find(m_activePlugins.begin(), m_activePlugins.end(), itPlugin->second);
			if (itActivePlugin != m_activePlugins.end())
			{
				// Remove the pointer to the plug-in from the vector of active plug-ins.
				m_activePlugins.erase(itActivePlugin);
				// Unregister the plug-in as an event observer.
				Events::GEventSystem::instance().unregister_observer(Events::ERevealEventChannels::kGameplayChannel, itPlugin->second);
				Events::GEventSystem::instance().unregister_observer(Events::ERevealEventChannels::kExperimentChannel, itPlugin->second);
			}
			else
			{
				RV_DEBUG_PRINTF("[ExperimentManager] Warning: The plug-in with name \"%s\" was not currently active.", pluginName.get_message());
			}
			return itPlugin->second;
		}
		else
		{
			RV_DEBUG_PRINTF("[ExperimentManager] There is no registered plug-in with name \"%s\" available!", pluginName.get_message());
			return nullptr;
		}
	}

	void ExperimentManager::start()
	{
		// Do not reset the experiment manager here, as it was already done during initialisation.
		// This is because the participant number is set before the experiment is started.
		RV_ASSERT(!m_isRunning);
		RV_ASSERT(m_currentParticipant != kInvalidParticipantNumber);

		// Open output file with the participant number and time in its name.
		char outputPath[128];
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		char dateString[32];
		strftime(dateString, 32, "%A_%d-%m-%Y_%H-%M-%S", timeinfo);
#if defined(RV_PLATFORM_ORBIS) && defined(RV_PACKAGE)
		// [NOTE] All experiment data is written to a USB drive!
		sprintf_s(outputPath, 128, "/usb0/participant_%02d_%s.csv", m_currentParticipant, dateString);
#else
		sprintf_s(outputPath, 128, RV_PATH_LITERAL("Media/Config/participant_%02d_%s.csv"), m_currentParticipant, dateString);
#endif
		m_outputWriter.open(outputPath, std::ios::app);

		// Initialise the condition value vector with the current default condition values.
		m_conditionValues = m_conditionDefaults;

		// Reset all active plug-ins.
		for (auto plugin : m_activePlugins)
		{
			plugin->reset();
		}

		// Write the header line with all currently available conditions and plug-ins.
		m_outputWriter << "participant" << m_separator << "elapsedTime";
		for (auto& conditionPair : m_conditionValues)
		{
			m_outputWriter << m_separator << conditionPair.first.get_message();
		}
		for (auto plugin : m_activePlugins)
		{
			for (auto& field : plugin->get_data())
			{
				m_outputWriter << m_separator << field.first.get_message();
			}
		}
		m_outputWriter << "\n";
		m_outputWriter.flush();

		// Open an audio port for voice recording if enabled in the configuration.
		static SceUserServiceUserId userid;
		if (m_enableAudioRecording && sceUserServiceGetInitialUser(&userid) >= 0) {
			// We got the user id, now try to open an audio port with default input parameters.
			m_audioPort = sceAudioInOpen(userid, SCE_AUDIO_IN_TYPE_VOICE, 0, SCE_AUDIO_IN_GRAIN_DEFAULT, SCE_AUDIO_IN_FREQ_DEFAULT, SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
			if (m_audioPort >= 0) {
				// The audio port was opened, now open the output audio file..
#if defined(RV_PLATFORM_ORBIS) && defined(RV_PACKAGE)
				// [NOTE] All experiment data is written to a USB drive!
				sprintf_s(outputPath, 128, "/usb0/participant_%02d_%s.wav", m_currentParticipant, dateString);
#else
				sprintf_s(outputPath, 128, RV_PATH_LITERAL("Media/Config/participant_%02d_%s.wav"), m_currentParticipant, dateString);
#endif
				m_audioFilePath = outputPath;
				m_audioRecording = AudioFile::AudioFile<short>();
				m_audioRecording.setNumChannels(1);
				m_audioRecording.setSampleRate(16000);
			}
			else
			{
				RV_DEBUG_PRINTF("[ExperimentManager] A new audio port could not be opened!", outputPath);
			}
		}

		// Set the experiment running and write one line just for the initial condition values.
		m_isRunning = true;
		record_experiment_state();
	}

	void ExperimentManager::update(const f32 fDeltaTime, Input::InputController& inputController)
	{
		if (m_isRunning)
		{
			// Update the experiment time.
			m_fTotalTime += fDeltaTime;

			// Update all active plug-ins and write a new line when at least one requested its data to be written or a condition changed.
			bool writeRequest = false;
			for (auto plugin : m_activePlugins)
			{
				writeRequest |= plugin->update(fDeltaTime);
			}
			bool writeRequired = m_conditionChanged || writeRequest;
			if (writeRequired)
			{
				record_experiment_state();
				m_conditionChanged = false;
			}

			// Check if this was the last update:
			if (m_lastHaltEvent != Events::ERevealEventTypes::kDummyEvent)
			{
				if (!writeRequired)
				{
					// Although no write is necessary, write one last line.
					record_experiment_state();
				}
				// Proceed according to the halt event:
				if (m_lastHaltEvent == Events::ERevealEventTypes::kExperiment_End)
				{
					end();
				}
				else if (m_lastHaltEvent == Events::ERevealEventTypes::kExperiment_Abort)
				{
					abort();
				}
			}
		}
	}

	void ExperimentManager::set_experiment_condition(const char* conditionName, const ConditionValue& conditionValue)
	{
		Utilities::Name condition(conditionName);

		// Only predefined conditions in Media/Config/experiment_config.json at "conditions" can be set.
		RV_ASSERT(m_conditionValues.find(condition) != m_conditionValues.end() && "Only values of predefined conditions can be set!");
		// [NOTE] The old Utilities::Name value could in many cases now be deleted from the cache.
		// However, this could be dangerous if the old condition value was by coincidence also used to reference a system-relevant name!
		m_conditionValues[condition] = conditionValue;

		// Set the flag for condition changes, so the next opportunity to write a line is taken.
		m_conditionChanged = true;
	}

	void ExperimentManager::increment_experiment_condition(const char* conditionName, s32 increment)
	{
		// Read out the current value string and try to parse it as an integer.
		auto pCurrentValue = get_experiment_condition_value(conditionName);
		if (pCurrentValue.type == ConditionValue::kInteger)
		{
			pCurrentValue.integer += increment;
			set_experiment_condition(conditionName, pCurrentValue);
		}
		else
		{
			RV_DEBUG_PRINTF("[ExperimentManager] Could not increment the value of condition %s!", conditionName);
		}
	}

	void ExperimentManager::trigger(const char* triggerName)
	{
		Utilities::Name trigger(triggerName);

		// Only predefined triggers in Media/Config/experiment_config.json at "triggers" can be executed.
		RV_ASSERT(m_triggers.find(trigger) != m_triggers.end() && "Only predefined triggers can be executed!");

		// Find out which block should be triggered and execute the appropriate command block.
		auto blockName = m_triggers[trigger].get_command_block(m_currentParticipant);
		auto blockIndex = GamePlay::g_globalGameState.command_block_manager().find_command_block_index(blockName);
		GamePlay::g_globalGameState.command_block_manager().play_block(blockIndex, Events::GEventSystem::instance(), GamePlay::g_globalGameState.callback_manager());
	}

	void ExperimentManager::record_experiment_state()
	{
		RV_ASSERT(m_isRunning);
		m_outputWriter << m_currentParticipant << m_separator << std::fixed << std::setprecision(2) << m_fTotalTime;
		for (auto& conditionPair : m_conditionValues)
		{
			m_outputWriter << m_separator << conditionPair.second;
		}
		for (auto plugin : m_activePlugins)
		{
			for (auto& field : plugin->get_data())
			{
				// Data fields are defined to have an age of zero during the entire frame they were modified in.
				// Ignore any data with an age greater than zero if they are not marked as always up to date.
				bool ignoreOld = !field.second.is_always_up_to_date() && field.second.older_than(0.0f);
				auto& value = ignoreOld || field.second.is_undefined() ? m_undefinedValue : field.second.get();
				m_outputWriter << m_separator << value;
			}
		}
		m_outputWriter << "\n";
		// [NOTE] It can be useful to always flush to avoid losing records after a crash.
		// However, this might not be necessary for the final application!
		m_outputWriter.flush();
	}

	void ExperimentManager::end()
	{
		if (m_isRunning)
		{
			// Reset the experiment manager for the next experiment.
			reset();
		}
	}

	void ExperimentManager::abort()
	{
		if (m_isRunning)
		{
			// Write a line to the file that indicates that the experiment was aborted!
			// This should be enough to make statistics software notice a problem during file import...
			m_outputWriter << "ABORTED!" << "\n";
			// Reset the experiment manager for the next experiment.
			reset();
		}
	}

	void ExperimentManager::reset()
	{
		// Close the output writer if necessary:
		// [NOTE] Do that BEFORE requesting the next file handle, as in package mode, only one file handle can be used at a time.
		// But naturally, neither the kernel nor SCE clearly state that in their logs. IT WOULDN'T BE PS4 DEVELOPMENT IF THEY JUST DID, RIGHT?!
		if (m_outputWriter.is_open())
		{
			m_outputWriter.close();
		}

		// Stop and finalise the audio recording if necessary:
		if (m_enableAudioRecording && m_audioPort >= 0)
		{
			// Join the recording thread if it is active...
			if (m_isAudioRecording)
			{
				m_isAudioRecording = false;
				m_audioThread.join();
			}
			// Close the audio output file:
			m_audioRecording.save(m_audioFilePath);
			// End the SCE audio input and reset the port handle:
			sceAudioInInput(m_audioPort, nullptr);
			sceAudioInClose(m_audioPort);
			m_audioPort = -1;
		}

		// Reset any helper variables, but not the configuration!
		m_isRunning = false;
		m_isAudioRecording = false;
		m_lastHaltEvent = Events::ERevealEventTypes::kDummyEvent;
		m_currentParticipant = kInvalidParticipantNumber;
		m_fTotalTime = 0.0f;
		m_conditionValues.clear();
	}

	bool ExperimentManager::is_running() const
	{
		return m_isRunning;
	}

	ExperimentManager::participant_number_t ExperimentManager::get_current_participant() const
	{
		return m_currentParticipant;
	}

	f32 ExperimentManager::get_elapsed_time() const
	{
		return m_fTotalTime;
	}

	ConditionValue ExperimentManager::get_experiment_condition_value(const char* conditionName) const
	{
		// Allow to probe for existence without crashing the application.
		auto itCondition = m_conditionValues.find(Utilities::Name(conditionName));
		if (itCondition != m_conditionValues.end())
		{
			return itCondition->second;
		}
		else
		{
			// Log a warning and return an invalid value if the condition was not defined.
			RV_DEBUG_PRINTF("[ExperimentManager] Warning: Could not find condition value with name %s.", conditionName);
			return ConditionValue();
		}
	}

	void ExperimentManager::on_event(const Events::Event& evt)
	{
		if (m_isRunning)
		{
			// Make sure we can write to the output file.
			RV_ASSERT(m_outputWriter.good());

			switch (evt.eventType)
			{
			case Events::ERevealEventTypes::kExperiment_End:
			{
				// End the experiment. The experiment state will automatically fade to the main menu.
				// The member variable is used to let plug-ins have one last update.
				m_lastHaltEvent = Events::ERevealEventTypes::kExperiment_End;
				break;
			}
			case Events::ERevealEventTypes::kExperiment_Abort:
			{
				// Abort the experiment. The experiment state will automatically jump to the main menu.
				// The member variable is used to let plug-ins have one last update.
				m_lastHaltEvent = Events::ERevealEventTypes::kExperiment_Abort;
				break;
			}
			case Events::ERevealEventTypes::kExperiment_SetCondition:
			{
				// Set the new value for the specified experiment condition.
				const ExperimentArgs& arg = g_CI_experiment_args_bank[evt.uUserArg];
				set_experiment_condition(Utilities::Name(arg.conditionHash).get_message(), arg.newValue);
				break;
			}
			case Events::ERevealEventTypes::kExperiment_IncrementCondition:
			{
				// Try to increment the specified experiment condition.
				const ExperimentArgs& arg = g_CI_experiment_args_bank[evt.uUserArg];
				increment_experiment_condition(Utilities::Name(arg.conditionHash).get_message(), arg.increment);
				break;
			}
			case Events::ERevealEventTypes::kExperiment_Trigger:
			{
				// Execute the given experiment trigger.
				trigger(Utilities::Name(evt.uUserArg).get_message());
				break;
			}
			case Events::ERevealEventTypes::kExperiment_StartAudioRecording:
			{
				// Start recording the participant's voice if allowed and possible:
				if (!m_isAudioRecording && m_enableAudioRecording && m_audioPort >= 0)
				{
					if (m_audioThread.joinable())
					{
						// The thread member variable was used before, make sure to join it!
						m_audioThread.join();
					}
					m_isAudioRecording = true;
					m_audioThread = std::thread(&ExperimentManager::record_audio, this);
				}
				break;
			}
			case Events::ERevealEventTypes::kExperiment_StopAudioRecording:
			{
				// Stop recording the participant's voice.
				// The audio recording thread will automatically exit when the flag changes.
				m_isAudioRecording = false;
				break;
			}
			};
		}
		else if (evt.eventType == Events::ERevealEventTypes::kExperiment_Start)
		{
			// Start a new experiment!
			Experiment::GExperimentManager::instance().start();
		}
	}

	void ExperimentManager::record_audio()
	{
		while (m_isAudioRecording)
		{
			static const int kSamplesPerBlock = 256;
			static short pcmBuf[2][kSamplesPerBlock] = { { 0 } };
			static int iSide = 0;
			auto samples = sceAudioInInput(m_audioPort, pcmBuf[iSide]);
			iSide ^= 1;
			auto* arrayStart = &pcmBuf[iSide][0];
			auto& rChannel(m_audioRecording.samples[0]);
			rChannel.insert(rChannel.end(), arrayStart, arrayStart + kSamplesPerBlock);
		}
	}

	rv::result_t CI_set_experiment_condition::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		if (rCommandJson.HasMember("condition") && rCommandJson.HasMember("value"))
		{
			rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_SetCondition;
			rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
			// Set the next available index of the experiment argument bank and append it to the bank!
			rCmdOut.m_event.uUserArg = g_CI_experiment_args_bank.size();
			ExperimentArgs newArg = {};
			newArg.conditionHash = Utilities::Name(rCommandJson["condition"].GetString()).get_hash();
			newArg.newValue = ConditionValue(rCommandJson["value"]);
			g_CI_experiment_args_bank.push_back(newArg);
		}
		else
		{
			RV_DEBUG_PRINTF("[COMMAND: set_experiment_condition] Both \"condition\" or \"value\" have to be specified.");
			return Result::kParseError;
		}

		return Result::kNoError;
	}

	const char* CI_set_experiment_condition::description() const
	{
		return "Sets the value of an experiment condition defined in Media/Config/experiment_config.json.";
	}

	const char** CI_set_experiment_condition::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"condition", "The predefined name of the condition to set the value of.",
			"value", "The value to set the condition to."
		};
		numArgsOut = 2;
		return kArgs;
	}

	rv::result_t CI_increment_experiment_condition::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		if (rCommandJson.HasMember("condition"))
		{
			s32 increment = 1;
			if (rCommandJson.HasMember("increment"))
			{
				increment = rCommandJson["increment"].GetInt();
			}
			rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_IncrementCondition;
			rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
			// Set the next available index of the experiment argument bank and append it to the bank!
			rCmdOut.m_event.uUserArg = g_CI_experiment_args_bank.size();
			ExperimentArgs newArg = {};
			newArg.conditionHash = Utilities::Name(rCommandJson["condition"].GetString()).get_hash();
			newArg.increment = increment;
			g_CI_experiment_args_bank.push_back(newArg);
		}
		else
		{
			RV_DEBUG_PRINTF("[COMMAND: increment_experiment_condition] No \"condition\" was specified.");
			return Result::kParseError;
		}

		return Result::kNoError;
	}

	const char* CI_increment_experiment_condition::description() const
	{
		return "Increments the integer value of an experiment condition defined in Media/Config/experiment_config.json.";
	}

	const char** CI_increment_experiment_condition::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"condition", "The predefined name of the condition to increment the value of.",
			"increment", "Optional: The value to add to the previous value of the condition, 1 by default."
		};
		numArgsOut = 2;
		return kArgs;
	}

	rv::result_t CI_experiment_trigger::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		if (rCommandJson.HasMember("trigger"))
		{
			rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_Trigger;
			rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;
			rCmdOut.m_event.uUserArg = Utilities::Name(rCommandJson["trigger"].GetString()).get_hash();
		}
		else
		{
			RV_DEBUG_PRINTF("[COMMAND: experiment_trigger] No \"trigger\" was specified.");
			return Result::kParseError;
		}

		return Result::kNoError;
	}

	const char* CI_experiment_trigger::description() const
	{
		return "Executes a trigger defined in Media/Config/experiment_config.json.";
	}

	const char** CI_experiment_trigger::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"trigger", "The predefined name of the trigger to execute."
		};
		numArgsOut = 1;
		return kArgs;
	}

	rv::result_t CI_start_experiment::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_Start;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_start_experiment::description() const
	{
		return "Starts a new experiment with the participant number that was last set by the setup menu.";
	}

	const char** CI_start_experiment::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

	rv::result_t CI_end_experiment::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_End;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_end_experiment::description() const
	{
		return "Ends the current experiment. The screen fades to black and the experiment menu is shown again.";
	}

	const char** CI_end_experiment::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

	rv::result_t CI_abort_experiment::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_Abort;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_abort_experiment::description() const
	{
		return "Aborts the current experiment. After marking the output file, the experiment menu is shown again.";
	}

	const char** CI_abort_experiment::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

	rv::result_t CI_start_audio_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StartAudioRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_start_audio_recording::description() const
	{
		return "Starts recording to the audio output file if audio recording was enabled in the experiment configuration.";
	}

	const char** CI_start_audio_recording::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

	rv::result_t CI_stop_audio_recording::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StopAudioRecording;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		return Result::kNoError;
	}

	const char* CI_stop_audio_recording::description() const
	{
		return "Stops recording to the audio output file if audio is currently being recorded.";
	}

	const char** CI_stop_audio_recording::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			""
		};
		numArgsOut = 0;
		return kArgs;
	}

	rv::result_t CI_start_controller_check::interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const
	{
		RV_UNUSED(rAllocator);

		rCmdOut.m_event.eventType = Events::ERevealEventTypes::kExperiment_StartControllerCheck;
		rCmdOut.m_event.eventChannel = Events::ERevealEventChannels::kExperimentChannel;

		if (rCommandJson.HasMember("callbackBlock"))
		{
			rCmdOut.m_event.uUserArg = Utilities::Name(rCommandJson["callbackBlock"].GetString()).get_hash();
		}
		else
		{
			rCmdOut.m_event.uUserArg = 0;
		}

		return Result::kNoError;
	}

	const char* CI_start_controller_check::description() const
	{
		return "Starts the playback of a fixed sequence of instructions and input processing that tests controller interaction.";
	}

	const char** CI_start_controller_check::arguments(u32& numArgsOut) const
	{
		static const char* kArgs[] =
		{
			"callbackBlock", "Optional: The name of a command block that should be played when the procedure finished."
		};
		numArgsOut = 1;
		return kArgs;
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
