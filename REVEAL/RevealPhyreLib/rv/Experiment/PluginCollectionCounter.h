#pragma once

#include <string>
#include <vector>

#include "rv/RevealConfig.h"
#include "rv/Events/Events.h"
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

	//! This plug-in counts the number of collected inventory items.
	//! Optionally, a command block can be executed for each collection.
	//! If several command blocks are specified, the plug-in cycles through them.
	class PluginCollectionCounter : public ExperimentPlugin
	{
	public:

		PluginCollectionCounter();

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

		std::vector<Utilities::Name> m_commandBlocks;
		bool m_onlyInventory;

		u32 m_currentItems;

	};

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
