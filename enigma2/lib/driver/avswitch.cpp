#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include <lib/base/cfile.h>
#include <lib/base/init.h>
#include <lib/base/init_num.h>
#include <lib/base/eerror.h>
#include <lib/base/ebase.h>
#include <lib/driver/avswitch.h>
#include <lib/gdi/xineLib.h>
eAVSwitch *eAVSwitch::instance = 0;

eAVSwitch::eAVSwitch()
{
	ASSERT(!instance);
	instance = this;
	m_video_mode = 0;
	m_active = false;
	m_fp_fd = open("/dev/dbox/fp0", O_RDONLY|O_NONBLOCK);
	if (m_fp_fd == -1)
	{
		eDebug("couldnt open /dev/dbox/fp0 to monitor vcr scart slow blanking changed!");
		m_fp_notifier=0;
	}
	else
	{
		m_fp_notifier = eSocketNotifier::create(eApp, m_fp_fd, eSocketNotifier::Read|POLLERR);
		CONNECT(m_fp_notifier->activated, eAVSwitch::fp_event);
	}
}

#ifndef FP_IOCTL_GET_EVENT
#define FP_IOCTL_GET_EVENT 20
#endif

#ifndef FP_IOCTL_GET_VCR
#define FP_IOCTL_GET_VCR 7
#endif

#ifndef FP_EVENT_VCR_SB_CHANGED
#define FP_EVENT_VCR_SB_CHANGED 1
#endif

int eAVSwitch::getVCRSlowBlanking()
{
	return 0;
}

void eAVSwitch::fp_event(int what)
{
	if (what & POLLERR) // driver not ready for fp polling
	{
		eDebug("fp driver not read for polling.. so disable polling");
		m_fp_notifier->stop();
	}
	else
	{
		CFile f("/proc/stb/fp/events", "r");
		if (f)
		{
			int events;
			if (fscanf(f, "%d", &events) != 1)
				eDebug("read /proc/stb/fp/events failed!! (%m)");
			else if (events & FP_EVENT_VCR_SB_CHANGED)
				/* emit */ vcr_sb_notifier(getVCRSlowBlanking());
		}
		else
		{
			int val = FP_EVENT_VCR_SB_CHANGED;  // ask only for this event
			if (ioctl(m_fp_fd, FP_IOCTL_GET_EVENT, &val) < 0)
				eDebug("FP_IOCTL_GET_EVENT failed (%m)");
			else if (val & FP_EVENT_VCR_SB_CHANGED)
				/* emit */ vcr_sb_notifier(getVCRSlowBlanking());
		}
	}
}

eAVSwitch::~eAVSwitch() {}
eAVSwitch *eAVSwitch::getInstance()
{
	return instance;
}

bool eAVSwitch::haveScartSwitch()
{
return false;
}

bool eAVSwitch::isActive()
{
	return m_active;
}
void eAVSwitch::setInput(int val)
{}

void eAVSwitch::setColorFormat(int format)
{}

void eAVSwitch::setAspectRatio(int ratio)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setAspectRatio(ratio);
}

void eAVSwitch::setPolicy43(int mode)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setPolicy43(mode);
}

void eAVSwitch::setPolicy169(int mode)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setPolicy169(mode);
}

void eAVSwitch::setZoom(int zoom43_x, int zoom43_y, int zoom169_x, int zoom169_y)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setZoom(zoom43_x, zoom43_y, zoom169_x, zoom169_y);
}

void eAVSwitch::updateScreen()
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->adjust_policy();
}

// 50/60 Hz
void eAVSwitch::setVideomode(int mode)
{
	//printf("----------------------- eAVSwitch::setVideomode %d\n", mode);
	if (mode == m_video_mode)
		return;
	m_video_mode = mode;
}

void eAVSwitch::setWSS(int val) // 0 = auto, 1 = auto(4:3_off)
{}

void eAVSwitch::setDeinterlace(int global, int sd, int hd)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setDeinterlace(global, sd, hd);
}

void eAVSwitch::setSDfeatures(int sharpness, int noise)
{
	cXineLib *xineLib = cXineLib::getInstance();
	cXineLib::getInstance()->setSDfeatures(sharpness, noise);
}
//FIXME: correct "run/startlevel"
eAutoInitP0<eAVSwitch> init_avswitch(eAutoInitNumbers::rc, "AVSwitch Driver");
