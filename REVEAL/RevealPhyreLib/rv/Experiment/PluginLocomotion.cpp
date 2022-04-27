#include "PluginLocomotion.h"

#include "rv/GamePlay/SpatialNode.h"

#ifdef ENABLE_EXPERIMENT

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	static constexpr const char* kHeaderLocomotionNode = "locomotionNode";
	static constexpr const char* kHeaderLocomotionDistance = "locomotionDistance";

	PluginLocomotion::PluginLocomotion()
	{
		// Add all static data fields to the data map.
		add_data_field(kHeaderLocomotionNode, DataField(true));
		add_data_field(kHeaderLocomotionDistance);

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginLocomotion::configure_from_json(const Json::Value& jsonData)
	{
	}

	void PluginLocomotion::reset()
	{
		// Reset all data fields.
		data(kHeaderLocomotionNode).reset();
		data(kHeaderLocomotionDistance).reset();
	}

	Utilities::Name PluginLocomotion::get_name() const
	{
		return Utilities::Name("locomotion");
	}

	void PluginLocomotion::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kGamePlay_PerformDirectJump:
		{
			// A direct jump is often just organisational and not regarded as locomotion.
			// The new node will be recorded with an undefined travelled distance.
			// This is technically not correct, but useful for the analysis.
			// (The beeline would probably not be very helpful anyway!)
			data(kHeaderLocomotionNode) = Utilities::Name(evt.uUserArg).get_message();
			break;
		}
		case Events::ERevealEventTypes::kAnalytics_NodeReached:
		{
			// The player moved to one of the adjacent locomotion nodes.
			// Record the name of the new node and the distance that was travelled.
			RV_ASSERT(evt.userPtr);
			const Events::NodeReachedArgs* pArgs(reinterpret_cast<const Events::NodeReachedArgs*>(evt.userPtr));
			data(kHeaderLocomotionNode) = pArgs->nodeName.get_message();
			data(kHeaderLocomotionDistance) = std::to_string(pArgs->distance);
			break;
		}
		case Events::ERevealEventTypes::kAnalytics_Teleport:
		{
			// The player moved forward using the pointer controller.
			// Record the distance that was travelled.
			// TODO: It would be good if the free locomotion controllers were still mapping onto the node-system.
			// In case this is realised, the plug-in should also write the current locomotion node at this point!
			// The only challenge then would be to sensibly combine this event with the node reached event.
			RV_ASSERT(evt.userPtr);
			const Events::TeleportArgs* pArgs(reinterpret_cast<const Events::TeleportArgs*>(evt.userPtr));
			data(kHeaderLocomotionDistance) = std::to_string(pArgs->distance);
			break;
		}
		// These events recorded the free controller in a set amount of time:
		// Events::ERevealEventTypes::kAnalytics_PositionUpdate and Events::ERevealEventTypes::kAnalytics_RotationUpdate.
		// TODO: It might be a good idea to remove these events and move all related code from the free controller to update_internal!
		}
	}

	void PluginLocomotion::update_internal(const f32 fDeltaTime)
	{
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
