#include "PluginCollectionCounter.h"

#include "rv/GamePlay/SpatialNodes/ArtifactNode.h"

#ifdef ENABLE_EXPERIMENT

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	static constexpr const char* kHeaderCollectionCounterItems = "items";

	namespace JsonFieldName
	{
		// This is an optional value that defines the list of command blocks available for collection events.
		// Whenever such an event occurs, the next block will be executed, cycling through the list endlessly.
		static constexpr const char* kPluginCollectionCounterCommandBlocks = "commandBlocks";
		// This is a mandatory value that defines whether only items which go into the inventory should be counted.
		static constexpr const char* kPluginCollectionCounterOnlyInventory = "onlyInventoryItems";
	};

	PluginCollectionCounter::PluginCollectionCounter()
		: m_currentItems(0)
	{
		// Add all static data fields to the data map.
		add_data_field(kHeaderCollectionCounterItems, DataField("0", true));

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginCollectionCounter::configure_from_json(const Json::Value& jsonData)
	{
		// Clear the list of available command blocks:
		m_commandBlocks.clear();
		if (jsonData.HasMember(JsonFieldName::kPluginCollectionCounterCommandBlocks))
		{
			// [OPTIONAL] An array containing the names of command blocks available for collection events.
			auto names = jsonData[JsonFieldName::kPluginCollectionCounterCommandBlocks].GetArray();
			for (Json::Value::ConstValueIterator it = names.Begin(); it != names.End(); ++it)
			{
				m_commandBlocks.push_back(Utilities::Name(it->GetString()));
			}
		}
		RV_ASSERT(jsonData.HasMember(JsonFieldName::kPluginCollectionCounterOnlyInventory) && "No flag for item counting rules provided!");
		// A boolean indicating whether only items which go into the inventory should be counted.
		m_onlyInventory = jsonData[JsonFieldName::kPluginCollectionCounterOnlyInventory].GetBool();
	}

	void PluginCollectionCounter::reset()
	{
		// Reset all data fields.
		data(kHeaderCollectionCounterItems) = DataField("0", true);
		// Reset the item counter.
		m_currentItems = 0;
	}

	Utilities::Name PluginCollectionCounter::get_name() const
	{
		return Utilities::Name("collectionCounter");
	}

	void PluginCollectionCounter::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kGamePlay_OnPickArtifact:
		{
			// An artifact was picked up, check if it's an inventory item or if all items should be counted.
			auto& rWG = GamePlay::g_globalGameState.world_graph();
			auto* artifact = static_cast<GamePlay::ArtifactNode*>(rWG.get_node_value(rWG.find_node_by_id(evt.uUserArg)));
			if (!m_onlyInventory || artifact->is_inventory_item())
			{
				if (m_commandBlocks.size() > 0)
				{
					// There is at least one command block, execute the one associated with the "previous" count...
					auto blockIndex = GamePlay::g_globalGameState.command_block_manager().find_command_block_index(m_commandBlocks[m_currentItems % m_commandBlocks.size()]);
					GamePlay::g_globalGameState.command_block_manager().play_block(blockIndex, Events::GEventSystem::instance(), GamePlay::g_globalGameState.callback_manager());
				}
				// Increase the collected item count and update the data field:
				m_currentItems++;
				data(kHeaderCollectionCounterItems) = std::to_string(m_currentItems);
			}
			break;
		}
		}
	}

	void PluginCollectionCounter::update_internal(const f32 fDeltaTime)
	{
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
