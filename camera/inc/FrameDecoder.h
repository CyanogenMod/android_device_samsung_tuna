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

#ifndef FRAMEDECODER_H_
#define FRAMEDECODER_H_

#include <utils/Vector.h>
#include <utils/StrongPointer.h>
#include "CameraHal.h"


namespace Ti {
namespace Camera {

enum DecoderType {
    DecoderType_MJPEG,
    DecoderType_H264
};

enum BufferStatus {
    BufferStatus_Unknown,
    BufferStatus_InQueued,
    BufferStatus_InWaitForEmpty,
    BufferStatus_InDecoded,
    BufferStatus_OutQueued,
    BufferStatus_OutWaitForFill,
    BufferStatus_OutFilled
};

enum DecoderState {
    DecoderState_Uninitialized,
    DecoderState_Initialized,
    DecoderState_Running,
    DecoderState_Requested_Stop,
    DecoderState_Stoppped
};

class MediaBuffer: public virtual android::RefBase {

public:
    MediaBuffer()
    : bufferId(-1), buffer(0), filledLen(0), size(0),
      mOffset(0), mTimestamp(0), mStatus(BufferStatus_Unknown) {
    }

    MediaBuffer(int id, void* buffer, size_t buffSize = 0)
    : bufferId(id), buffer(buffer), filledLen(0), size(buffSize),
      mOffset(0), mTimestamp(0), mStatus(BufferStatus_Unknown) {
    }

    virtual ~MediaBuffer() {
    }

    int bufferId;
    void* buffer;
    int filledLen;
    size_t size;

    nsecs_t getTimestamp() const {
        return mTimestamp;
    }
    void setTimestamp(nsecs_t ts) {
        mTimestamp = ts;
    }

    BufferStatus getStatus() const {
        return mStatus;
    }

    void setStatus(BufferStatus status) {
        mStatus = status;
    }

    android::Mutex& getLock() const {
        return mLock;
    }

    uint32_t getOffset() const {
        return mOffset;
    }

    void setOffset(uint32_t offset) {
        mOffset = offset;
    }

private:
    uint32_t mOffset;
    nsecs_t mTimestamp;
    BufferStatus mStatus;
    mutable android::Mutex mLock;
};

struct DecoderParameters {
    int width;
    int height;
    int inputBufferCount;
    int outputBufferCount;
};

class FrameDecoder {
public:
    FrameDecoder();
    virtual ~FrameDecoder();
    void configure(const DecoderParameters& config);
    status_t start();
    void stop();
    void release();
    void flush();
    status_t queueInputBuffer(int id);
    status_t dequeueInputBuffer(int &id);
    status_t queueOutputBuffer(int id);
    status_t dequeueOutputBuffer(int &id);

    void registerOutputBuffers(android::Vector< android::sp<MediaBuffer> > *outBuffers) {
        android::AutoMutex lock(mLock);
        mOutQueue.clear();
        mOutBuffers = outBuffers;
    }

    void registerInputBuffers(android::Vector< android::sp<MediaBuffer> > *inBuffers) {
        android::AutoMutex lock(mLock);
        mInQueue.clear();
        mInBuffers = inBuffers;
    }

    virtual bool getPaddedDimensions(size_t &width, size_t &height) {
        return false;
    }

    void setHal(CameraHal* hal) {
        mCameraHal = hal;
    }

protected:
    virtual void doConfigure(const DecoderParameters& config) = 0;
    virtual void doProcessInputBuffer() = 0;
    virtual status_t doStart() = 0;
    virtual void doStop() = 0;
    virtual void doFlush() = 0;
    virtual void doRelease() = 0;

    DecoderParameters mParams;

    android::Vector<int> mInQueue;
    android::Vector<int> mOutQueue;

    android::Vector< android::sp<MediaBuffer> >* mInBuffers;
    android::Vector< android::sp<MediaBuffer> >* mOutBuffers;

    CameraHal* mCameraHal;

private:
    DecoderState mState;
    android::Mutex mLock;
};

}  // namespace Camera
}  // namespace Ti

#endif /* FRAMEDECODER_H_ */
