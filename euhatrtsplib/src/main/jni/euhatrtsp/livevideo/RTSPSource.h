#ifndef H_AV_STREAM_RTSP_SOURCE
#define H_AV_STREAM_RTSP_SOURCE

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include "vlc_url.h"
#include <vector>
#include "librtspsource.h"
#include <pthread.h>

namespace avstream {
	class  RTSPSource;
	class RTSPClientVlc : public RTSPClient
	{
	public:
		RTSPClientVlc( UsageEnvironment& env, char const* rtspURL, int verbosityLevel,
            char const* applicationName, portNumBits tunnelOverHTTPPortNum,int socketNumToServer,
			RTSPSource *p_sys) :
		RTSPClient( env, rtspURL, verbosityLevel, applicationName,
            tunnelOverHTTPPortNum,socketNumToServer )
		{
			this->p_sys = p_sys;
		}

		virtual ~RTSPClientVlc() {
			fprintf(stderr, "%s\n", __FUNCTION__);
		}

		RTSPSource *p_sys;
	};

	typedef struct _live_track_t {
		int streamType;
		MediaSubsession *sub;
		RTSPSource *p_sys;
		uint8_t *p_buffer;
		int i_buffer;
		int waiting;
	}live_track_t;
	
	// port from VLC
	class Live555 {
	public:
		static bool wait_Live555_response(RTSPSource *p_demux, int i_timeout = 0 /* ms */ );
		static void TaskInterruptRTSP( void *p_private );
		static void TaskInterruptData( void *p_private );
		static void default_live555_callback( RTSPClient* client, int result_code, char* result_string );
		static void continueAfterDESCRIBE( RTSPClient* client, int result_code, char* result_string );
		static void continueAfterOPTIONS( RTSPClient* client, int result_code, char* result_string );
		static void StreamClose(void *p_private);
		static void StreamRead( void *p_private, unsigned int i_size,
			unsigned int i_truncated_bytes, struct timeval pts,
			unsigned int duration );
	};

	class RTSPSource {
	private:
		MediaSession     *ms_;
		TaskScheduler    *scheduler_;
		UsageEnvironment *env_ ;
		RTSPClientVlc    *rtsp_;
		StreamCallbackFunc streamCallback_;
        DisConnectNotifyFunc disconnectCallback_;
		void* context_;
		bool isPlay_;
		int flags_;
		int timeout_;
		vlc_url_t url_;
		char* psz_url_;

		bool opened_;

		std::vector<live_track_t*> tracks_;

	private: // for VLC
		int result_code;
		bool b_error;
		char event_rtsp;
		unsigned char event_data;
		bool b_no_data;
		int i_no_data_ti;
		int i_live555_ret;

		char* p_sdp;
		double i_npt_start;
		double i_npt_length;
		int i_track ;
		int waiting;

#ifdef WIN32
		HANDLE demuxThread_;
#else
		pthread_t demuxThread_;
#endif

	public:
		MediaSession* ms() {return ms_;}
		TaskScheduler* scheduler() {return scheduler_;}
		UsageEnvironment* env() {return env_;}
		RTSPClient* rtsp() {return rtsp_;}

		int numOfTracks() {return i_track;}
		bool isRTPOverRTSP() {return (flags_ & RTP_OVER_RTSP) != 0;}
		bool isRTPRawFrame() {return (flags_ & RTP_RAW_FRAME) != 0;}
		bool isPlay() {return isPlay_;}
		bool isOpened() {return opened_;}
	
		
		vlc_url_t& url() {return url_;}
		const vlc_url_t& url() const {return url_;}


#ifdef WIN32
		static unsigned __stdcall demuxLoop(void*);
#else
		static void* demuxLoop(void*);
#endif

	public:
		RTSPSource();
		~RTSPSource();
		void setStreamCallback(StreamCallbackFunc callback, void* context);
        void setDisconnectCallback(DisConnectNotifyFunc callback);

		bool open(const char* url, int flags, int timeout);
		int sessionsSetup();
		bool play();
		bool stop();
		void close();
		int demux();

		char* getSDP();
		const std::vector<live_track_t*>& getTracks();
		bool connect();

		friend class Live555;
	};
};

#endif
