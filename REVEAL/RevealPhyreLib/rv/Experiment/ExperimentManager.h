#pragma once

#include <ios>
#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <utility>
#include <unordered_map>
#include <vector>
#include <thread>
#include <AudioFile/AudioFile.h>

#include "rv/RevealConfig.h"
#include "rv/Events/Events.h"
#include "rv/Containers/Graph.h"
#include "rv/Input/InputController.h"
#include "rv/GamePlay/SpatialNode.h"
#include "rv/GamePlay/RevealEvents.h"

#include "ExperimentPlugin.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	// This defines how condition values are represented.
	// Only 32 bit integers and strings can be used.
	struct ConditionValue
	{
		enum Type
		{
			kInteger,
			kString,
			kInvalid
		} type;
		union
		{
			s32 integer;
			Utilities::Name stringHash;
		};

		// The default constructor provides an invalid value.
		ConditionValue()
			: type(kInvalid), integer(0)
		{
		}

		// Construct a condition value from a JSON value.
		// Only 32 bit integers and strings can be used.
		ConditionValue(const Json::Value& jsonValue)
		{
			switch (jsonValue.GetType())
			{
			case Json::Type::kNumberType:
				integer = jsonValue.GetInt();
				type = kInteger;
				break;
			case Json::Type::kStringType:
				stringHash = Utilities::Name(jsonValue.GetString());
				type = kString;
				break;
			default:
				RV_DEBUG_PRINTF("[ConditionValue] Warning: Could not create condition value from the given JSON value.");
				integer = 0;
				type = kInvalid;
				break;
			}
		}
	};

	// This defines what information is needed for an experiment trigger.
	// Alongside the list of possible command blocks, a rotation value greater zero is required.
	// The rotation value defines after how many participants the command blocks are rotated.
	struct Trigger
	{
		std::vector<Utilities::Name> commands;

		// The default constructor provides an empty trigger.
		Trigger()
			: participantRotateInterval(1)
		{
		}

		Trigger(u32 prI)
			: participantRotateInterval(prI > 0 ? prI : 1)
		{
		}

		Utilities::Name get_command_block(u32 participant) const
		{
			return commands.size() > 0 ? commands[(participant / participantRotateInterval) % commands.size()] : Utilities::Name::kInvalidHash;
		}

	private:

		u32 participantRotateInterval;
	};

	class ExperimentManager : public Events::EventSystemObserver
	{
	public:

		// A participant number is an unsigned integer with 32 bits.
		// Its maximum value is reserved for representing an invalid participant number.
		using participant_number_t = u32;
		enum : participant_number_t
		{
			kMaximumParticipantNumber = 99,
			kInvalidParticipantNumber = 0xFFFFffff
		};

		ExperimentManager();

		// Registers special command interpreters for experiment commands.
		// Important: These will only work if an experiment is currently running!
		static void register_interpreters(Events::CommandBlockManager& rCBManager);

		// Connects the experiment manager to the system and resets helper variables.
		// Be sure to only call this once to avoid several registrations with the event system.
		// The configuration of the experiment manager will not be reset.
		void init();

		// Configure the experiment manager from a JSON file.
		void configure_from_json_file(const char* pstrFilePath, Memory::MemAllocator& rAllocator);

		// Configure the experiment manager from a JSON object.
		void configure_from_json(const Json::Value& jsonData);

		// Sets the participant number recorded data will be associated with.
		// This cannot be done while the experiment is running!
		void set_participant(const participant_number_t number);

		// Adds a new condition to the configuration and optionally sets it value.
		// This is not possible while an experiment is running!
		void add_experiment_condition(const char* conditionName, const ConditionValue& defaultValue = ConditionValue());

		// Removes the given condition from the configuration.
		// This is not possible while an experiment is running!
		void remove_experiment_condition(const char* conditionName);

		// Adds a new experiment trigger to the configuration.
		void add_experiment_trigger(const char* triggerName, const Trigger& trigger);

		// Removes the given experiment trigger from the configuration.
		void remove_experiment_trigger(const char* triggerName);

		// This static function should be called once for each plug-in that should be registered.
		// The experimenter can then activate these plug-ins by providing their configuration.
		// This can be done in Media/Config/experiment_config.json at the "plugins" array.
		// Note that a plug-in with the same name as an already registered one will be ignored!
		static void register_plugin(Utilities::Name pluginName, ExperimentPlugin* pPlugin);

		// Sets the specified plug-in active and keeps it in the loop from now on.
		ExperimentPlugin* enable_plugin(Utilities::Name pluginName);

		// Sets the specified plug-in inactive.
		ExperimentPlugin* disable_plugin(Utilities::Name pluginName);

		// Starts the experiment with the current participant number.
		// This involves opening the output file and configuring the world.
		// The filename will contain the current time and the participant number.
		void start();

		// This has to be called every frame to update timings.
		// TODO: A good place for an experiment plugin system that can record special data when triggered?
		void update(const f32 fDeltaTime, Input::InputController& inputController);

		// Sets the value of the given condition which has to have been registered before.
		// This can be done in Media/Config/experiment_config.json at the "conditions" array.
		// Objects with "name" and (optional) "value" fields define names and default values.
		// Make sure to not call this function with too many different values and names:
		// It internally uses an unordered map of registered Utilities::Name pairs!
		// [NOTE] The Name class globally stores all occurances without reference counters.
		void set_experiment_condition(const char* conditionName, const ConditionValue& conditionValue);

		// Increments the value of the given condition which has to have been registered before.
		// For more information, see the comments on ExperimentManager::set_experiment_condition.
		// The value has to represent a signed integer (up to 32 bits) in order to be incremented.
		void increment_experiment_condition(const char* conditionName, s32 increment = 1);

		// Executes the given trigger which in turn will execute one of its command blocks.
		// Which one this is depends in the number of the current participant.
		void trigger(const char* triggerName);

		// Appends another line according to the current experiment state to the output file.
		void record_experiment_state();

		// Ends the experiment, closes the output file and resets the experiment manager.
		// The configuration of the experiment manager will not be reset.
		void end();

		// Resets the experiment and marks the output file as incomplete in the last line.
		// The configuration of the experiment manager will not be reset.
		void abort();

		// Deletes any recorded data and resets parameters.
		// The configuration is not affected by this!
		void reset();

		// Returns true if the experiment is currently running.
		bool is_running() const;

		// Returns the participant number that recorded data will be associated with.
		participant_number_t get_current_participant() const;

		// Returns the time the experiment has been running in seconds.
		f32 get_elapsed_time() const;

		// Returns the current value of the given condition which has to have been registered before.
		// For more information, see the comments on ExperimentManager::set_experiment_condition.
		// If the requested condition has not been registered, an invalid value is returned.
		ConditionValue get_experiment_condition_value(const char* conditionName) const;

	protected:

		// Handles game events that are relevant to the experiment manager.
		virtual void on_event(const Events::Event& evt) override;

		// This function records audio and will be run in a separate thread.
		// Start and stop commands just resume and pause the recording.
		void record_audio();

	private:

		// This is used for static registration of plug-ins.
		using PluginRegister = std::unordered_map<Utilities::Name, ExperimentPlugin*>;

		bool m_isRunning = false;
		bool m_isAudioRecording = false;
		Events::ERevealEventTypes m_lastHaltEvent = Events::ERevealEventTypes::kDummyEvent;
		participant_number_t m_currentParticipant = kInvalidParticipantNumber;
		f32 m_fTotalTime = 0.0f;

		std::unordered_map<Utilities::Name, ConditionValue> m_conditionDefaults;
		std::unordered_map<Utilities::Name, ConditionValue> m_conditionValues;
		std::unordered_map<Utilities::Name, Trigger> m_triggers;
		static PluginRegister m_sAvailablePlugins;
		std::vector<ExperimentPlugin*> m_activePlugins;
		bool m_conditionChanged = false;

		std::ofstream m_outputWriter;
		const char* m_separator = "\t";
		std::string m_undefinedValue;
		bool m_enableAudioRecording;
		s32 m_audioPort;
		AudioFile::AudioFile<short> m_audioRecording;
		std::string m_audioFilePath;
		std::thread m_audioThread;
	};

	// Singleton experiment manager.
	using GExperimentManager = Utilities::SingletonHolder<ExperimentManager>;

	//! Experiment command interpreter for the "set_experiment_condition" command. @ref CommandList "Commands."
	//! This command sets the value of the experiment condition specified at "condition" to the value specified at "value".
	//! The condition has to have been registered in Media/Config/experiment_config.json at "conditions" with its exact name.
	//! Conditions define the base columns that contain the current condition values for each line written to the output file.
	//! @ingroup CommandBlockSystem
	class CI_set_experiment_condition : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "increment_experiment_condition" command. @ref CommandList "Commands."
	//! This command increments the integer value of the experiment condition specified at "condition" by the integer value specified at "increment".
	//! The condition has to have been registered in Media/Config/experiment_config.json at "conditions" with its exact name and needs to be an integer (up to 32 bits).
	//! Conditions define the base columns that contain the current condition values for each line written to the output file.
	//! @ingroup CommandBlockSystem
	class CI_increment_experiment_condition : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	// This struct is needed to store the parameters for all possible experiment commands in a unified way.
	struct ExperimentArgs
	{
		// For set_experiment_condition and increment_experiment_condition:
		Utilities::hash_t conditionHash;
		// For set_experiment_condition:
		ConditionValue newValue;
		// For increment_experiment_condition:
		s32 increment;
	};

	//! Experiment command interpreter for the "experiment_trigger" command. @ref CommandList "Commands."
	//! This command executes the given trigger, which will then play an appropriate command block depending on the participant number.
	//! The trigger has to have been registered in Media/Config/experiment_config.json at "triggers" with its exact name.
	//! @ingroup CommandBlockSystem
	class CI_experiment_trigger : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "start_experiment" command. @ref CommandList "Commands."
	//! This command starts a new experiment with the participant number that was last set by the setup menu.
	//! @ingroup CommandBlockSystem
	class CI_start_experiment : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "end_experiment" command. @ref CommandList "Commands."
	//! This command ends the current experiment. The screen fades to black and the experiment menu is shown again.
	//! @ingroup CommandBlockSystem
	class CI_end_experiment : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "abort_experiment" command. @ref CommandList "Commands."
	//! This command aborts the current experiment. After marking the output file, the experiment menu is shown again.
	//! @ingroup CommandBlockSystem
	class CI_abort_experiment : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "start_audio_recording" command. @ref CommandList "Commands."
	//! This command starts recording to the audio output file if audio recording was enabled in the experiment configuration.
	//! For each experiment, only one audio output file will be written. Starting and stopping merely resumes or pauses recording.
	//! @ingroup CommandBlockSystem
	class CI_start_audio_recording : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment command interpreter for the "stop_audio_recording" command. @ref CommandList "Commands."
	//! This command stops recording to the audio output file if audio is currently being recorded.
	//! For each experiment, only one audio output file will be written. Starting and stopping merely resumes or pauses recording.
	//! @ingroup CommandBlockSystem
	class CI_stop_audio_recording : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

	//! Experiment state command interpreter for the "start_controller_check" command. @ref CommandList "Commands."
	//! The ExperimentState will react on the event produced by this command. The interpreter is defined here for convenience.
	//! This command starts the playback of a fixed sequence of instructions and input processing that tests controller interaction.
	//! Participants get the opportunity to understand the controller better during this procedure.
	//! Afterwards, the callback block at "callbackBlock" is played if it was provided.
	//! @ingroup CommandBlockSystem
	class CI_start_controller_check : public Events::CommandInterpreter
	{
	public:
		result_t interpret_json(const Json::Value& rCommandJson, Events::Command& rCmdOut, Memory::MemAllocator& rAllocator) const override;
		virtual const char* description() const override;
		virtual const char** arguments(u32& numArgsOut) const override;
	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
