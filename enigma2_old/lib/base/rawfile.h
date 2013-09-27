#ifndef __lib_base_rawfile_h
#define __lib_base_rawfile_h

#include <string>
#include <lib/base/itssource.h>
#include <lib/base/tsRingbuffer.h>
#include <lib/dvb/demux.h>

class eRawFile: public iTsSource
{
	DECLARE_REF(eRawFile);
public:
	eRawFile();
	virtual ~eRawFile();
	int open(const char *filename, int cached = 0);
	void setfd(int fd);
	int close();

	// iTsSource
	off_t lseek(off_t offset, int whence);
	ssize_t read(off_t offset, void *buf, size_t count);
	off_t length();
	int valid();
protected:
	eSingleLock m_lock;
	int m_fd;     /* for uncached */
private:
	FILE *m_file; /* for cached */
	int m_cached;
	std::string m_basename;
	off_t m_splitsize, m_totallength, m_current_offset, m_base_offset, m_last_offset;
	int m_nrfiles;
	void scan();
	int m_current_file;
	int switchOffset(off_t off);

	off_t lseek_internal(off_t offset, int whence);
	FILE *openFileCached(int nr);
	int openFileUncached(int nr);
};

class eDecryptRawFile: public eRawFile
{
public:
	eDecryptRawFile();
	~eDecryptRawFile();
	void setDemux(ePtr<eDVBDemux> demux);
	ssize_t read(off_t offset, void *buf, size_t count);
private:
	ePtr<eDVBDemux> demux;
	cRingBufferLinear *ringBuffer;
	int bs_size;
	bool delivered;
	int lastPacketsCount;
	bool stream_correct;

	uint8_t* getPackets(int &packetsCount);
};

#endif
