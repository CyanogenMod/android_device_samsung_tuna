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

#ifndef SWFRAMEDECODER_H_
#define SWFRAMEDECODER_H_

#include "FrameDecoder.h"
#include "Decoder_libjpeg.h"

namespace Ti {
namespace Camera {

class SwFrameDecoder: public FrameDecoder {
public:
    SwFrameDecoder();
    virtual ~SwFrameDecoder();

protected:
    virtual void doConfigure(const DecoderParameters& config);
    virtual void doProcessInputBuffer();
    virtual status_t doStart() { return NO_ERROR; }
    virtual void doStop() { }
    virtual void doFlush() { }
    virtual void doRelease() { }

private:
    int mjpegWithHdrSize;
    Decoder_libjpeg mJpgdecoder;
    unsigned char* mJpegWithHeaderBuffer;
};

}  // namespace Camera
}  // namespace Ti
#endif /* SWFRAMEDECODER_H_ */
