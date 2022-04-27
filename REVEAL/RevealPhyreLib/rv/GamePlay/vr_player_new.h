#pragma once

#include <Phyre.h>
#include <Framework/PhyreFramework.h>
#include <VR/PhyreVrTracker.h>

#include <Scene\PhyreNode.h>
#include <Scene\PhyreNode.inl>

#include "rv\Containers\Graph.h"
#include "rv\Maths\Vector3.h"
#include "rv/Utilities/rv_phyre_utils.h"
#include "rv/Containers/Mask.h"
#include "rv/Containers/LinearArray.h"
#include "rv/Input/InputAction.h"
#include "rv/Input/Joystick.h"
#include "rv/Maths/Animation.h"
#include "rv/GamePlay/SpatialNode.h"
#include "rv/UI/PlayerHUD.h"
#include "rv/Input/Mouse.h"
#include "rv/RevealConfig.h"
#include "rv/GamePlay/SpatialNodes/ArtifactNode.h"
#include "SpatialNodes/SpatialNodePhyre.h"

namespace rv
{
namespace GamePlay
{
	// Merge this with your implementation of the VR player...
	m4 get_camera_track_matrix() { return m_cameraTrackMatrix; }
	m4 get_controller_track_matrix() const;

} // namespace GamePlay
} // namespace rv
