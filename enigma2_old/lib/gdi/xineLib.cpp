#include <fstream>
#include <lib/gdi/xineLib.h>
#include <lib/base/eenv.h>

cXineLib   *cXineLib::instance;

DEFINE_REF(cXineLib);

cXineLib::cXineLib(x11_visual_t *vis) : m_pump(eApp, 1) {
	char        configfile[150];
	char        *vo_driver = "auto";
	char        *ao_driver = "alsa";

	instance = this;
	osd = NULL;
	stream = NULL;
	end_of_stream = false;
	videoPlayed = false;

	printf("XINE-LIB version: %s\n", xine_get_version_string() );

	xine = xine_new();
	strcpy(configfile, eEnv::resolve("${datadir}/enigma2/xine.conf").c_str());
	printf("configfile  %s\n", configfile);
	xine_config_load(xine, configfile);
	xine_init(xine);
	xine_engine_set_param(xine, XINE_ENGINE_PARAM_VERBOSITY, XINE_VERBOSITY_LOG);
  
  cfg_entry_t *entry;
	config_values_t *cfg;
// read Video Driver from config
	cfg = this->xine->config;
	entry = cfg->lookup_entry(cfg, "video.driver");
	vo_driver = strdup(entry->unknown_value);
// read Audio Driver from config
	entry = cfg->lookup_entry(cfg, "audio.driver");
	ao_driver = strdup(entry->unknown_value);
	printf("use vo_driver: %s \n", vo_driver);
	printf("use ao_driver: %s \n", ao_driver);

	
	if((vo_port = xine_open_video_driver(xine, vo_driver , XINE_VISUAL_TYPE_X11, (void *) vis)) == NULL)
	{
		printf("I'm unable to initialize '%s' video driver. Giving up.\n", vo_driver);
		return;
	}

	ao_port     = xine_open_audio_driver(xine , ao_driver, NULL);
	stream      = xine_stream_new(xine, ao_port, vo_port);

	if ( (!xine_open(stream, eEnv::resolve("${sysconfdir}/tuxbox/logo.mvi").c_str()))
			|| (!xine_play(stream, 0, 0)) ) {
		return;
	}

	xine_queue = xine_event_new_queue (stream);
	xine_event_create_listener_thread(xine_queue, xine_event_handler, this);

	CONNECT(m_pump.recv_msg, cXineLib::pumpEvent);

	m_width     = 0;
	m_height    = 0;
	m_framerate = 0;
	m_aspect    = -1;
	m_windowAspectRatio = 0;
	m_policy43 = 0;
	m_policy169 = 0;

	m_sharpness = 0;
	m_noise = 0;
}

cXineLib::~cXineLib() {
	instance = 0;

	if (stream)
	{
		xine_stop(stream);
		xine_close(stream);

		if (xine_queue)
		{
			xine_event_dispose_queue(xine_queue);
			xine_queue = 0;
		}

		_x_demux_flush_engine(stream);

		xine_dispose(stream);
		stream = NULL;
	}

	if (ao_port)
		xine_close_audio_driver(xine, ao_port);
	if (vo_port)
		xine_close_video_driver(xine, vo_port);
}

void cXineLib::setVolume(int value) {
//	xine_set_param (stream, XINE_PARAM_AUDIO_VOLUME, value);
	xine_set_param (stream, XINE_PARAM_AUDIO_AMP_LEVEL , value);
}

void cXineLib::setVolumeMute(int value) {
//	xine_set_param (stream, XINE_PARAM_AUDIO_MUTE, value==0?0:1);
	xine_set_param(stream, XINE_PARAM_AUDIO_AMP_MUTE, value==0?0:1);
}

void cXineLib::showOsd() {
	xine_osd_show_scaled(osd, 0);
	//stream->osd_renderer->draw_bitmap(osd, (uint8_t*)m_surface.data, 0, 0, 720, 576, temp_bitmap_mapping);
}

void cXineLib::newOsd(int width, int height, uint32_t *argb_buffer) {
	osdWidth  = width;
	osdHeight = height;

	if (osd)
		xine_osd_free(osd);

	osd = xine_osd_new(stream, 0, 0, osdWidth, osdHeight);
	xine_osd_set_extent(osd, osdWidth, osdHeight);
	xine_osd_set_argb_buffer(osd, argb_buffer, 0, 0, osdWidth, osdHeight);
}

void cXineLib::playVideo(void) {
	xine_stop(stream);
	end_of_stream = false;
	videoPlayed = false;

	printf("XINE try START !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	if ( !xine_open(stream, "enigma:/") ) {
		printf("Unable to open stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}

//	setStreamType(1);
//	setStreamType(0);
xine_pids_data_t data;
xine_event_t event;
  event.type = XINE_EVENT_PIDS_CHANGE;
  data.vpid = videoData.pid;
  data.apid = audioData.pid;
  event.data = &data;
  event.data_length = sizeof (xine_pids_data_t);

  printf ("input_dvb: sending event\n");

  xine_event_send (stream, &event);
setStreamType(1);
setStreamType(0);

        //_x_demux_control_start(stream);
        //_x_demux_seek(stream, 0, 0, 0);

	if( !xine_play(stream, 0, 0) ) {
		printf("Unable to play stream !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	printf("XINE STARTED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

	videoPlayed = true;
}

void cXineLib::stopVideo(void) {
	xine_stop(stream);
	xine_close (stream);
	end_of_stream = false;
	videoPlayed = false;
}

void cXineLib::setStreamType(int video) {
	xine_event_t event;

	if (video==1) {
		event.type = XINE_EVENT_SET_VIDEO_STREAMTYPE;
		event.data = &videoData;
	} else {
		event.type = XINE_EVENT_SET_AUDIO_STREAMTYPE;
		event.data = &audioData;
	}

	event.data_length = sizeof (xine_streamtype_data_t);

	xine_event_send (stream, &event);
}

void cXineLib::setVideoType(int pid, int type) {
	videoData.pid = pid;
	videoData.streamtype = type;
}

//////////////////////7
void cXineLib::FilmVideo(char *mrl) {
ASSERT(stream);
	
	if (!xine_open(stream, mrl))
	{
		eWarning("xine_open failed!");
		return ;
	}
	
	if (!xine_play(stream, 0, 0))
	{
		eWarning("xine_play failed!");
		return ;
	}
videoPlayed = true;
}

int
cXineLib::VideoPause()
{
xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
return 1;
}


int
cXineLib::VideoResume()
{
	int ret;
	/* Resume the playback. */
	ret = xine_get_param(stream, XINE_PARAM_SPEED);
	if( ret != XINE_SPEED_NORMAL ){
		xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
	}
	return 1;
}

int
cXineLib::VideoGeriT(pts_t Sar)
{// 10 saniye Geri Sarma 
pts_t geriSar;
xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
VideoPosisyon();
geriSar=Vpos+Sar;
printf("%d---Vpos=%d---Sar=%d",geriSar,Vpos,Sar);
if (geriSar<0) geriSar=0;
xine_play(stream, 0, geriSar);
xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
return 1;
}

int
cXineLib::VideoIleriF()
{
	int ret;
	/* Slow the playback. */
	ret = xine_get_param(stream, XINE_PARAM_SPEED);
	if( ret != XINE_EVENT_INPUT_RIGHT){
		xine_set_param(stream, XINE_PARAM_SPEED, XINE_EVENT_INPUT_RIGHT);
	}
	return 1;
}

int
cXineLib::VideoPosisyon()
{
xine_get_pos_length (stream, &VposStream, &Vpos, &Vlength);
return 1;
}
/*
XINE_SPEED_SLOW_4
XINE_SPEED_SLOW_2
XINE_SPEED_NORMAL
XINE_SPEED_FAST_2
XINE_SPEED_FAST_4
XINE_FINE_SPEED_NORMAL
*/
/////////////////////

void cXineLib::SeekTo(long long value) {
	xine_play(stream, value, 0);
}

void cXineLib::setAudioType(int pid, int type) {
	audioData.pid = pid;
	audioData.streamtype = type;
}

void cXineLib::setPrebuffer(int prebuffer) {
	xine_set_param(stream, XINE_PARAM_METRONOM_PREBUFFER, prebuffer);
}

void cXineLib::xine_event_handler(void *user_data, const xine_event_t *event)
{
	cXineLib *xineLib = (cXineLib*)user_data;
	//if (event->type!=15)
	//	printf("I have event %d\n", event->type);

	switch (event->type)
	{
	case XINE_EVENT_UI_PLAYBACK_FINISHED:
		printf("XINE_EVENT_UI_PLAYBACK_FINISHED\n");
		break;
	case XINE_EVENT_NBC_STATS:
		return;
	case XINE_EVENT_FRAME_FORMAT_CHANGE:
		printf("XINE_EVENT_FRAME_FORMAT_CHANGE\n");
		{
			xine_format_change_data_t* data = (xine_format_change_data_t*)event->data;
			printf("width %d  height %d  aspect %d\n", data->width, data->height, data->aspect);

			struct iTSMPEGDecoder::videoEvent evt;
			evt.type = iTSMPEGDecoder::videoEvent::eventSizeChanged;
			xineLib->m_aspect = evt.aspect = data->aspect;
			xineLib->m_height = evt.height = data->height;
			xineLib->m_width  = evt.width  = data->width;
			xineLib->m_pump.send(evt);

			xineLib->adjust_policy();
		}
		return;
	case XINE_EVENT_FRAMERATE_CHANGE:
		printf("XINE_EVENT_FRAMERATE_CHANGE\n");
		{
			xine_framerate_data_t* data = (xine_framerate_data_t*)event->data;
			printf("framerate %d  \n", data->framerate);

			struct iTSMPEGDecoder::videoEvent evt;
			evt.type = iTSMPEGDecoder::videoEvent::eventFrameRateChanged;
			xineLib->m_framerate = evt.framerate = data->framerate;
			xineLib->m_pump.send(evt);
		}
		return;
	case XINE_EVENT_PROGRESS:
		{
			xine_progress_data_t* data = (xine_progress_data_t*) event->data;
			printf("XINE_EVENT_PROGRESS  %s  %d\n", data->description, data->percent);
			if (xineLib->videoPlayed && data->percent==0)
				xineLib->end_of_stream = true;
		}
		break;

	default:
		printf("xine_event_handler(): event->type: %d\n", event->type);
		return;
	}
}

void cXineLib::pumpEvent(const iTSMPEGDecoder::videoEvent &event)
{
	m_event(event);
}

int cXineLib::getVideoWidth()
{
	return m_width;
}

int cXineLib::getVideoHeight()
{
	return m_height;
}

int cXineLib::getVideoFrameRate()
{
	return m_framerate;
}

int cXineLib::getVideoAspect()
{
	return m_aspect;
}

RESULT cXineLib::getPTS(pts_t &pts)
{
	pts_t* last_pts_l = (pts_t*)vo_port->get_property(vo_port, VO_PROP_LAST_PTS);

	pts = *last_pts_l;

	if (pts != 0)
		return 0;
	
	return -1;
}

void cXineLib::setVideoWindow(int window_x, int window_y, int window_width, int window_height)
{
	int left = window_x * windowWidth / osdWidth;
	int top = window_y * windowHeight / osdHeight;
	int width = window_width * windowWidth / osdWidth;
	int height = window_height * windowHeight / osdHeight;

	xine_osd_set_video_window(osd, left, top, width, height);
	showOsd();
}

void cXineLib::updateWindowSize(int width, int height)
{
	windowWidth  = width;
	windowHeight = height;
}

void cXineLib::setDeinterlace(int global, int sd, int hd)
{
	vo_port->set_property(vo_port, VO_PROP_DEINTERLACE_SD, sd);
	vo_port->set_property(vo_port, VO_PROP_DEINTERLACE_HD, hd);
	vo_port->set_property(vo_port, VO_PROP_INTERLACED, global);
}

void cXineLib::setSDfeatures(int sharpness, int noise)
{
	m_sharpness = sharpness;
	m_noise = noise;
}

void cXineLib::setAspectRatio(int ratio)
{
	m_windowAspectRatio = ratio;
}

void cXineLib::setPolicy43(int mode)
{
	m_policy43 = mode;
}

void cXineLib::setPolicy169(int mode)
{
	m_policy169 = mode;
}

void cXineLib::setZoom(int zoom43_x, int zoom43_y, int zoom169_x, int zoom169_y)
{
	m_zoom43_x = zoom43_x;
	m_zoom43_y = zoom43_y;
	m_zoom169_x = zoom169_x;
	m_zoom169_y = zoom169_y;
}

void cXineLib::set_zoom_settings(int x, int y)
{
	xine_set_param(stream, XINE_PARAM_VO_ZOOM_X, x);
	xine_set_param(stream, XINE_PARAM_VO_ZOOM_Y, y);
}

void cXineLib::set_crop_settings(int left, int right, int top, int bottom)
{
	xine_set_param(stream, XINE_PARAM_VO_CROP_LEFT, left);
	xine_set_param(stream, XINE_PARAM_VO_CROP_RIGHT, right);
	xine_set_param(stream, XINE_PARAM_VO_CROP_TOP, top);
	xine_set_param(stream, XINE_PARAM_VO_CROP_BOTTOM, bottom);
}

void cXineLib::adjust_policy()
{
	switch (m_windowAspectRatio) {
	case XINE_VO_ASPECT_AUTO:
		printf("XINE_VO_ASPECT_AUTO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, 0);
		set_zoom_settings(100, 100);
	 	set_crop_settings(0, 0, 0, 0);
		break;
	case XINE_VO_ASPECT_4_3:
		printf("XINE_VO_ASPECT_4_3 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		switch (m_aspect) {
		case 2: // 4:3
			printf("m_policy43 %d\n", m_policy43);
			switch (m_policy43) {
			case 0: // scale
			case 1: // nonlinear
			case 2: // panscan
			case 3: // pillarbox
				printf("4:3 SCALE/NONLINEAR/PANSCAN/PILLARBOX\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, 0, 0);
				break;
			case 4: // zoom
				printf("4:3 ZOOM\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
				set_zoom_settings(m_zoom43_x, m_zoom43_y);
	 			set_crop_settings(0, 0, 0, 0);
				break;
			}
			break;
		case 3: // 16:9
			printf("m_policy169 %d\n", m_policy169);
			switch (m_policy169) {
			case 0: // scale
				printf("16:9 SCALE\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, 0, 0);
				break;
			case 1: // panscan
				printf("16:9 PANSCAN\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
				set_zoom_settings(100, 100);
			 	set_crop_settings(m_width/8, m_width/8, 0, 0);
				break;
			case 2: // letterbox
				printf("16:9 LETTERBOX\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, 0, 0);
				break;
			case 3: // zoom
				printf("16:9 ZOOM\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
				set_zoom_settings(m_zoom169_x, m_zoom169_y);
	 			set_crop_settings(0, 0, 0, 0);
				break;
			}
			break;
		}
		break;
	case XINE_VO_ASPECT_ANAMORPHIC: //16:9
		printf("XINE_VO_ASPECT_ANAMORPHIC (16:9) !!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		switch (m_aspect) {
		case 2: // 4:3
			switch (m_policy43) {
			case 0: // scale
				printf("4:3 SCALE\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, 0, 0);
				break;
			case 1: // nonlinear
				printf("4:3 NONLINEAR\n");
				break;
			case 2: // panscan
				printf("4:3 PANSCAN\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, m_height/8, m_height/8);
				break;
			case 3: // pillarbox
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_4_3);
				set_zoom_settings(100, 100);
				printf("4:3 PILLARBOX\n");
				break;
			case 4: // zoom
				printf("4:3 ZOOM\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
				set_zoom_settings(m_zoom43_x, m_zoom43_y);
	 			set_crop_settings(0, 0, 0, 0);
				break;
			}
			break;
		case 3: // 16:9
			printf("m_policy169 %d\n", m_policy169);
			switch (m_policy169) {
			case 0: // scale
			case 1: // panscan
			case 2: // letterbox
				printf("16:9 SCALE/PANSCAN/LETTERBOX\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_ANAMORPHIC);
				set_zoom_settings(100, 100);
			 	set_crop_settings(0, 0, 0, 0);
				break;
			case 3: // zoom
				printf("16:9 ZOOM\n");
				xine_set_param(stream, XINE_PARAM_VO_ASPECT_RATIO, XINE_VO_ASPECT_AUTO);
				set_zoom_settings(m_zoom169_x, m_zoom169_y);
	 			set_crop_settings(0, 0, 0, 0);
				break;
			}
			break;
		}
		break;
	}

	if (m_width<=720) // SD channels
	{
		vo_port->set_property(vo_port, VO_PROP_SHARPNESS, m_sharpness);
		vo_port->set_property(vo_port, VO_PROP_NOISE_REDUCTION, m_noise);
	}
	else // HD channels
	{
		vo_port->set_property(vo_port, VO_PROP_SHARPNESS, 0);
		vo_port->set_property(vo_port, VO_PROP_NOISE_REDUCTION, 0);
	}
}

