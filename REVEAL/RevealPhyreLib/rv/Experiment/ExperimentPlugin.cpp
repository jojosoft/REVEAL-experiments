#include "ExperimentPlugin.h"

#include "ExperimentManager.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace Experiment
{

	ExperimentPlugin::~ExperimentPlugin()
	{
	}

	void ExperimentPlugin::register_interpreters(Events::CommandBlockManager& rCBManager) const
	{
	}

	void ExperimentPlugin::configure_from_json(const Json::Value& jsonData)
	{
	}

	bool ExperimentPlugin::update(const f32 fDeltaTime)
	{
		// Update the age of all data fields.
		for (auto& dataPair : m_data)
		{
			dataPair.second.update_age(fDeltaTime);
		}

		// Let the plug-in handle all queued events for this frame.
		while (m_eventQueue.size() > 0)
		{
			handle_event(m_eventQueue.front());
			m_eventQueue.pop_front();
		}

		// Let the plug-in logic update itself and the data.
		update_internal(fDeltaTime);

		// Find out if at least one data field now contains new data.
		// Fields that are assigned the undefined value are ignored.
		bool writeRequired = false;
		for (auto& dataPair : m_data)
		{
			if (!dataPair.second.is_undefined_or_old())
			{
				writeRequired = true;
				// Break the loop, as we have found at least one modified data field.
				break;
			}
		}
		return writeRequired;
	}

	const ExperimentPlugin::DataMap& ExperimentPlugin::get_data() const
	{
		// This should be the only output channel for plug-ins.
		// The experiment manager will read through the constant reference.
		return m_data;
	}

	void ExperimentPlugin::on_event(const Events::Event& evt)
	{
		// Queue up any events during an event dispatch.
		// They will be handled by the plug-in during update.
		// Ignore events while there is no experiment running!
		if (GExperimentManager::instance().is_running())
		{
			m_eventQueue.push_back(evt);
		}
	}

	void ExperimentPlugin::initialise()
	{
		// Automatically register this plugin with the experiment manager.
		ExperimentManager::register_plugin(get_name(), this);
	}

	void ExperimentPlugin::add_data_field(const char* headerName, const DataField& initialValue)
	{
		Utilities::Name headerNameHash(headerName);
		if (!exists_data_field(headerName))
		{
			m_data.insert(std::make_pair(headerNameHash, initialValue));
		}
		else
		{
			data(headerNameHash) = initialValue;
		}
	}

	bool ExperimentPlugin::exists_data_field(const char* headerName) const
	{
		Utilities::Name headerNameHash(headerName);
		return m_data.find(headerNameHash) != m_data.end();
	}

	void ExperimentPlugin::remove_data_field(const char* headerName)
	{
		if (exists_data_field(headerName))
		{
			m_data.erase(Utilities::Name(headerName));
		}
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
