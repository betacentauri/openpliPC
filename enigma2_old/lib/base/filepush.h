#ifndef __lib_base_filepush_h
#define __lib_base_filepush_h

#include <lib/base/thread.h>
#include <lib/base/ioprio.h>
#include <libsig_comp.h>
#include <lib/base/message.h>
#include <sys/types.h>
#include <lib/base/rawfile.h>
//#include <lib/dvb/demux.h>

class iFilePushScatterGather
{
public:
	virtual void getNextSourceSpan(off_t current_offset, size_t bytes_read, off_t &start, size_t &size)=0;
	virtual ~iFilePushScatterGather() {}
};

class eFilePushThread: public eThread, public Object
{

public:
	eFilePushThread(int prio_class=IOPRIO_CLASS_BE, int prio_level=0, int blocksize=188, size_t buffersize=188*1024);
	~eFilePushThread();
	void thread();
	void stop();

	void start(int sourcefd, int destfd);
	int start(const char *filename, int destfd);

	void start(ePtr<iTsSource> &source, int destfd);

	void start(ePtr<eDVBDemux> &demux, int fd, int fd_dest);
	
	void pause();
	void resume();
	
		/* flushes the internal readbuffer */ 
	void flush();
	void enablePVRCommit(int);
	
		/* stream mode will wait on EOF until more data is available. */
	void setStreamMode(int);
	
	void setScatterGather(iFilePushScatterGather *);
	
	enum { evtEOF, evtReadError, evtWriteError, evtUser };
	Signal1<void,int> m_event;

		/* you can send private events if you want */
	void sendEvent(int evt);
protected:
	virtual int filterRecordData(const unsigned char *data, int len, size_t &current_span_remaining);
private:
	int prio_class;
	int prio;
	iFilePushScatterGather *m_sg;
	int m_stop;
	int m_buf_start, m_buf_end, m_filter_end;
	int m_fd_dest;
	int m_send_pvr_commit;
	int m_stream_mode;
	int m_blocksize;
	size_t m_buffersize;
	unsigned char* m_buffer;
	off_t m_current_position;

	ePtr<iTsSource> m_source;

	eFixedMessagePump<int> m_messagepump;

	void recvEvent(const int &evt);
};

class eFilePushThreadRecorder: public eThread, public Object
{
public:
	eFilePushThreadRecorder(int prio_class=IOPRIO_CLASS_BE, int prio_level=0, int blocksize=188, size_t buffersize=188*1024);
	~eFilePushThreadRecorder();
	void thread();
	void stop();
	void start(int sourcefd, int destfd);
  void start(ePtr<eDVBDemux> &demux, int fd, int fd_dest);

	enum { evtEOF, evtReadError, evtWriteError, evtUser };
	Signal1<void,int> m_event;

	void sendEvent(int evt);
protected:
	virtual void filterRecordData(const unsigned char *data, int len) = 0;
private:
	int prio_class;
	int prio;
	int m_stop;
	int m_fd_dest;
	int m_fd_source;
	int m_blocksize;
	size_t m_buffersize;
	unsigned char* m_buffer;
	off_t m_current_position;
	eFixedMessagePump<int> m_messagepump;
	void recvEvent(const int &evt);
};

#endif
