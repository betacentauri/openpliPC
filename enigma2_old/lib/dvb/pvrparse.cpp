#include <lib/dvb/pvrparse.h>
#include <lib/base/eerror.h>
#include <byteswap.h>

#ifndef BYTE_ORDER
#error no byte order defined!
#endif

eMPEGStreamInformation::eMPEGStreamInformation():
	m_structure_cache_entries(0),
	m_structure_read(NULL)
{
}

eMPEGStreamInformation::~eMPEGStreamInformation()
{
	if (m_structure_read)
		fclose(m_structure_read);
}

int eMPEGStreamInformation::load(const char *filename)
{
	std::string s_filename(filename);
	if (m_structure_read)
		fclose(m_structure_read);
	m_structure_read = fopen((s_filename + ".sc").c_str(), "rb");
	m_access_points.clear();
	m_pts_to_offset.clear();
	m_timestamp_deltas.clear();
	FILE *f = fopen((s_filename + ".ap").c_str(), "rb");
	if (!f)
		return -1;
	while (1)
	{
		unsigned long long d[2];
		if (fread(d, sizeof(d), 1, f) < 1)
			break;
		
#if BYTE_ORDER == LITTLE_ENDIAN
		d[0] = bswap_64(d[0]);
		d[1] = bswap_64(d[1]);
#endif
		m_access_points[d[0]] = d[1];
		m_pts_to_offset.insert(std::pair<pts_t,off_t>(d[1], d[0]));
	}
	fclose(f);
	fixupDiscontinuties();
	return 0;
}

void eMPEGStreamInformation::fixupDiscontinuties()
{
	if (m_access_points.empty())
		return;
		/* if we have no delta at the beginning, extrapolate it */
	if ((m_access_points.find(0) == m_access_points.end()) && (m_access_points.size() > 1))
	{
		std::map<off_t,pts_t>::const_iterator second = m_access_points.begin();
		std::map<off_t,pts_t>::const_iterator first  = second++;
		if (first->first < second->first) /* i.e., not equal or broken */
		{
			off_t diff = second->first - first->first;
			pts_t tdiff = second->second - first->second;
			tdiff *= first->first;
			tdiff /= diff;
			m_timestamp_deltas[0] = first->second - tdiff;
//			eDebug("first delta is %08llx", first->second - tdiff);
		}
	}

	if (m_timestamp_deltas.empty())
		m_timestamp_deltas[m_access_points.begin()->first] = m_access_points.begin()->second;

	pts_t currentDelta = m_timestamp_deltas.begin()->second, lastpts_t = 0;
	for (std::map<off_t,pts_t>::const_iterator i(m_access_points.begin()); i != m_access_points.end(); ++i)
	{
		pts_t current = i->second - currentDelta;
		pts_t diff = current - lastpts_t;
		
		if (llabs(diff) > (90000*10)) // 10sec diff
		{
//			eDebug("%llx < %llx, have discont. new timestamp is %llx (diff is %llx)!", current, lastpts_t, i->second, diff);
			currentDelta = i->second - lastpts_t; /* FIXME: should be the extrapolated new timestamp, based on the current rate */
//			eDebug("current delta now %llx, making current to %llx", currentDelta, i->second - currentDelta);
			m_timestamp_deltas[i->first] = currentDelta;
		}
		lastpts_t = i->second - currentDelta;
	}
}

pts_t eMPEGStreamInformation::getDelta(off_t offset)
{
	if (!m_timestamp_deltas.size())
		return 0;
	std::map<off_t,pts_t>::iterator i = m_timestamp_deltas.upper_bound(offset);
	/* i can be the first when you query for something before the first PTS */
	if (i != m_timestamp_deltas.begin())
		--i;
	return i->second;
}

// fixupPTS is apparently called to get UI time information and such
int eMPEGStreamInformation::fixupPTS(const off_t &offset, pts_t &ts)
{
	//eDebug("eMPEGStreamInformation::fixupPTS(offset=%llu pts=%llu)", offset, ts);
	if (m_timestamp_deltas.empty())
		return -1;

	std::multimap<pts_t, off_t>::const_iterator 
		l = m_pts_to_offset.upper_bound(ts - 60 * 90000), 
		u = m_pts_to_offset.upper_bound(ts + 60 * 90000), 
		nearest = m_pts_to_offset.end();

	while (l != u)
	{
		if ((nearest == m_pts_to_offset.end()) || (llabs(l->first - ts) < llabs(nearest->first - ts)))
			nearest = l;
		++l;
	}
	if (nearest == m_pts_to_offset.end())
		return 1;

	ts -= getDelta(nearest->second);

	return 0;
}

// getPTS is typically called when you "jump" in a file.
int eMPEGStreamInformation::getPTS(off_t &offset, pts_t &pts)
{
	//eDebug("eMPEGStreamInformation::getPTS(offset=%llu, pts=%llu)", offset, pts);
	std::map<off_t,pts_t>::iterator before = m_access_points.lower_bound(offset);

		/* usually, we prefer the AP before the given offset. however if there is none, we take any. */
	if (before != m_access_points.begin())
		--before;
	
	if (before == m_access_points.end())
	{
		pts = 0;
		return -1;
	}
	
	offset = before->first;
	pts = before->second - getDelta(offset);
	
	return 0;
}

pts_t eMPEGStreamInformation::getInterpolated(off_t offset)
{
		/* get the PTS values before and after the offset. */
	std::map<off_t,pts_t>::iterator before, after;
	after = m_access_points.upper_bound(offset);
	before = after;

	if (before != m_access_points.begin())
		--before;
	else	/* we query before the first known timestamp ... FIXME */
		return 0;

		/* empty... */
	if (before == m_access_points.end())
		return 0;

		/* if after == end, then we need to extrapolate ... FIXME */
	if ((before->first == offset) || (after == m_access_points.end()))
		return before->second - getDelta(offset);
	
	pts_t before_ts = before->second - getDelta(before->first);
	pts_t after_ts = after->second - getDelta(after->first);
	
//	eDebug("%08llx .. ? .. %08llx", before_ts, after_ts);
//	eDebug("%08llx .. %08llx .. %08llx", before->first, offset, after->first);
	
	pts_t diff = after_ts - before_ts;
	off_t diff_off = after->first - before->first;
	
	diff = (offset - before->first) * diff / diff_off;
//	eDebug("%08llx .. %08llx .. %08llx", before_ts, before_ts + diff, after_ts);
	return before_ts + diff;
}
 
off_t eMPEGStreamInformation::getAccessPoint(pts_t ts, int marg)
{
	//eDebug("eMPEGStreamInformation::getAccessPoint(ts=%llu, marg=%d)", ts, marg);
		/* FIXME: more efficient implementation */
	off_t last = 0;
	off_t last2 = 0;
	ts += 1; // Add rounding error margin
	for (std::map<off_t, pts_t>::const_iterator i(m_access_points.begin()); i != m_access_points.end(); ++i)
	{
		pts_t delta = getDelta(i->first);
		pts_t c = i->second - delta;
		if (c > ts) {
			if (marg > 0)
				return (last + i->first)/376*188;
			else if (marg < 0)
				return (last + last2)/376*188;
			else
				return last;
		}
		last2 = last;
		last = i->first;
	}
	if (marg < 0)
		return (last + last2)/376*188;
	else
		return last;
}

int eMPEGStreamInformation::getNextAccessPoint(pts_t &ts, const pts_t &start, int direction)
{
	if (m_access_points.empty())
	{
		eDebug("can't get next access point without streaminfo (yet)");
		return -1;
	}
	off_t offset = getAccessPoint(start);
	std::map<off_t, pts_t>::const_iterator i = m_access_points.find(offset);
	if (i == m_access_points.end())
	{
		eDebug("getNextAccessPoint: initial AP not found");
		return -1;
	}
	pts_t c1 = i->second - getDelta(i->first);
	while (direction)
	{
		while (direction > 0)
		{
			if (i == m_access_points.end())
				return -1;
			++i;
			pts_t c2 = i->second - getDelta(i->first);
			if (c1 == c2) { // Discontinuity
				++i;
				c2 = i->second - getDelta(i->first);
			}
			c1 = c2;
			direction--;
		}
		while (direction < 0)
		{
			if (i == m_access_points.begin())
			{
				eDebug("getNextAccessPoint at start");
				return -1;
			}
			--i;
			pts_t c2 = i->second - getDelta(i->first);
			if (c1 == c2) { // Discontinuity
				--i;
				c2 = i->second - getDelta(i->first);
			}
			c1 = c2;
			direction++;
		}
	}
	ts = i->second - getDelta(i->first);
	eDebug("getNextAccessPoint fine, at %lld - %lld = %lld", ts, i->second, getDelta(i->first));
	return 0;
}

#if BYTE_ORDER != BIG_ENDIAN
#	define structureCacheOffset(i) ((off_t)bswap_64(m_structure_cache[(i)*2]))
#	define structureCacheData(i) ((off_t)bswap_64(m_structure_cache[(i)*2+1]))
#else
#	define structureCacheOffset(i) ((off_t)m_structure_cache[(i)*2])
#	define structureCacheData(i) ((off_t)m_structure_cache[(i)*2+1])
#endif
static const int entry_size = 16;

int eMPEGStreamInformation::loadCache(int index)
{
	const int structure_cache_size = sizeof(m_structure_cache) / entry_size;
	fseek(m_structure_read, index * entry_size, SEEK_SET);
	int num = fread(m_structure_cache, entry_size, structure_cache_size, m_structure_read);
	eDebug("[eMPEGStreamInformation] cache starts at %d entries: %d", index, num);
	m_structure_cache_entries = num;
	return num;
}

int eMPEGStreamInformation::getStructureEntry(off_t &offset, unsigned long long &data, int get_next)
{
	//eDebug("[eMPEGStreamInformation] getStructureEntry(offset=%llu, get_next=%d)", offset, get_next);
	if (!m_structure_read)
	{
		eDebug("getStructureEntry failed because of no m_structure_read");
		return -1;
	}

	const int structure_cache_size = sizeof(m_structure_cache) / entry_size;
	if ((m_structure_cache_entries == 0) ||
	    (structureCacheOffset(0) > offset) ||
	    (structureCacheOffset(m_structure_cache_entries - (get_next ? 2 : 1)) <= offset))
	{
		fseek(m_structure_read, 0, SEEK_END);
		int l = ftell(m_structure_read) / entry_size;
		if (l == 0)
		{
			eDebug("getStructureEntry failed because file size is zero");
			return -1;
		}

		/* do a binary search */
		int count = l;
		int i = 0;
		while (count)
		{
			int step = count >> 1;
			fseek(m_structure_read, (i + step) * entry_size, SEEK_SET);
			unsigned long long d;
			if (!fread(&d, 1, sizeof(d), m_structure_read))
			{
				eDebug("read error at entry %d", i);
				return -1;
			}
#if BYTE_ORDER != BIG_ENDIAN
			d = bswap_64(d);
#endif
			if (d < (unsigned long long)offset)
			{
				i += step + 1;
				count -= step + 1;
			} else
				count = step;
		}
		//eDebug("[eMPEGStreamInformation] found i=%d size=%d get_next=%d", i, l / entry_size, get_next);

		// Put the cache in the center
		if (i + structure_cache_size > l)
		{
			i = l - structure_cache_size; // Near end of file, just fetch the last
		}
		else
		{
			i -= structure_cache_size / 2;
		}
		if (i < 0)
			i = 0;
		int num = loadCache(i);
		if ((num < structure_cache_size) && (structureCacheOffset(num - 1) <= offset))
		{
			eDebug("[eMPEGStreamInformation] offset %lld is past EOF", offset);
			offset = 0x7fffffffffffffffULL;
			data = 0;
			return 0;
		}
	}

	// Binary search for offset
	int i = 0;
	int low = 0;
	int high = m_structure_cache_entries - 1;
	while (low <= high)
	{
		int mid = (low + high) / 2;
		off_t value = structureCacheOffset(mid);
		if (value <= offset)
			low = mid + 1;
		else
			high = mid - 1;
	}
	// Note that low > high
	if (get_next)
	{
		if (i >= m_structure_cache_entries)
			i = m_structure_cache_entries-1;
		else
			i = low;
	}
	else
	{
		if (high >= 0)
			i = high;
		else
			i = 0;
	}

	// eDebug("[eMPEGStreamInformation] search %llu (get_next=%d), found %llu: %llu at %d", offset, get_next, structureCacheOffset(i), structureCacheData(i), i);
	offset = structureCacheOffset(i);
	data = structureCacheData(i);
	return 0;
}


// Get first or last PTS value and offset.
int eMPEGStreamInformation::getFirstFrame(off_t &offset, pts_t& pts)
{
	std::map<off_t,pts_t>::const_iterator entry = m_access_points.begin();
	if (entry != m_access_points.end())
	{
		offset = entry->first;
		pts = entry->second;
		return 0;
	}
	// No access points (yet?) use the .sc data instead
	if (m_structure_read != NULL)
	{
		int num = loadCache(0);
		if (num > 20) num = 20; // We don't need to look that hard, it may be an old file without PTS data
		for (int i = 0; i < num; ++i)
		{
			unsigned long long data = structureCacheData(i);
			if ((data & 0x1000000) != 0)
			{
				pts = data >> 31;
				offset = structureCacheOffset(i);
				return 0;
			}
		}
	}
	return -1;
}
int eMPEGStreamInformation::getLastFrame(off_t &offset, pts_t& pts)
{
	std::map<off_t,pts_t>::const_reverse_iterator entry = m_access_points.rbegin();
	if (entry != m_access_points.rend())
	{
		offset = entry->first;
		pts = entry->second;
		return 0;
	}
	// No access points (yet?) use the .sc data instead
	if (m_structure_read != NULL)
	{
		fseek(m_structure_read, 0, SEEK_END);
		int l = ftell(m_structure_read) / entry_size;
		const int structure_cache_size = sizeof(m_structure_cache) / entry_size;
		int index = l - structure_cache_size;
		if (index < 0)
			index = 0;
		int num = loadCache(index);
		if (num > 10)
			index = num - 10;
		else
			index = 0;
		for (int i = num-1; i >= index; --i)
		{
			unsigned long long data = structureCacheData(i);
			if ((data & 0x1000000) != 0)
			{
				pts = data >> 31;
				offset = structureCacheOffset(i);
				return 0;
			}
		}
	}
	return -1;
}


eMPEGStreamInformationWriter::eMPEGStreamInformationWriter():
	m_structure_write(NULL)
{}

eMPEGStreamInformationWriter::~eMPEGStreamInformationWriter()
{
	if (m_structure_write)
		fclose(m_structure_write);
}

int eMPEGStreamInformationWriter::startSave(const std::string& filename)
{
	m_filename = filename;
	m_structure_write = fopen((m_filename + ".sc").c_str(), "wb");
	return 0;
}

int eMPEGStreamInformationWriter::stopSave(void)
{
	if (m_structure_write)
	{
		fclose(m_structure_write);
		m_structure_write = NULL;
	}
	if (m_filename.empty())
		return -1;
	FILE *f = fopen((m_filename + ".ap").c_str(), "wb");
	if (!f)
		return -1;

	for (std::deque<AccessPoint>::const_iterator i(m_access_points.begin()); i != m_access_points.end(); ++i)
	{
		unsigned long long d[2];
#if BYTE_ORDER == BIG_ENDIAN
		d[0] = i->off;
		d[1] = i->pts;
#else
		d[0] = bswap_64(i->off);
		d[1] = bswap_64(i->pts);
#endif
		fwrite(d, sizeof(d), 1, f);
	}
	fclose(f);

	return 0;
}


void eMPEGStreamInformationWriter::writeStructureEntry(off_t offset, unsigned long long data)
{
	unsigned long long d[2];
#if BYTE_ORDER == BIG_ENDIAN
	d[0] = offset;
	d[1] = data;
#else
	d[0] = bswap_64(offset);
	d[1] = bswap_64(data);
#endif
	if (m_structure_write)
		fwrite(d, sizeof(d), 1, m_structure_write);
}



eMPEGStreamParserTS::eMPEGStreamParserTS():
	m_pktptr(0),
	m_pid(-1),
	m_streamtype(-1),
	m_need_next_packet(0),
	m_skip(0),
	m_last_pts_valid(0)
{
}

int eMPEGStreamParserTS::processPacket(const unsigned char *pkt, off_t offset)
{
	if (!wantPacket(pkt))
		eWarning("something's wrong.");

	const unsigned char *end = pkt + 188, *begin = pkt;
	
	int pusi = !!(pkt[1] & 0x40);
	
	if (!(pkt[3] & 0x10)) /* no payload? */
		return 0;

	if (pkt[3] & 0x20) // adaptation field present?
		pkt += pkt[4] + 4 + 1;  /* skip adaptation field and header */
	else
		pkt += 4; /* skip header */

	if (pkt > end)
	{
		eWarning("[TSPARSE] dropping huge adaption field");
		return 0;
	}

	pts_t pts = 0;
	int ptsvalid = 0;
	
	if (pusi)
	{
			// ok, we now have the start of the payload, aligned with the PES packet start.
		if (pkt[0] || pkt[1] || (pkt[2] != 1))
		{
			eWarning("broken startcode");
			return 0;
		}

		if (pkt[7] & 0x80) // PTS present?
		{
			pts  = ((unsigned long long)(pkt[ 9]&0xE))  << 29;
			pts |= ((unsigned long long)(pkt[10]&0xFF)) << 22;
			pts |= ((unsigned long long)(pkt[11]&0xFE)) << 14;
			pts |= ((unsigned long long)(pkt[12]&0xFF)) << 7;
			pts |= ((unsigned long long)(pkt[13]&0xFE)) >> 1;
			ptsvalid = 1;
			
			m_last_pts = pts;
			m_last_pts_valid = 1;

	#if 0		
			int sec = pts / 90000;
			int frm = pts % 90000;
			int min = sec / 60;
			sec %= 60;
			int hr = min / 60;
			min %= 60;
			int d = hr / 24;
			hr %= 24;
			
			eDebug("pts: %016llx %d:%02d:%02d:%02d:%05d", pts, d, hr, min, sec, frm);
	#endif
		}
		
			/* advance to payload */
		pkt += pkt[8] + 9;
	}

	while (pkt < (end-4))
	{
		int pkt_offset = pkt - begin;
		if (!(pkt[0] || pkt[1] || (pkt[2] != 1)))
		{
//			eDebug("SC %02x %02x %02x %02x, %02x", pkt[0], pkt[1], pkt[2], pkt[3], pkt[4]);
			unsigned int sc = pkt[3];
			
			if (m_streamtype == 0) /* mpeg2 */
			{
				if ((sc == 0x00) || (sc == 0xb3) || (sc == 0xb8)) /* picture, sequence, group start code */
				{
					if (sc == 0xb3) /* sequence header */
					{
						if (ptsvalid)
						{
							addAccessPoint(offset, pts);
							//eDebug("Sequence header at %llx, pts %llx", offset, pts);
						}
					}
					if (pkt <= (end - 6))
					{
						unsigned long long data = sc | ((unsigned)pkt[4] << 8) | ((unsigned)pkt[5] << 16);
						if (ptsvalid) // If available, add timestamp data as well. PTS = 33 bits
							data |= (pts << 31) | 0x1000000;
						writeStructureEntry(offset + pkt_offset, data);
					}
					else
					{
						// Returning non-zero suggests we need more data. This does not
						// work, and never has, so we should make this a void function
						// or fix that...
						return 1;
					}
				}
			}
			else if (m_streamtype == 1) /* H.264 */
			{
				if (sc == 0x09)
				{
					/* store image type */
					unsigned long long data = sc | (pkt[4] << 8);
					if (ptsvalid) // If available, add timestamp data as well. PTS = 33 bits
						data |= (pts << 31) | 0x1000000;
					writeStructureEntry(offset + pkt_offset, data);
					if ( //pkt[3] == 0x09 &&   /* MPEG4 AVC NAL unit access delimiter */
						 (pkt[4] >> 5) == 0) /* and I-frame */
					{
						if (ptsvalid)
						{
							addAccessPoint(offset, pts);
							// eDebug("MPEG4 AVC UAD at %llx, pts %llx", offset, pts);
						}
					}
				}
			}
		}
		++pkt;
	}
	return 0;
}

inline int eMPEGStreamParserTS::wantPacket(const unsigned char *hdr) const
{
	if (hdr[0] != 0x47)
	{
		eDebug("missing sync!");
		return 0;
	}
	int ppid = ((hdr[1]&0x1F) << 8) | hdr[2];

	if (ppid != m_pid)
		return 0;
		
	if (m_need_next_packet)  /* next packet (on this pid) was required? */
		return 1;
	
	if (hdr[1] & 0x40)	 /* pusi set: yes. */
		return 1;

	return m_streamtype == 0; /* we need all packets for MPEG2, but only PUSI packets for H.264 */
}

void eMPEGStreamParserTS::parseData(off_t offset, const void *data, unsigned int len)
{
	const unsigned char *packet = (const unsigned char*)data;
	const unsigned char *packet_start = packet;
	
			/* sorry for the redundant code here, but there are too many special cases... */
	while (len)
	{
			/* emergency resync. usually, this should not happen, because the data should 
			   be sync-aligned.
			   
			   to make this code work for non-strictly-sync-aligned data, (for example, bad 
			   files) we fix a possible resync here by skipping data until the next 0x47.
			   
			   if this is a false 0x47, the packet will be dropped by wantPacket, and the
			   next time, sync will be re-established. */
		int skipped = 0;
		while (!m_pktptr && len)
		{
			if (packet[0] == 0x47)
				break;
			len--;
			packet++;
			skipped++;
		}
		
		if (skipped)
			eDebug("SYNC LOST: skipped %d bytes.", skipped);
		
		if (!len)
			break;
		
		if (m_pktptr)
		{
				/* skip last packet */
			if (m_pktptr < 0)
			{
				unsigned int skiplen = -m_pktptr;
				if (skiplen > len)
					skiplen = len;
				packet += skiplen;
				len -= skiplen;
				m_pktptr += skiplen;
				continue;
			} else if (m_pktptr < 4) /* header not complete, thus we don't know if we want this packet */
			{
				unsigned int storelen = 4 - m_pktptr;
				if (storelen > len)
					storelen = len;
				memcpy(m_pkt + m_pktptr, packet,  storelen);
				
				m_pktptr += storelen;
				len -= storelen;
				packet += storelen;
				
				if (m_pktptr == 4)
					if (!wantPacket(m_pkt))
					{
							/* skip packet */
						packet += 184;
						len -= 184;
						m_pktptr = 0;
						continue;
					}
			}
				/* otherwise we complete up to the full packet */
			unsigned int storelen = 188 - m_pktptr;
			if (storelen > len)
				storelen = len;
			memcpy(m_pkt + m_pktptr, packet,  storelen);
			m_pktptr += storelen;
			len -= storelen;
			packet += storelen;
			
			if (m_pktptr == 188)
			{
				m_need_next_packet = processPacket(m_pkt, offset + (packet - packet_start));
				m_pktptr = 0;
			}
		} else if (len >= 4)  /* if we have a full header... */
		{
			if (wantPacket(packet))  /* decide wheter we need it ... */
			{
				if (len >= 188)          /* packet complete? */
				{
					m_need_next_packet = processPacket(packet, offset + (packet - packet_start)); /* process it now. */
				} else
				{
					memcpy(m_pkt, packet, len);  /* otherwise queue it up */
					m_pktptr = len;
				}
			}

				/* skip packet */
			int sk = len;
			if (sk >= 188)
				sk = 188;
			else if (!m_pktptr) /* we dont want this packet, otherwise m_pktptr = sk (=len) > 4 */
				m_pktptr = sk - 188;

			len -= sk;
			packet += sk;
		} else             /* if we don't have a complete header */
		{
			memcpy(m_pkt, packet, len);   /* complete header next time */
			m_pktptr = len;
			packet += len;
			len = 0;
		}
	}
}

void eMPEGStreamParserTS::setPid(int _pid, int type)
{
	m_pktptr = 0;
	m_pid = _pid;
	m_streamtype = type;
}

int eMPEGStreamParserTS::getLastPTS(pts_t &last_pts)
{
	if (!m_last_pts_valid)
	{
		last_pts = 0;
		return -1;
	}
	last_pts = m_last_pts;
	return 0;
}

