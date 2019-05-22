#ifndef H_RawRTPSource
#define H_RawRTPSource

#include "RTPSource.hh"

namespace avstream{

	class RawRTPSource;
	class BufferedPacket {
	public:
		BufferedPacket();
		virtual ~BufferedPacket();

		Boolean hasUsableData() const { return fTail > fHead; }
		unsigned useCount() const { return fUseCount; }

		Boolean fillInData(RTPInterface& rtpInterface, Boolean& packetReadWasIncomplete);
		void assignMiscParams(unsigned short rtpSeqNo, unsigned rtpTimestamp,
		struct timeval presentationTime,
			Boolean hasBeenSyncedUsingRTCP,
			Boolean rtpMarkerBit, struct timeval timeReceived);
		void skip(unsigned numBytes); // used to skip over an initial header
		void removePadding(unsigned numBytes); // used to remove trailing bytes
		void appendData(unsigned char* newData, unsigned numBytes);
		void use(unsigned char* to, unsigned toSize,
			unsigned& bytesUsed, unsigned& bytesTruncated,
			unsigned short& rtpSeqNo, unsigned& rtpTimestamp,
		struct timeval& presentationTime,
			Boolean& hasBeenSyncedUsingRTCP, Boolean& rtpMarkerBit);

		BufferedPacket*& nextPacket() { return fNextPacket; }

		unsigned short rtpSeqNo() const { return fRTPSeqNo; }
		struct timeval const& timeReceived() const { return fTimeReceived; }

		unsigned char* data() const { return &fBuf[fHead]; }
		unsigned dataSize() const { return fTail-fHead; }
		Boolean rtpMarkerBit() const { return fRTPMarkerBit; }
		Boolean& isFirstPacket() { return fIsFirstPacket; }
		unsigned bytesAvailable() const { return fPacketSize - fTail; }
		void rewind();

	protected:
		virtual void reset();
		virtual unsigned nextEnclosedFrameSize(unsigned char*& framePtr,
			unsigned dataSize);
		// The above function has been deprecated.  Instead, new subclasses should use:
		virtual void getNextEnclosedFrameParameters(unsigned char*& framePtr,
			unsigned dataSize,
			unsigned& frameSize,
			unsigned& frameDurationInMicroseconds);

		unsigned fPacketSize;
		unsigned char* fBuf;
		unsigned fHead;
		unsigned fTail;

	private:
		BufferedPacket* fNextPacket; // used to link together packets

		unsigned fUseCount;
		unsigned short fRTPSeqNo;
		unsigned fRTPTimestamp;
		struct timeval fPresentationTime; // corresponding to "fRTPTimestamp"
		Boolean fHasBeenSyncedUsingRTCP;
		Boolean fRTPMarkerBit;
		Boolean fIsFirstPacket;
		struct timeval fTimeReceived;
	};
	class BufferedPacketFactory {
	public:
		BufferedPacketFactory();
		virtual ~BufferedPacketFactory();

		virtual BufferedPacket* createNewPacket(RawRTPSource* ourSource);
	};


	class RawRTPSource : public RTPSource
	{
	protected:
		RawRTPSource(UsageEnvironment& env, Groupsock* RTPgs,
			unsigned char rtpPayloadFormat,
			unsigned rtpTimestampFrequency,
			BufferedPacketFactory* packetFactory = NULL);
		// virtual base class
		virtual ~RawRTPSource();

		virtual Boolean processSpecialHeader(BufferedPacket* packet,
			unsigned& resultSpecialHeaderSize);
		// Subclasses redefine this to handle any special, payload format
		// specific header that follows the RTP header.

		virtual Boolean packetIsUsableInJitterCalculation(unsigned char* packet,
			unsigned packetSize);
		// The default implementation returns True, but this can be redefined

	protected:
		Boolean fCurrentPacketBeginsFrame;
		Boolean fCurrentPacketCompletesFrame;

	protected:
		// redefined virtual functions:
		virtual void doStopGettingFrames();

	private:
		// redefined virtual functions:
		virtual void doGetNextFrame();
		virtual void setPacketReorderingThresholdTime(unsigned uSeconds);

	private:
		void reset();
		void doGetNextFrame1();

		static void networkReadHandler(RawRTPSource* source, int /*mask*/);
		void networkReadHandler1();

		Boolean fAreDoingNetworkReads;
		BufferedPacket* fPacketReadInProgress;
		Boolean fNeedDelivery;
		Boolean fPacketLossInFragmentedFrame;
		unsigned char* fSavedTo;
		unsigned fSavedMaxSize;

		// A buffer to (optionally) hold incoming pkts that have been reorderered
		class ReorderingPacketBuffer* fReorderingBuffer;

	public:
		static RawRTPSource*
			createNew(UsageEnvironment& env, Groupsock* RTPgs,
			unsigned char rtpPayloadFormat,
			unsigned rtpTimestampFrequency = 90000);
	};


};
#endif

