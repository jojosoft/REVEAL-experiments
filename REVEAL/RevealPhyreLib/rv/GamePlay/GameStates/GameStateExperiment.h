#pragma once

#include "GameState.h"
#include "rv/RevealConfig.h"
#include "rv/Utilities/rv_types.h"
#include "rv/Experiment/ExperimentManager.h"

#ifdef ENABLE_EXPERIMENT

namespace rv
{
namespace GamePlay
{

	class GameStateExperiment : public GameState, public Events::EventSystemObserver
	{

	public:

		GameStateExperiment(GameStateManager& rGameStateManager, GlobalGameState& rGlobalState);

		virtual void handle_input(f32 deltaTime, Input::InputController& inputController) override;

		virtual void render() override;

		virtual void on_draw_debug(const Phyre::PCamera& rCamera, const m4& viewProj) override;

		virtual void on_gui() override;

		virtual void on_enter_state() override;

		virtual void on_first_enter_state() override;

		virtual void on_exit_state() override;

		virtual void on_release() override;

		virtual Phyre::PResult after_transparent_callback(Phyre::PWorldRendering::PWorldRendererFrame& frame, void* callbackData) override;

		virtual void reset();

	protected:

		virtual void on_event(const Events::Event& evt) override;

	private:

		void handle_debug_input();

		rv::v2 convert_world_position_to_map(const v3& vPosition) const;

		// This helper function defines the behaviour of the controller check.
		void update_controller_check(const Input::InputController& inputController);

	private:

		enum EPlayingGSFlags : u32
		{
			Initialised = 1 << 0,
			GraphInitialised = 1 << 1,
			ConfigLoaded = 1 << 2,
			EndingGame = 1 << 3,
			ControllerCheck = 1 << 4
		};

		Maths::AnimHelper<f32> m_animator;
		Events::FadeScreenParams m_fadeParams;
		u32 m_currentPreset = 0;
		u32 m_controllerCheckCallbackBlock = Events::CommandBlockManager::kInvalidCommandBlockIndex;

		Input::Action m_actionAbort;
		Input::Action m_actionDebugSave;
		Input::Action m_actionControllerShoulder;
		Input::Action m_actionControllerSymbol;

	};

} // namespace GamePlay
} // namespace rv

#endif // ENABLE_EXPERIMENT
