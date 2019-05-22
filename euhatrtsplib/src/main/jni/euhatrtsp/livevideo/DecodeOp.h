#pragma once

typedef int (*EuhatDecoderCallback)(char *outBuf, void *context);

class MemPool;

class EuhatDecoderBase
{
protected:
    EuhatDecoderCallback callback_;
    void *context_;
    MemPool *outputMemPool_;

public:
    virtual ~EuhatDecoderBase();

    void setOutputMemPool(MemPool *pool);
    virtual int init(EuhatDecoderCallback callback, void *context) = 0;
    virtual int fini() = 0;
    virtual int updateSps(const char *sps) = 0;
    virtual int updatePps(const char *pps) = 0;
    virtual int updateWH(int width, int height) = 0;
    virtual int updateSurface(void *surface) = 0;
    virtual int decode(char *frame, int frameLen) = 0;
};

class EuhatDecodeOp
{
public:
    static EuhatDecoderBase *genDecoder();
};