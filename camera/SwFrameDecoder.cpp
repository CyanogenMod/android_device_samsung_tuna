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
#include "SwFrameDecoder.h"

namespace Ti {
namespace Camera {

SwFrameDecoder::SwFrameDecoder()
: mjpegWithHdrSize(0), mJpegWithHeaderBuffer(NULL) {
}

SwFrameDecoder::~SwFrameDecoder() {
    delete [] mJpegWithHeaderBuffer;
    mJpegWithHeaderBuffer = NULL;
}


void SwFrameDecoder::doConfigure(const DecoderParameters& params) {
    LOG_FUNCTION_NAME;

    mjpegWithHdrSize = (mParams.width * mParams.height / 2) +
            mJpgdecoder.readDHTSize();
    if (mJpegWithHeaderBuffer != NULL) {
        delete [] mJpegWithHeaderBuffer;
        mJpegWithHeaderBuffer = NULL;
    }
    mJpegWithHeaderBuffer = new unsigned char[mjpegWithHdrSize];

    LOG_FUNCTION_NAME_EXIT;
}


void SwFrameDecoder::doProcessInputBuffer() {
    LOG_FUNCTION_NAME;
    nsecs_t timestamp = 0;

    CAMHAL_LOGV("Will add header to MJPEG");
    int final_jpg_sz = 0;
    {
        int inIndex = mInQueue.itemAt(0);
        android::sp<MediaBuffer>& inBuffer = mInBuffers->editItemAt(inIndex);
        android::AutoMutex lock(inBuffer->getLock());
        timestamp = inBuffer->getTimestamp();
        final_jpg_sz = mJpgdecoder.appendDHT(
                reinterpret_cast<unsigned char*>(inBuffer->buffer),
                inBuffer->filledLen, mJpegWithHeaderBuffer, mjpegWithHdrSize);
        inBuffer->setStatus(BufferStatus_InDecoded);
    }
    CAMHAL_LOGV("Added header to MJPEG");
    {
        int outIndex = mOutQueue.itemAt(0);
        android::sp<MediaBuffer>& outBuffer = mOutBuffers->editItemAt(outIndex);
        android::AutoMutex lock(outBuffer->getLock());
        CameraBuffer* buffer = reinterpret_cast<CameraBuffer*>(outBuffer->buffer);
        if (!mJpgdecoder.decode(mJpegWithHeaderBuffer, final_jpg_sz,
                reinterpret_cast<unsigned char*>(buffer->mapped), 4096)) {
            CAMHAL_LOGEA("Error while decoding JPEG");
            return;
        }
        outBuffer->setTimestamp(timestamp);
        outBuffer->setStatus(BufferStatus_OutFilled);
    }
    CAMHAL_LOGV("JPEG decoded!");

    LOG_FUNCTION_NAME_EXIT;
}


}  // namespace Camera
}  // namespace Ti
