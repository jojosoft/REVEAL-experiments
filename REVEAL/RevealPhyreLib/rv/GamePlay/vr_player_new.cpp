#include "vr_player.h"

#include "rv/Utilities/rv_types.h"

#include "GameStates/GameStatesReveal.h"
#include "RevealEvents.h"
#include "TweakableConstants.h"
#include "rv/Input/rv_input_utilities.h"
#include "rv\DebugDrawing\DebugDraw.h"
#include "rv\Maths\MathUtilities.h"
#include "rv/Utilities/colour.h"
#include "rv/Framework/PaperArtifactRenderer.h"
#include "rv/Json/JsonKeys.h"

#include "rv/Audio/AudioManager.h"

namespace rv
{
namespace GamePlay
{
	// Merge this with your implementation of the VR player...
	m4 VRPlayer::get_controller_track_matrix() const
	{
		m4 out = m4::identity();
#ifdef RV_ENABLE_VR
		if (m_tracker)
		{
			for (u32 i = 0; i < m_tracker->getTrackedDeviceCount(); ++i)
			{
				auto& dev = m_tracker->getTrackedDevice(i);
				if (dev.m_type == Phyre::PVr::PVrTrackerDeviceType::PE_VR_TRACKER_DEVICE_DUALSHOCK4)
				{
					// [NOTE] The naming is quite misleading, this actually appears to be the tracking matrix!
					out = dev.m_worldMatrix;
					break;
				}
			}
		}
#endif
		return out;
	}
} // namespace GamePlay
} // namespace rv
