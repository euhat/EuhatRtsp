#include "DecodeOp.h"
#include <stdlib.h>
#include <stdio.h>
#include "DecoderSoft.h"
#include "DecoderHard.h"
#include "CommonOp.h"

EuhatDecoderBase::~EuhatDecoderBase()
{

}

void EuhatDecoderBase::setOutputMemPool(MemPool *pool)
{
    outputMemPool_ = pool;
}

EuhatDecoderBase *EuhatDecodeOp::genDecoder()
{
#if 1
    if (EuhatDecoderHard::canWork()) {
        DBG(("using hard decoder.\n"));
        return new EuhatDecoderHard();
    }
#endif
    DBG(("using soft decoder.\n"));
    return new EuhatDecoderSoft();
}