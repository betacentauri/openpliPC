#ifndef __lib_gdi_xineLib_h
#define __lib_gdi_xineLib_h

#include <lib/gdi/gpixmap.h>
#include <xine.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <lib/dvb/idvb.h>
#include <lib/base/message.h>

class cXineLib : public Object {
	DECLARE_REF(cXineLib);
private:
	static cXineLib        *instance;

	xine_t                 *xine;
	xine_stream_t          *stream;
	xine_video_port_t      *vo_port;
	xine_audio_port_t      *ao_port;
	xine_osd_t             *osd;
	xine_event_queue_t     *xine_queue;

        xine_streamtype_data_t  videoData, audioData;

	bool                    videoPlayed;
	int                     osdWidth, osdHeight;
	int                     windowWidth, windowHeight;

	int m_width, m_height, m_framerate, m_aspect, m_progressive;
	int m_windowAspectRatio, m_policy43, m_policy169;
	int m_zoom43_x, m_zoom43_y, m_zoom169_x, m_zoom169_y;
	int m_sharpness, m_noise;

	void setStreamType(int video);

	static void xine_event_handler(void *user_data, const xine_event_t *event);

	eFixedMessagePump<iTSMPEGDecoder::videoEvent> m_pump;
	void pumpEvent(const iTSMPEGDecoder::videoEvent &event);

	void set_zoom_settings(int x, int y);
	void set_crop_settings(int left, int right, int top, int bottom);
public:
	bool                    end_of_stream;

	cXineLib(x11_visual_t *vis);
	virtual ~cXineLib();

	static cXineLib *getInstance() { return instance; }

	void setPrebuffer(int prebuffer);
	void setVolume(int value);
	void setVolumeMute(int value);
	void showOsd();
	void newOsd(int width, int height, uint32_t *argb_buffer);
	void playVideo(void);
	void stopVideo(void);
	void setVideoType(int pid, int type);
	void setAudioType(int pid, int type);
	
	//////////////////////////////
	void FilmVideo(char *mrl); 
	int VideoPause();
	int VideoResume();
	int VideoPosisyon();
	int VideoIleriF();
	int VposStream;
	int Vpos; 
	int Vlength;
	int VideoGeriT(pts_t Sar);
    void SeekTo(long long value);
	///////////////////////
	

	Signal1<void, struct iTSMPEGDecoder::videoEvent> m_event;

	int getVideoWidth();
	int getVideoHeight();
	int getVideoFrameRate();
	int getVideoAspect();
	void adjust_policy();
	RESULT getPTS(pts_t &pts);
	void setVideoWindow(int window_x, int window_y, int window_width, int window_height);
	void updateWindowSize(int width, int height);

	void setDeinterlace(int global, int sd, int hd);
	void setSDfeatures(int sharpness, int noise);
	void setAspectRatio(int ratio);
	void setPolicy43(int mode);
	void setPolicy169(int mode);
	void setZoom(int zoom43_x, int zoom43_y, int zoom169_x, int zoom169_y);
};

#endif
