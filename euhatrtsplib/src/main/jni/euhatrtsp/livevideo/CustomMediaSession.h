#pragma once
#include "MediaSession.hh"
namespace avstream {
class CustomMediaSession : public MediaSession
{
protected:
	CustomMediaSession(UsageEnvironment& env);
	virtual MediaSubsession* createNewMediaSubsession(MediaSession& parent);
	virtual MediaSubsession* createNewMediaSubsession();
public:
	static CustomMediaSession* createNew(UsageEnvironment& env,
		char const* sdpDescription);

	virtual ~CustomMediaSession(void);

};

class CustomMediaSubsession : public MediaSubsession
{
protected:
	virtual Boolean createSourceObjects(int useSpecialRTPoffset);
	CustomMediaSubsession(MediaSession& parent);
public:
	virtual ~CustomMediaSubsession(void);
	friend class CustomMediaSession;
};


};
