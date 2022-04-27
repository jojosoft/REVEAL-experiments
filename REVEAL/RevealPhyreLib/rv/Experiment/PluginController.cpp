#include "PluginController.h"

#ifdef ENABLE_EXPERIMENT

#undef GetObject
#pragma warning(disable:4996)

namespace rv
{
namespace Experiment
{

	static constexpr const char* kHeaderController = "controller";
	static constexpr const char* kHeaderControllerMovement = "controllerMovement";

	namespace JsonFieldName
	{
		// This is an optional value that will record the spatial transition flag.
		static constexpr const char* kPluginControllerMovement = "recordMovementFlag";
	};

	PluginController::PluginController()
	{
		// Add all static data fields to the data map.
		add_data_field(kHeaderController, DataField(true));

		// Initialise the base class to register this plug-in!
		initialise();
	}

	void PluginController::configure_from_json(const Json::Value& jsonData)
	{
		if (jsonData.HasMember(JsonFieldName::kPluginControllerMovement))
		{
			// [OPTIONAL] A boolean indicating whether the spatial transition flag should be recorded.
			bool recordMovement = jsonData[JsonFieldName::kPluginControllerMovement].GetBool();
			// Update the data map by including or excluding this data field:
			if (recordMovement)
			{
				add_data_field(kHeaderControllerMovement, DataField(true));
			}
			else
			{
				remove_data_field(kHeaderControllerMovement);
			}
		}
	}

	void PluginController::reset()
	{
		// Reset all data fields.
		data(kHeaderController).reset();
		if (exists_data_field(kHeaderControllerMovement))
		{
			data(kHeaderControllerMovement).reset();
		}
	}

	Utilities::Name PluginController::get_name() const
	{
		return Utilities::Name("controller");
	}

	void PluginController::handle_event(const Events::Event& evt)
	{
		switch (evt.eventType)
		{
		case Events::ERevealEventTypes::kGamePlay_SwitchController:
		{
			// The controller was switched, register the new one for the next line!
			// This will result in one new line in the output file for each controller switch.

			// [NOTE] Determine the new controller's name. (Specific to your implementation...)
			data(kHeaderController) = "ControllerName";

			auto& movementFlagData = data(kHeaderControllerMovement);
			if (movementFlagData.get_age() > 0.0f)
			{
				// Reset the movement flag if it was not written during this event dispatch.
				movementFlagData.reset();
			}
			break;
		}
		case Events::ERevealEventTypes::kGamePlay_SetControllerMovement:
		{
			if (exists_data_field(kHeaderControllerMovement))
			{
				data(kHeaderControllerMovement) = static_cast<bool>(evt.uUserArg) ? "TRUE" : "FALSE";
			}
			break;
		}
		}
	}

	void PluginController::update_internal(const f32 fDeltaTime)
	{
	}

} // namespace Experiment
} // namespace rv

#endif // ENABLE_EXPERIMENT
