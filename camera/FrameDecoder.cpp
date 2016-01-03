/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Common.h"
#include "FrameDecoder.h"


namespace Ti {
namespace Camera {

FrameDecoder::FrameDecoder()
: mCameraHal(NULL), mState(DecoderState_Uninitialized) {
}

FrameDecoder::~FrameDecoder() {
}

status_t FrameDecoder::start() {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);
    status_t ret;
    if (mState == DecoderState_Running) {
        return NO_INIT;
    }
    ret = doStart();
    if (ret == NO_ERROR) {
        mState = DecoderState_Running;
    }

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

void FrameDecoder::stop() {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);
    if (mState >= DecoderState_Requested_Stop) {
        return;
    }
    mState = DecoderState_Requested_Stop;
    doStop();
    mState = DecoderState_Stoppped;

    LOG_FUNCTION_NAME_EXIT;
}

void FrameDecoder::release() {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);
    if (mState <= DecoderState_Requested_Stop) {
        return;
    }
    doRelease();
    mState = DecoderState_Uninitialized;

    LOG_FUNCTION_NAME_EXIT;
}

void FrameDecoder::flush() {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);
    if (mState <= DecoderState_Requested_Stop) {
        return;
    }
    doFlush();
    mInQueue.clear();
    mOutQueue.clear();


    LOG_FUNCTION_NAME_EXIT;
}

void FrameDecoder::configure(const DecoderParameters& params) {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);
    if (mState == DecoderState_Running) {
        return;
    }
    mParams = params;
    mInQueue.reserve(mParams.inputBufferCount);
    mOutQueue.reserve(mParams.outputBufferCount);
    doConfigure(params);
    mState = DecoderState_Initialized;

    LOG_FUNCTION_NAME_EXIT;
}

status_t FrameDecoder::dequeueInputBuffer(int &id) {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);

    if (mState != DecoderState_Running) {
        CAMHAL_LOGE("Try to use Decoder not in RUNNING state");
        return INVALID_OPERATION;
    }

    for (size_t i = 0; i < mInQueue.size(); i++) {
        int index = mInQueue[i];
        android::sp<MediaBuffer>& in = mInBuffers->editItemAt(index);
        android::AutoMutex bufferLock(in->getLock());
        if (in->getStatus() == BufferStatus_InDecoded) {
            id = index;
            in->setStatus(BufferStatus_Unknown);
            mInQueue.removeAt(i);
            return NO_ERROR;
        }
    }

    LOG_FUNCTION_NAME_EXIT;
    return INVALID_OPERATION;
}

status_t FrameDecoder::dequeueOutputBuffer(int &id) {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);

    if (mState != DecoderState_Running) {
        CAMHAL_LOGE("Try to use Decoder not in RUNNING state");
        return INVALID_OPERATION;
    }

    for (size_t i = 0; i < mOutQueue.size(); i++) {
        int index = mOutQueue[i];
        android::sp<MediaBuffer>& out = mOutBuffers->editItemAt(index);
        android::AutoMutex bufferLock(out->getLock());
        if (out->getStatus() == BufferStatus_OutFilled) {
            id = index;
            out->setStatus(BufferStatus_Unknown);
            mOutQueue.removeAt(i);
            return NO_ERROR;
        }
    }

    LOG_FUNCTION_NAME_EXIT;
    return INVALID_OPERATION;
}

status_t FrameDecoder::queueOutputBuffer(int index) {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);

    //We queue all available buffers to Decoder not in recording mode - before start
    if (mState > DecoderState_Running) {
        CAMHAL_LOGE("Try to use Decoder not in RUNNING state");
        return INVALID_OPERATION;
    }

    android::sp<MediaBuffer>& out = mOutBuffers->editItemAt(index);
    android::AutoMutex bufferLock(out->getLock());
    out->setStatus(BufferStatus_OutQueued);
    mOutQueue.push_back(index);

    LOG_FUNCTION_NAME_EXIT;
    return NO_ERROR;
}

status_t FrameDecoder::queueInputBuffer(int id) {
    LOG_FUNCTION_NAME;

    android::AutoMutex lock(mLock);

    if (mState != DecoderState_Running) {
        CAMHAL_LOGE("Try to use Decoder not in RUNNING state");
        return INVALID_OPERATION;
    }

    {
        android::sp<MediaBuffer>& in = mInBuffers->editItemAt(id);
        android::AutoMutex bufferLock(in->getLock());
        in->setStatus(BufferStatus_InQueued);
        mInQueue.push_back(id);
    }

    // Since we got queued buffer - we can process it
    doProcessInputBuffer();

    LOG_FUNCTION_NAME_EXIT;
    return NO_ERROR;
}


}   // namespace Camera
}   // namespace Ti
