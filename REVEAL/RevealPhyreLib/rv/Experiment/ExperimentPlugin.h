#pragma once

#include <string>
#include <utility>
#include <unordered_map>
#include <algorithm>

#include "rv/RevealConfig.h"
#include "rv/Events/Events.h"
#include "rv/Events/CommandBlocks.h"
#include "rv/Json/JsonDecl.h"
#include "rv/Json/JsonHelpers.h"
#include "rv/Containers/RingArray.h"

#include "ExperimentPluginDataField.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	class ExperimentPlugin : public Events::EventSystemObserver
	{
	public:

		// Define the type that will map plug-in field names onto the field data.
		using DataField = ExperimentPluginDataField;
		using DataMap = std::unordered_map<Utilities::Name, DataField>;

		virtual ~ExperimentPlugin();

		// Registers special command interpreters for this experiment plug-in.
		// This function may be overridden by subclasses that define their own commands.
		virtual void register_interpreters(Events::CommandBlockManager& rCBManager) const;

		// Configure the plug-in from a JSON object.
		virtual void configure_from_json(const Json::Value& jsonData);

		// Updates this plug-in and indicates if new data is available.
		// The resulting boolean will be stored for later queries.
		bool update(const f32 fDeltaTime);

		// Resets the plug-in's data fields and internal variables.
		// The data fields are set back to their initial values.
		virtual void reset() = 0;

		// Return the unique name of the plug-in used as an identifier.
		virtual Utilities::Name get_name() const = 0;

		// Returns a constant reference to read the plug-in's current data map.
		const DataMap& get_data() const;

	protected:

		// Handles game events that are relevant to this plug-in.
		// Events are queued up and handled by specialisations.
		void on_event(const Events::Event& evt) override;

		// Updates the specific plug-in logic and any plug-in data dependent on it.
		// Changes in the data are automatically detected afterwards by this base class.
		// If none of the available plug-ins modifies their data, no new line will be written.
		// Plug-ins should generally only modify their data if necessary for the statistics!
		virtual void update_internal(const f32 fDeltaTime) = 0;

		// Updates any plug-in data that is dependent on certain events.
		// Plug-in specialisations may not directly register as an observer!
		// Their event handling needs to be part of the update procedure.
		// The reason for this is the way the data field ages are managed.
		virtual void handle_event(const Events::Event& evt) = 0;

		// All subclasses should initialise the base class at construction time.
		// Solves the problem explained here: https://stackoverflow.com/a/8630215
		void initialise();

		// Subclasses shall use this function to add another field (column) to future output.
		// If a field with this header name already exists, its value is replaced with the given default value.
		void add_data_field(const char* headerName, const DataField& initialValue = DataField());

		// Subclasses shall use this function to check if a data field is currently registered.
		bool exists_data_field(const char* headerName) const;

		// Subclasses shall use this function to remove a field (column) from future output.
		// If no field with this header name exists, nothing happens.
		void remove_data_field(const char* headerName);

		// Subclasses shall use this function to directly read or assign datafields.
		inline DataField& data(Utilities::Name headerName)
		{
			return m_data.at(headerName);
		}
		inline DataField& data(const char* headerName)
		{
			// An overload for constant strings is convenient.
			return data(Utilities::Name(headerName));
		}

	private:

		// The current data will be processed by the experiment manager.
		// An empty string symbolises an undefined data value.
		// It can directly written to by all derived classes.
		DataMap m_data;

		// Define a type for the event queue managed by this base class.
		using EventQueue = Containers::RingArray<Events::Event, u16, 1024>;
		// This makes it much easier for plug-ins to update their data in one go.
		// Any events are queued up here and then handled during the internal update.
		// This eliminates the problem of aging data written during an event dispatch.
		// Reason: This class updates the data field age only after event dispatch!
		EventQueue m_eventQueue;

	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
