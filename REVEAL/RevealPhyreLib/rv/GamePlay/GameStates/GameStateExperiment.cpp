#include "GameStateExperiment.h"

#include "rv/RevealConfig.h"
#include "GameStatesReveal.h"
#include "rv/Maths/MathUtilities.h"
#include "rv/Input/rv_input_utilities.h"
#include "rv/DebugDrawing/DebugDraw.h"
#include "rv/GamePlay/TweakableConstants.h"
#include "rv/Rendering/Effects/Tonemap.h"
#include "rv/Rendering/PostEffectManager.h"
#include "rv/Framework/PaperArtifactRenderer.h"
#include "rv/Rendering/Effects/Effects.h"
#include "rv/Events/CommandBlocks.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace GamePlay
{

	DebugDrawing::DebugDrawPalette g_experimentStateDebugPalette;

	static constexpr f32 kFadeDuration = 0.5f;

	GameStateExperiment::GameStateExperiment(GameStateManager& rGameStateManager, GlobalGameState& rGlobalState)
		: GameState(rGameStateManager, rGlobalState)
	{
	}

	void GameStateExperiment::handle_input(f32 deltaTime, Input::InputController& inputController)
	{
		// [NOTE] This procedure seems to be necessary for any game state.
		auto& rGS = global_state();
		Phyre::PFramework::PInputMapper& input = rGS.input_mapper();
		if (input.checkAndClearKey(Phyre::PInputs::PInputBase::InputChannel_Key_F11))
		{
			bool activate = !global_state().is_editor_ui_active();
			global_state().set_editor_ui_active(activate);
			global_state().set_pc_input_active(!activate);
			if (activate)
			{
				rGS.set_cursor_active(true);
			}
			else
			{
				rGS.set_cursor_active(false);
			}
		}

#ifndef RV_PACKAGE_BUILD
		handle_debug_input();
#endif

		if (m_flags.is_set(EndingGame))
		{
			// The experiment has ended and the responsible command block currently fades out.
			static f32 fadeTime = 0.0f;
			fadeTime += deltaTime;
			if (fadeTime >= 3.2f)
			{
				// Jump back to the main menu!
				GamePlay::GGameStateManager::instance().set_next_state(GamePlay::EGameState::kGameMenu);
				fadeTime = 0.0f;
			}
		}
		else if (m_flags.is_set(ControllerCheck))
		{
			update_controller_check(inputController);
		}

		// Update the experiment manager.
		Experiment::GExperimentManager::instance().update(deltaTime, inputController);

		// [NOTE] In the old locomotion experiment, the controllers checked for key presses related to the experiment logic.
		// From now on, they are ONLY listening for and acting upon input that has to do with the locomotion mechanic.
		// ALL OTHER input that has to do with the experiment logic (skip or abort trial, for example) is handled HERE.

		static f32 elapsedSeconds = 0.0f;
		if (m_actionAbort.is_all_pressed(inputController.m_keyRegister))
		{
			if (elapsedSeconds > 3.0f)
			{
				// The experimenter aborted the experiment.
				Experiment::GExperimentManager::instance().abort();
				GamePlay::GGameStateManager::instance().set_next_state(GamePlay::EGameState::kGameMenu);
			}
			elapsedSeconds += deltaTime;
		}
		else
		{
			elapsedSeconds = 0.0f;
		}

#ifndef RV_PACKAGE
		// Overwrite the experiment save file which is used for debugging.
		// Define USE_EXPERIMENT_SAVEGAME in RevealConfig.h to load it automatically.
		static bool justSaved = false;
		if (m_actionDebugSave.is_all_pressed(inputController.m_keyRegister))
		{
			// Prevent saving every frame while all buttons are pressed.
			if (!justSaved)
			{
				rGS.save(RV_PATH_LITERAL("experiment_data.json"));
				justSaved = true;
			}
		}
		else
		{
			justSaved = false;
		}
#endif

		// Update player related things.
		auto& rPlayer = rGS.player();

		// Update GUI.
		UI::UIManager::instance().set_origin(rPlayer.player_world_matrix());

		// Update audio system.
		auto& rAudioManager = rGS.audio_manager();
		rAudioManager.update_3d_audio(deltaTime, rPlayer.get_head_world_pos(), rPlayer.get_cam_forward(), rPlayer.get_cam_up());

		// Update ToggleableNode manager.
		rGS.toggleable_manager()->update(rGS.narrative_graph());

#ifdef ENABLE_MAP
		// Update the player's map:
		auto& rMapScreen = UI::UIManager::instance().get_screen(Utilities::Name("map_screen"));
		auto* pUIElement = rMapScreen.get_with_name(Utilities::Name("target"));
		if (pUIElement)
		{
			v3 vWorldPos(g_globalGameState.controller()->get_player().get_head_world_pos());
			v2 vPos2d(convert_world_position_to_map(vWorldPos));
			RV_DEBUG_PRINTF("%.3f %.3f %.3f -> %.3f %.3f", RV_EXPAND_V3(vWorldPos), RV_EXPAND_V2(vPos2d));

			pUIElement->set_pos(v3(vPos2d.getX().getAsFloat(), vPos2d.getY().getAsFloat(), 9.0f));
			rMapScreen.update(m4::identity(), 0.0f, 0.0f);
			g_globalGameState.paper_artifact_renderer()->set_high_res_dirty();
		}
#endif
	}

	void GameStateExperiment::render()
	{
	}

	void GameStateExperiment::on_draw_debug(const Phyre::PCamera& rCamera, const m4& viewProj)
	{
		g_globalGameState.on_draw_debug(rCamera, viewProj);
	}

	void GameStateExperiment::on_gui()
	{
	}

	void GameStateExperiment::on_enter_state()
	{
		// [NOTE] This assumes that there is no pause menu!
		// Not very clean, so leaving and re-entering this state breaks things at the moment!
		RV_DEBUG_PRINTF("Entering experiment state.\n", 0);

		auto& rGS = global_state();
#ifndef RV_ENABLE_IMGUI
		rGS.set_cursor_active(false);
#endif // RV_ENABLE_IMGUI

		// Register the observer when this is the active state
		Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kGameplayChannel, this);
		Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kExperimentChannel, this);
		Events::GEventSystem::instance().register_observer(Events::ERevealEventChannels::kDebugChannel, this);

		// set the debug palette for game running.
		rGS.set_active_debug_draw_palette(g_experimentStateDebugPalette);

		// Activate museum mode, so participants are not able to use the save system.
		g_globalGameState.enable_museum_mode(true);

		// Activate the controller and disable the pause menu.
		g_globalGameState.controller_manager().set_controller_active(true);
		g_globalGameState.controller_manager().get_controller()->flags().enable(Controller::LocomotionController::ELocomotionFlags::kDisablePauseMenu);

		// Also reset the narrative graph.
		// Do this before loading a potential experiment save file to not reset its changes to the narrative graph!
		g_globalGameState.narrative_graph().reset();

		// The experiment room is always the start node if it is activated.
#ifdef ENABLE_EXPERIMENT_ROOM
		auto startNodeName = Utilities::Name("Node_Experiment");
		g_globalGameState.set_starting_node(startNodeName);
		RV_ASSERT(g_globalGameState.world_graph().find_node_by_id(startNodeName) != Containers::kInvalidNodeIndex && "The level needs to contain the experiment room.");
#else
		// With no experiment room, just use the starting node from the gameplay configuration.
		GamePlay::g_globalGameState.set_starting_node(g_globalGameState.config().startNodeName);
#endif  // ENABLE_EXPERIMENT_ROOM

		// But even with the experiment room activated, a loaded experiment save file still overwrites the start node.
#if (!defined(RV_PLATFORM_ORBIS) || !defined(RV_PACKAGE)) && defined(USE_EXPERIMENT_SAVEGAME)
		// If available, deserialise the world configuration for the experiment as if it was a save game.
		// This can be helpful for debugging gameplay from a specific point in the story on.
		// [NOTE] This procedure was derived from GameStateGameMenu::on_museum_mode.
		if (std::ifstream(RV_PATH_LITERAL("experiment_data.json")).good())
		{
			FileSystem::FileReader m_museumModeJsonData;
			m_museumModeJsonData.load(RV_PATH_LITERAL("experiment_data.json"), g_globalGameState.global_ring_allocator(), 4, 16);
			Json::Document gameStateData;
			gameStateData.Parse(reinterpret_cast<const char*>(m_museumModeJsonData.block().data()));
			// Do not load experiment save files in the experiment room, as this will most certainly break the game flow later on!
			auto loadedStartNodeName = Utilities::Name(gameStateData["currentNode"].GetUint());
			if (loadedStartNodeName != Utilities::Name("Node_Experiment"))
			{
				g_globalGameState.set_starting_node(loadedStartNodeName);
				g_globalGameState.narrative_graph().load_progress(gameStateData);
				g_globalGameState.post_level_load_reset();
				g_globalGameState.apply_loaded_data(gameStateData);
				const char* line = "----------------------------------------";
				rv::debug_printf("%s\n%s\n\n\tWARNING: Loaded experiment save file!\n\n%s\n%s\n", line, line, line, line);
				// Check if the experimenter provided a command block that sets up an "experiment" specifically for loaded games.
				// This has to be provided manually, but gives them an opportunity to still consider the participant number and get output files!
				auto loadSetupBlockName = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("experiment_save_setup"));
				if (loadSetupBlockName != Events::CommandBlockManager::kInvalidCommandBlockIndex)
				{
					g_globalGameState.command_block_manager().play_block(loadSetupBlockName, Events::GEventSystem::instance(), g_globalGameState.callback_manager());
				}
			}
		}
#endif // Non-package build with experiment save game loading enabled.

		// In any case, update the active light volume and turn the fog on or off.
		// This is especially important if the start node was set in the graveyard.
		auto fogEffectId = Rendering::Effects::PostEffectTraits<rv::Rendering::Effects::RVVolumetricFog>::effect_id();
		auto pFogPostEffect = Rendering::GPostEffectManager::instance().get_active_effect(fogEffectId);
		if (pFogPostEffect)
		{
			// This assumes that fog is only displayed on nodes in the graveyard and garden and that the naming scheme is kept consistent...
			static const std::string graveyard("Node_Graveyard_");
			static const std::string garden("Node_Garden_");
			auto name = std::string(GamePlay::g_globalGameState.starting_node().get_message());
			pFogPostEffect->setEnabled(name.compare(0, graveyard.length(), graveyard) == 0 || name.compare(0, garden.length(), garden) == 0);
		}
		auto& rWG(GamePlay::g_globalGameState.world_graph());
		auto pNode = static_cast<GamePlay::LocomotionNode*>(rWG.get_node_value(rWG.find_node_by_id(GamePlay::g_globalGameState.starting_node())));
		GamePlay::g_globalGameState.light_manager().set_active_volume_set(pNode->get_light_volume_set_id(), true);

		// Lastly, reset this state after everything else was set up.
		reset();
	}

	void GameStateExperiment::on_first_enter_state()
	{
		// setup debug palette
		g_experimentStateDebugPalette.reset(v3(0.f, 0.f, 0.f), 0.1f);

		// Create the button layout for actions that relate to the experiment.
		Input::Action::InputMask mappedKeys;
		mappedKeys.enable(Input::EDS4Buttons::R2);
		mappedKeys.enable(Input::EDS4Buttons::Cross);
		m_actionAbort.init(mappedKeys);
		mappedKeys.clear();
		mappedKeys.enable(Input::EDS4Buttons::R2);
		mappedKeys.enable(Input::EDS4Buttons::Triangle);
		m_actionDebugSave.init(mappedKeys);
		mappedKeys.clear();
		mappedKeys.enable(Input::EDS4Buttons::R1);
		mappedKeys.enable(Input::EDS4Buttons::L1);
		m_actionControllerShoulder.init(mappedKeys);
		mappedKeys.clear();
		mappedKeys.enable(Input::EDS4Buttons::Circle);
		mappedKeys.enable(Input::EDS4Buttons::Cross);
		mappedKeys.enable(Input::EDS4Buttons::Triangle);
		mappedKeys.enable(Input::EDS4Buttons::Square);
		m_actionControllerSymbol.init(mappedKeys);
	}

	void GameStateExperiment::on_exit_state()
	{
		RV_DEBUG_PRINTF("Exiting experiment state.\n", 0);
		// Unregister the observer when this is not the active state
		Events::GEventSystem::instance().unregister_observer(Events::ERevealEventChannels::kGameplayChannel, this);
		Events::GEventSystem::instance().unregister_observer(Events::ERevealEventChannels::kExperimentChannel, this);
		Events::GEventSystem::instance().unregister_observer(Events::ERevealEventChannels::kDebugChannel, this);
#ifndef RV_ENABLE_IMGUI
		global_state().set_cursor_active(true);
#endif // RV_ENABLE_IMGUI

		// Deactivate the controller.
		g_globalGameState.controller_manager().set_controller_active(false);
	}

	void GameStateExperiment::on_release()
	{
	}

	Phyre::PResult GameStateExperiment::after_transparent_callback(Phyre::PWorldRendering::PWorldRendererFrame& frame, void* callbackData)
	{
		RV_UNUSED(frame);
		RV_UNUSED(callbackData);

		return Phyre::PE_RESULT_NO_ERROR;
	}

	void GameStateExperiment::reset()
	{
		m_flags.disable(EndingGame);
		m_flags.disable(ControllerCheck);

		// Set the camera
		auto& rGS = global_state();
		rGS.set_camera(&rGS.player().camera());

		// REVISE: reset the debug preset
		// [NOTE] Could the presets possibly be made part of the JSON config file?
		TweakableConstants::set_preset(m_currentPreset);

		// Reset only if the previous state was not the pause menu
		if (state_manager().get_previous_state_id() != EGameState::kPauseMenu)
		{
			auto pController = g_globalGameState.controller_manager().get_controller();
			RV_ASSERT(pController);
			// Reset the player
			rGS.player().reset();
			// Reset the controller to the start node.
			pController->reset();

			// make the player pick the map artifact
#ifdef ENABLE_MAP
			Utilities::Name artifact("trial_map_dae");
			auto nodeIdx = rGS.world_graph().find_node_by_id(artifact);
			rGS.player().pick_artifact(static_cast<GamePlay::ArtifactNode*>(rGS.world_graph().get_node_value(nodeIdx)));
#endif

			// Trigger game reset system Command Block
			static Utilities::Name sPostGameResetBlockId("post_game_reset");
			rGS.command_block_manager().try_play_block(sPostGameResetBlockId, Events::GEventSystem::instance(), rGS.callback_manager());
		}
		RV_PACKAGE_MARKER("END GameStatePlaying::reset()");
	}

	void GameStateExperiment::on_event(const Events::Event& evt)
	{
		if (evt.eventChannel == Events::ERevealEventChannels::kGameplayChannel)
		{
			switch (evt.eventType)
			{
			case Events::ERevealEventTypes::kGamePlay_OnTryOpenLogicNode:
			{
				RV_DEBUG_PRINTF("kGamePlay_OnTryOpenLogicNode %u \n", evt.uUserArg);
				// Open a logic node.
				auto* pArgs = reinterpret_cast<GamePlay::OnTryOpenLogicNodeArgs*>(evt.userPtr);
				RV_ASSERT(pArgs);
				Utilities::Name nodeName{ evt.uUserArg };
				auto logicNodeIdx = g_globalGameState.world_graph().find_node_by_id(nodeName);
				GamePlay::LogicNode* pLogicNode = static_cast<GamePlay::LogicNode*>(g_globalGameState.world_graph().get_node_value(logicNodeIdx));
				RV_ASSERT(pLogicNode);
				pLogicNode->try_open(
					g_globalGameState.narrative_graph(),
					g_globalGameState.command_block_manager(),
					Events::GEventSystem::instance(),
					g_globalGameState.callback_manager(),
					pArgs->onBegin,
					pArgs->onEnd,
					pArgs->onFail,
					pArgs->duration
				);
				break;
			}
			case Events::ERevealEventTypes::kGamePlay_OnTryCloseLogicNode:
			{
				RV_DEBUG_PRINTF("kGamePlay_OnTryCloseLogicNode %u \n", evt.uUserArg);
				// Open a logic node.
				auto* pArgs = reinterpret_cast<GamePlay::OnTryCloseLogicNodeArgs*>(evt.userPtr);
				RV_ASSERT(pArgs);
				Utilities::Name nodeName{ evt.uUserArg };
				auto logicNodeIdx = g_globalGameState.world_graph().find_node_by_id(nodeName);
				GamePlay::LogicNode* pLogicNode = static_cast<GamePlay::LogicNode*>(g_globalGameState.world_graph().get_node_value(logicNodeIdx));
				RV_ASSERT(pLogicNode);
				pLogicNode->try_close(
					g_globalGameState.narrative_graph(),
					g_globalGameState.command_block_manager(),
					Events::GEventSystem::instance(),
					g_globalGameState.callback_manager(),
					pArgs->onBegin,
					pArgs->onEnd,
					pArgs->onFail,
					pArgs->duration
				);
				break;
			}
			}
		}
		else if (evt.eventChannel == Events::ERevealEventChannels::kExperimentChannel)
		{
			switch (evt.eventType)
			{
			case Events::ERevealEventTypes::kExperiment_End:
			{
				// Start fading to black to make the jump back to the main menu more pleasant...
				m_flags.enable(EndingGame);
				auto blockIndex = GamePlay::g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("end_fade"));
				GamePlay::g_globalGameState.command_block_manager().play_block(blockIndex, Events::GEventSystem::instance(), GamePlay::g_globalGameState.callback_manager());
				RV_DEBUG_PRINTF("The experiment has ended.\n", 0);
				break;
			}
			case Events::ERevealEventTypes::kExperiment_Abort:
			{
				// Jump back to the main menu...
				GamePlay::GGameStateManager::instance().set_next_state(GamePlay::EGameState::kGameMenu);
				RV_DEBUG_PRINTF("Aborting the experiment!\n", 0);
				break;
			}
			case Events::ERevealEventTypes::kExperiment_StartControllerCheck:
			{
				// Extract the identifier of the callback block specified in the event.
				m_controllerCheckCallbackBlock = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name(evt.uUserArg));
				// Set the flag active to indicate that the controller check procedure is active.
				m_flags.enable(ControllerCheck);
				RV_DEBUG_PRINTF("Starting the controller check.\n", 0);
				// Freeze the locomotion controller's rotation while the procedure is running.
				g_globalGameState.controller_manager().get_controller()->flags().disable(Controller::LocomotionController::ELocomotionFlags::kRotationActive);
				break;
			}
			}
		}
	}

	void GameStateExperiment::handle_debug_input()
	{
		// [NOTE] Copy this from other states if needed in experiment mode.
	}

	rv::v2 GameStateExperiment::convert_world_position_to_map(const v3& vPosition) const
	{
		// From visual inspection const v2 texPos0(180.0f, 75.0f); // maps from world center v3(0,0,0)
		// From visual inspection const v2 texPos1(445.0, 447.0f); // maps from world wash room.  v3(10.6, 0, 14.4)

		constexpr f32 kTexScale = 512.0f;
		constexpr f32 kHalfTexScale = kTexScale * 0.5f;
		constexpr f32 kUIScalingFactor = 1.5f;
		constexpr f32 kFloorSplit = 3.0f; // meters
		constexpr f32 kWorldToTexScale = (445.0f - 180.0f) / 10.6f * kUIScalingFactor;

		static const v2 kvWorldToTexOffset(180.0f - 50.0f, 75.0f); // + floor position
		static const v2 kvFloor[2] = { v2(450.0f - kHalfTexScale, 1450.0f - kHalfTexScale), v2(1300.f - kHalfTexScale, 1475.f - kHalfTexScale) }; // + floor position

		u32 floorId = vPosition.getY().getAsFloat() < kFloorSplit ? 0 : 1;

		v2 vMapPosition(vPosition.getX().getAsFloat(), vPosition.getZ().getAsFloat());

		vMapPosition *= kWorldToTexScale;
		vMapPosition.setY(-vMapPosition.getY().getAsFloat());
		vMapPosition += kvWorldToTexOffset + kvFloor[floorId];

		return vMapPosition;
	}

	void GameStateExperiment::update_controller_check(const Input::InputController& inputController)
	{
		// [NOTE] This is a hard-coded procedure which checks if the participant understands the controller.
		// This section changes in style to make the code more compact.
		// [NOTE] Skip the shoulder button check, as these are not needed in this specific experiment scenario.
		static const int startStep = 0;
		static int step = startStep;
		static Events::CommandBlockManager::CommandPlayer* commandPlayer = nullptr;
		static u32 block1 = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("welcome_participant"));
		static u32 block2 = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("check_joysticks"));
		static u32 block3 = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("bridge_to_presence_explanation"));
		static u32 block4 = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("explain_presence"));
		static u32 block5 = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("measure_presence_baseline"));
		static u32 waitBlock = g_globalGameState.command_block_manager().find_command_block_index(Utilities::Name("preparation_wait"));
		static bool isWaiting = false;
		// Some defines that avoid too ridiculous duplication of code.
#define CONTROLLER_CHECK_START_STEP(blockVariableName) \
	commandPlayer = g_globalGameState.command_block_manager().play_block(blockVariableName, Events::GEventSystem::instance(), g_globalGameState.callback_manager()); \
	isWaiting = false; step++;
#define CONTROLLER_CHECK_WAIT_FOR(boolVariableName) \
	if (isWaiting || commandPlayer->is_complete()) { \
		if (boolVariableName) { \
			step++; isWaiting = false; boolVariableName = false; \
		} else if (!isWaiting) { \
			isWaiting = true; \
			commandPlayer = g_globalGameState.command_block_manager().play_block(waitBlock, Events::GEventSystem::instance(), g_globalGameState.callback_manager()); \
		} else if (commandPlayer->is_complete()) { \
			step--; \
		} \
	}
#define CONTROLLER_WAIT_FOR_BLOCK \
	if (commandPlayer->is_complete()) { \
		step++; \
	}
		// Just sequentially go through all steps.
		if (step == 0) {
			// Play the welcome message for the participant:
			CONTROLLER_CHECK_START_STEP(block1);
		} else if (step == 1) {
			// Wait for the message to be finished.
			CONTROLLER_WAIT_FOR_BLOCK
		} else if (step == 2) {
			// Play the instructions for the joystick check:
			CONTROLLER_CHECK_START_STEP(block2);
		} else if (step == 3) {
			// Wait for the instructions to be finished and the correct input to be made.
			static bool joysticksTilted = false;
#ifndef ENABLE_REMOTE_VR_USAGE
			joysticksTilted |= inputController.m_rightJoystick.get_pos().getX() > 0.5f && inputController.m_leftJoystick.get_pos().getX() < -0.5f;
#else
			// Accept only one moved joystick if the application is controlled remotely and there is no easy way of simultaneously moving both joysticks...
			joysticksTilted |= inputController.m_rightJoystick.get_pos().getX() > 0.5f || inputController.m_leftJoystick.get_pos().getX() < -0.5f;
#endif
			CONTROLLER_CHECK_WAIT_FOR(joysticksTilted);
		} else if (step == 4) {
			// Play the bridge to the presence explanation:
			CONTROLLER_CHECK_START_STEP(block3);
		} else if (step == 5) {
			// Wait for the bridge to be finished.
			CONTROLLER_WAIT_FOR_BLOCK
		} else if (step == 6) {
			// Play the presence explanation:
			CONTROLLER_CHECK_START_STEP(block4);
		} else if (step == 7) {
			// Wait for the explanation to be finished and one of the symbol buttons to be pressed.
			static bool symbolButtonPressed = false;
			symbolButtonPressed |= m_actionControllerSymbol.is_any_down(inputController.m_keyRegister);
			CONTROLLER_CHECK_WAIT_FOR(symbolButtonPressed);
		} else if (step == 8) {
			// Take a first presence measurement (baseline in neutral environment) after the participant is ready to start.
			CONTROLLER_CHECK_START_STEP(block5);
		} else if (step == 9) {
			// Wait for the first presence measurement to be finished.
			CONTROLLER_WAIT_FOR_BLOCK
		} else if (step == 10) {
			// Reset all variables related to the controller check.
			step = startStep;
			isWaiting = false;
			commandPlayer = nullptr;
			m_flags.disable(ControllerCheck);
			// Execute the "callback" command block if one was specified.
			if (m_controllerCheckCallbackBlock != Events::CommandBlockManager::kInvalidCommandBlockIndex) {
				g_globalGameState.command_block_manager().play_block(m_controllerCheckCallbackBlock, Events::GEventSystem::instance(), g_globalGameState.callback_manager());
			}
			// Activate the locomotion controller's rotation again.
			g_globalGameState.controller_manager().get_controller()->flags().enable(Controller::LocomotionController::ELocomotionFlags::kRotationActive);
		}
	}

} // namespace GamePlay
} // namespace rv

#endif // ENABLE_EXPERIMENT
