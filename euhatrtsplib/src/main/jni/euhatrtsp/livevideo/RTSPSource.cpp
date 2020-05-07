#include <stdint.h>
#include "RTSPSource.h"
#include "CustomMediaSession.h"
#include "RawRTPSource.h"
#include "vlc_url.h"

#ifdef WIN32
#include <process.h>
#define pthread_mutex_init(mutex, x) InitializeCriticalSection((mutex))
#define pthread_mutex_unlock LeaveCriticalSection
#define pthread_mutex_lock EnterCriticalSection
#define pthread_mutex_destroy DeleteCriticalSection
#define pthread_join(thread) do{ \
    if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {assert(0);} \
    CloseHandle(thread); \
    }while(0)
#else
#include <pthread.h>
#endif

#define demux_sys_t RTSPSource

namespace avstream {
static inline const char *strempty( const char *s ) { return s?s:""; }
static inline Boolean toBool( bool b ) { return b?True:False; } // silly, no?
RTSPSource::RTSPSource() : ms_(NULL),
    scheduler_(NULL),
    env_(NULL),
    rtsp_(NULL),
    streamCallback_(NULL),
    disconnectCallback_(NULL),
    context_(NULL),
    isPlay_(false),
    flags_(0),
    timeout_(0),
    psz_url_(NULL),
    opened_(false),
//  tracks_(NULL),
    result_code(0),
    b_error(false),
    event_rtsp(0),
    event_data(0),
    b_no_data(false),
    i_no_data_ti(0),
    i_live555_ret(-1),
    p_sdp(NULL),
    i_npt_start(0.0),
    i_npt_length(0.0),
    i_track(0),
    waiting(0)
{
    memset(&url_,0,sizeof(url_));
}

RTSPSource::~RTSPSource() {
    fprintf(stderr, "%s\n", __FUNCTION__);
    close();
}

void RTSPSource::setStreamCallback(StreamCallbackFunc callback, void* context) {
    this->streamCallback_ = callback;
    this->context_ = context;
}

void RTSPSource::setDisconnectCallback(DisConnectNotifyFunc callback)
{
    this->disconnectCallback_ = callback;
}

bool RTSPSource::open(const char* url, int flags, int timeout) {

    this->flags_   = flags;
    this->timeout_ = timeout;
    psz_url_ = strdup(url);
    vlc_UrlParse( &this->url_, url, 0 );

    this->scheduler_ = BasicTaskScheduler::createNew(0);
    this->env_ = BasicUsageEnvironment::createNew(*scheduler_);

    if (!this->connect())
        return false;

    if(!sessionsSetup())
    {
        return false;
    }

    opened_ = true;

    return true;
}

bool RTSPSource::connect()
{
    demux_sys_t *p_sys = this;
    Authenticator authenticator;
    char *psz_user    = NULL;
    char *psz_pwd     = NULL;
    char *psz_url     = NULL;
    int  i_http_port  = 0;
    bool  i_ret        = true;
    const int i_timeout = timeout_;

    /* Get the user name and password */
    if( p_sys->url_.psz_username || p_sys->url_.psz_password)
    {
        /* Create the URL by stripping away the username/password part */
        if( p_sys->url().i_port == 0 )
            p_sys->url().i_port = 554;
        if( asprintf( &psz_url, "rtsp://%s:%d%s",
                      strempty( p_sys->url().psz_host ),
                      p_sys->url().i_port,
                      strempty( p_sys->url().psz_path ) ) == -1 )
            return false;

        psz_user = strdup( strempty( p_sys->url().psz_username ) );
        psz_pwd  = strdup( strempty( p_sys->url().psz_password ) );
    }
    else
    {
        psz_url = strdup(this->psz_url_);
    }

    p_sys->rtsp_ = new RTSPClientVlc(*p_sys->env_, psz_url, 0, "Poseidon", i_http_port, -1,p_sys );

    authenticator.setUsernameAndPassword( psz_user, psz_pwd );

    p_sys->rtsp_->sendOptionsCommand( &Live555::continueAfterOPTIONS, &authenticator );

    if( !Live555::wait_Live555_response( this, i_timeout ) )
    {
        int i_code = p_sys->i_live555_ret;
        if( i_code == 401 )
        {
            fprintf(stderr, "authentication failed\n" );
        }
        else
        {
            if( i_code == 0 )
                fprintf( stderr, "connection timeout\n" );
            else
                fprintf( stderr, "connection error %d\n", i_code );

            // if( p_sys->rtsp_ ) RTSPClient::close( p_sys->rtsp_ );
            // p_sys->rtsp_ = NULL;
        }
        i_ret = false;
    }

//bailout:
    /* malloc-ated copy */
    free( psz_url );
    free( psz_user );
    free( psz_pwd );

    return i_ret;
}

int RTSPSource::sessionsSetup()
{
    demux_sys_t             *p_sys  = this;
    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;

    bool           b_rtsp_tcp = false;
    int            i_client_port;
    int            i_return = true;
    unsigned int   i_buffer = 0;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */

    b_rtsp_tcp    = isRTPOverRTSP();
    i_client_port = -1;

    if (isRTPRawFrame()) {
        p_sys->ms_ = avstream::CustomMediaSession::createNew( *p_sys->env_, p_sys->p_sdp );
    } else {
        /* Create the session from the SDP */
        p_sys->ms_ = MediaSession::createNew( *p_sys->env_, p_sys->p_sdp);
    }

    /* Create the session from the SDP */
    if(!p_sys->ms() )
    {
        fprintf( stderr, "Could not create the RTSP Session: %s\n",
                 p_sys->env()->getResultMsg() );
        return false;
    }

    /* Initialise each media subsession */
    iter = new MediaSubsessionIterator( *p_sys->ms_ );
    while( ( sub = iter->next() ) != NULL )
    {
        Boolean bInit;
        live_track_t *tk;


        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
            i_buffer = 2000000;
        else continue;

        if( i_client_port != -1 )
        {
            sub->setClientPortNum( i_client_port );
            i_client_port += 2;
        }

        bInit = sub->initiate();

        if( !bInit )
        {
            fprintf( stderr, "RTP subsession '%s/%s' failed (%s)",
                     sub->mediumName(), sub->codecName(),
                     p_sys->env()->getResultMsg() );
        }
        else
        {
            if( sub->rtpSource() != NULL )
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( i_buffer > 0 )
                    increaseReceiveBufferTo( *p_sys->env(), fd, i_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            fprintf( stderr, "RTP subsession '%s/%s' payload %u\n", sub->mediumName(),
                     sub->codecName(), sub->rtpPayloadFormat());

            /* Issue the SETUP */
            if( p_sys->rtsp())
            {
                p_sys->rtsp_->sendSetupCommand( *sub, Live555::default_live555_callback, False,
                                                toBool( b_rtsp_tcp ),
                                                False );
                if( !Live555::wait_Live555_response( this , timeout_) )
                {
                    /* if we get an unsupported transport error, toggle TCP
                        * use and try again */
                    if( p_sys->i_live555_ret == 461 )
                        p_sys->rtsp()->sendSetupCommand( *sub, Live555::default_live555_callback, False,
                                                         !toBool( b_rtsp_tcp ), False );
                    if( p_sys->i_live555_ret != 461 || !Live555::wait_Live555_response( this , timeout_) )
                    {
                        fprintf( stderr, "SETUP of'%s/%s' failed %s\n",
                                 sub->mediumName(), sub->codecName(),
                                 p_sys->env()->getResultMsg() );
                        continue;
                    }
                }
            }

            tk = (live_track_t*)calloc(1, sizeof( live_track_t ) );
            if( !tk )
            {
                delete iter;
                return false;
            }


            if( !strcmp( sub->mediumName(), "audio" ) )
            {
                tk->streamType = AUDIO;
                tk->i_buffer    = 65536;
                tk->p_buffer    = (uint8_t *)malloc(65536);
                if( !tk->p_buffer )
                {
                    free( tk );
                    delete iter;
                    return false;
                }
            }
            else if( !strcmp( sub->mediumName(), "video" ) )
            {
                tk->streamType = VIDEO;
                tk->i_buffer    = 1024*512;
                tk->p_buffer    = (uint8_t *)malloc(1024*512);
                if( !tk->p_buffer )
                {
                    free( tk );
                    delete iter;
                    return false;
                }
            }

            tk->p_sys = p_sys;
            tk->waiting = 0;
            tk->sub = sub;

            p_sys->i_track++;
            tracks_.push_back(tk);
        }
    }
    delete iter;
    if( p_sys->i_track <= 0 ) i_return = false;

    p_sys->i_npt_start = p_sys->ms()->playStartTime();

    p_sys->i_npt_length = p_sys->ms()->playEndTime();

    /* */
    fprintf( stderr, "setup start: %f stop:%f\n", p_sys->i_npt_start, p_sys->i_npt_length );

    /* */
    p_sys->b_no_data = true;
    p_sys->i_no_data_ti = 0;

    return i_return;
}

#ifdef WIN32
unsigned  RTSPSource::demuxLoop(void* arg)
#else
void* RTSPSource::demuxLoop(void* arg)
#endif
{
    RTSPSource *p_sys = (RTSPSource*)arg;

    while (p_sys->isPlay()) {
        p_sys->demux();
    }
    fprintf(stderr, "demux thread done\n");
    return 0;
}


int RTSPSource::demux()
{
    demux_sys_t    *p_sys = this;
    TaskToken      task;

//    bool            b_send_pcr = true;
//    int64_t         i_pcr = 0;
    int             i;

    /* First warn we want to read data */
    p_sys->event_data = 0;
    for( i = 0; i < p_sys->numOfTracks(); i++ )
    {
        live_track_t *tk = p_sys->tracks_[i];
		if(tk == NULL)
		{
			continue;
		}

        if (tk->waiting == 0)
		{
            tk->waiting = 1;
			if(tk->sub && tk->sub->readSource())
            {
            	tk->sub->readSource()->getNextFrame( tk->p_buffer, tk->i_buffer,
                                                 Live555::StreamRead, tk, Live555::StreamClose, tk );
			}
        }

    }
    /* Create a task that will be called if we wait more than 300ms */
	if(p_sys->scheduler())
    {
		// original 300000
	    task = p_sys->scheduler()->scheduleDelayedTask( 1000000, Live555::TaskInterruptData, this );

	    /* Do the read */
	    p_sys->scheduler()->doEventLoop( (char*)&p_sys->event_data );

	    /* remove the task */
	    p_sys->scheduler()->unscheduleDelayedTask( task );
	}

    return p_sys->b_error?0:-1;
}

bool RTSPSource::play() {
    demux_sys_t *p_sys = this;
    p_sys->rtsp()->sendPlayCommand( *p_sys->ms(), Live555::default_live555_callback, p_sys->i_npt_start, -1, 1 );

    if( !Live555::wait_Live555_response(this, timeout_))
    {
        fprintf( stderr, "RTSP PLAY failed %s", p_sys->env()->getResultMsg() );
        return false;
    }

    fprintf(stderr, "play ok, create demux thread\n");

    isPlay_ = true;

#ifdef WIN32
    demuxThread_ = (HANDLE)_beginthreadex(NULL, 0, demuxLoop, this, 0, NULL);
#else
    pthread_create(&demuxThread_, NULL, demuxLoop, this);
#endif


    return true;
}

bool RTSPSource::stop() {
    return false;
}

void RTSPSource::close() {
    demux_sys_t *p_sys = this;
    if (isPlay()) {
        isPlay_ = false;
#ifndef _WIN32
        if (demuxThread_ != 0) {
            pthread_join(demuxThread_, 0);
            demuxThread_ = 0;
        }
#else
        if (demuxThread_ != NULL) {
            pthread_join(demuxThread_,NULL);
            demuxThread_ = NULL;
        }
#endif
    }

    if( this->rtsp_ && this->ms_)   {this->rtsp()->sendTeardownCommand( *this->ms(), NULL );}

    if( p_sys->ms_ )                { Medium::close( p_sys->ms_); p_sys->ms_ = NULL;}
    if (rtsp_)                      { RTSPClient::close(rtsp_); rtsp_ = NULL; }
    if (psz_url_)                   { free(psz_url_); psz_url_ = NULL; }
    if (p_sdp)                      { free(p_sdp); p_sdp = NULL; }
    if (env_)                       { env_->reclaim(); env_ = NULL; }
    if (scheduler_)                 { delete scheduler_; scheduler_ = NULL; }
    vlc_UrlClean( &p_sys->url());

    for (std::vector<live_track_t*>::iterator iter = tracks_.begin(); iter != tracks_.end(); ++iter) {
        live_track_t *track = *(iter);
        free(track->p_buffer);
        free(track);
    }

    tracks_.clear();

    opened_ = false;
}

char* RTSPSource::getSDP() {
    return p_sdp;
}

const std::vector<live_track_t*>& RTSPSource::getTracks() {
    return tracks_;
}


void Live555::TaskInterruptRTSP( void *p_private )
{
    RTSPSource *p_demux = (RTSPSource*)p_private;

    /* Avoid lock */
    p_demux->event_rtsp = 0xff;
}

void Live555::TaskInterruptData( void *p_private )
{
    RTSPSource *p_demux = (RTSPSource*)p_private;

    p_demux->i_no_data_ti++;

    fprintf(stderr, "i_no_data_ti %d\n", p_demux->i_no_data_ti);

    if(p_demux->disconnectCallback_ != NULL && p_demux->i_no_data_ti > 5)
    {
        p_demux->disconnectCallback_();
    }

    /* Avoid lock */
    p_demux->event_data = 0xff;
}


bool Live555::wait_Live555_response(RTSPSource *p_sys, int i_timeout)
{
    TaskToken task;

    p_sys->event_rtsp = 0;
    if( i_timeout > 0 )
    {
        /* Create a task that will be called if we wait more than timeout ms */
        task = p_sys->scheduler()->scheduleDelayedTask( i_timeout*1000,
                                                        TaskInterruptRTSP,
                                                        p_sys );
    }
    p_sys->event_rtsp = 0;
    p_sys->b_error = true;
    p_sys->i_live555_ret = 0;
    p_sys->scheduler()->doEventLoop( &p_sys->event_rtsp );
    //here, if b_error is true and i_live555_ret = 0 we didn't receive a response
    if( i_timeout > 0 )
    {
        /* remove the task */
        p_sys->scheduler()->unscheduleDelayedTask( task );
    }
    return !p_sys->b_error;
}

void Live555::default_live555_callback( RTSPClient* client, int result_code, char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    RTSPSource *p_sys = client_vlc->p_sys;

    if (result_string) delete []result_string;
    p_sys->i_live555_ret = result_code;
    p_sys->b_error = p_sys->i_live555_ret != 0;
    p_sys->event_rtsp = 1;
}

void Live555::continueAfterDESCRIBE( RTSPClient* client, int result_code,
                                     char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    demux_sys_t *p_sys = client_vlc->p_sys;
    p_sys->i_live555_ret = result_code;
    if ( result_code == 0 )
    {
        char* sdpDescription = result_string;
        free( p_sys->p_sdp );
        p_sys->p_sdp = NULL;
        if( sdpDescription )
        {
            p_sys->p_sdp = strdup( sdpDescription );
            p_sys->b_error = false;
        }
    }
    else
        p_sys->b_error = true;
    if (result_string) delete[] result_string;
    p_sys->event_rtsp = 1;
}

void Live555::continueAfterOPTIONS( RTSPClient* client, int result_code,
                                    char* result_string )
{
//    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> (client);
//    demux_sys_t *p_sys = client_vlc->p_sys;
    client->sendDescribeCommand( continueAfterDESCRIBE );
    if (result_string) delete[] result_string;
}

void Live555::StreamClose( void *p_private )
{
    live_track_t   *tk = (live_track_t*)p_private;

    demux_sys_t    *p_sys = tk->p_sys;

    fprintf(stderr, "StreamClose\n" );

    p_sys->event_rtsp = 0xff;
    p_sys->event_data = 0xff;
    p_sys->b_error = true;
}


void Live555::StreamRead( void *p_private, unsigned int i_size,
                          unsigned int i_truncated_bytes, struct timeval pts,
                          unsigned int duration )
{

    live_track_t   *tk = (live_track_t*)p_private;
    demux_sys_t    *p_sys = tk->p_sys;

    int64_t i_pts = (int64_t)pts.tv_sec * 1000000 +
            (int64_t)pts.tv_usec;

    FrameInfoPtr temp = (FrameInfoPtr)calloc(1, sizeof(FrameInfo));
    temp->pts = i_pts;

    if (p_sys->streamCallback_)
    {
        p_sys->streamCallback_(tk->streamType, tk->p_buffer, i_size, p_sys->context_, temp);
    }

    free(temp);

    /* warn that's ok */
    p_sys->event_data = 0xff;

    /* we have read data */
    tk->waiting = 0;
    p_sys->b_no_data = false;
    p_sys->i_no_data_ti = 0;
}

};

