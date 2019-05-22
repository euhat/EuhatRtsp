#include "CustomMediaSession.h"
#include "RawRTPSource.h"

namespace avstream {

	CustomMediaSession::CustomMediaSession(UsageEnvironment& env): MediaSession(env)
	{

	}


	CustomMediaSession::~CustomMediaSession(void)
	{	
		fprintf(stderr, "%s\n", __FUNCTION__);
	}

	CustomMediaSession* CustomMediaSession::createNew(UsageEnvironment& env, char const* sdpDescription)
	{
		CustomMediaSession* newSession = new CustomMediaSession(env);
		if (newSession != NULL) {
			if (!newSession->initializeWithSDP(sdpDescription)) {
				delete newSession;
				return NULL;
			}
		}

		return newSession;
	}

	MediaSubsession* CustomMediaSession::createNewMediaSubsession(MediaSession &parent) {
		return new CustomMediaSubsession(parent);
	}

	MediaSubsession* CustomMediaSession::createNewMediaSubsession() {
		return new CustomMediaSubsession(*this);
	}


	CustomMediaSubsession::CustomMediaSubsession(MediaSession &parent) : MediaSubsession(parent) {

	}

	Boolean CustomMediaSubsession::createSourceObjects(int useSpecialRTPoffset) {
		fprintf(stderr, "media: %s codec: %s\n", this->mediumName(), this->fCodecName);
		fReadSource = fRTPSource
			= RawRTPSource::createNew(env(), fRTPSocket,
			fRTPPayloadFormat,
			fRTPTimestampFrequency);
		return True;
	}

	CustomMediaSubsession::~CustomMediaSubsession() {
		fprintf(stderr, "%s\n", __FUNCTION__);
	}

};
