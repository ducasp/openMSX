#include "VisibleSurface.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "VideoSystem.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "one_of.hh"
#include <cassert>

namespace openmsx {

VisibleSurface::VisibleSurface(
		Display& display_,
		RTScheduler& rtScheduler,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_,
		VideoSystem& videoSystem_)
	: RTSchedulable(rtScheduler)
	, display(display_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
	, cliComm(cliComm_)
	, videoSystem(videoSystem_)
{
	auto& renderSettings = display.getRenderSettings();

	inputEventGenerator.getGrabInput().attach(*this);
	renderSettings.getPointerHideDelaySetting().attach(*this);
	renderSettings.getFullScreenSetting().attach(*this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);

	updateCursor();
}

VisibleSurface::~VisibleSurface()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	inputEventGenerator.getGrabInput().detach(*this);
	auto& renderSettings = display.getRenderSettings();
	renderSettings.getPointerHideDelaySetting().detach(*this);
	renderSettings.getFullScreenSetting().detach(*this);
}

void VisibleSurface::update(const Setting& /*setting*/)
{
	updateCursor();
}

void VisibleSurface::executeRT()
{
	// timer expired, hide cursor
	videoSystem.showCursor(false);
	inputEventGenerator.updateGrab(grab);
}

int VisibleSurface::signalEvent(const std::shared_ptr<const Event>& event)
{
	assert(event->getType() == one_of(OPENMSX_MOUSE_MOTION_EVENT,
	                                  OPENMSX_MOUSE_BUTTON_UP_EVENT,
	                                  OPENMSX_MOUSE_BUTTON_DOWN_EVENT));
	updateCursor();
	return 0;
}

void VisibleSurface::updateCursor()
{
	cancelRT();
	auto& renderSettings = display.getRenderSettings();
	grab = renderSettings.getFullScreen() ||
	       inputEventGenerator.getGrabInput().getBoolean();
	if (grab) {
		// always hide cursor in fullscreen or grabinput mode, but do it
		// after the derived class is constructed to avoid an SDL bug.
		scheduleRT(0);
		return;
	}
	inputEventGenerator.updateGrab(grab);
	float delay = renderSettings.getPointerHideDelay();
	if (delay == 0.0f) {
		videoSystem.showCursor(false);
	} else {
		videoSystem.showCursor(true);
		if (delay > 0.0f) {
			scheduleRT(int(delay * 1e6f)); // delay in s, schedule in us
		}
	}
}

} // namespace openmsx
