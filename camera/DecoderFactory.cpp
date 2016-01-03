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

#include "FrameDecoder.h"
#include "SwFrameDecoder.h"
#include "OmxFrameDecoder.h"
#include "CameraHal.h"
#include "DecoderFactory.h"

namespace Ti {
namespace Camera {


FrameDecoder* DecoderFactory::createDecoderByType(DecoderType type, bool forceSwDecoder) {
    FrameDecoder* decoder = NULL;
    switch (type) {
        case DecoderType_MJPEG: {

            if (!forceSwDecoder) {
                decoder = new OmxFrameDecoder(DecoderType_MJPEG);
                CAMHAL_LOGD("Using HW Decoder for MJPEG");
            } else {
                decoder = new SwFrameDecoder();
                CAMHAL_LOGD("Using SW Decoder for MJPEG");
            }

            //TODO add logic that handle verification is HW Decoder is available ?
            // And if no - create SW decoder.
            break;
        }
        case DecoderType_H264: {
            decoder = new OmxFrameDecoder(DecoderType_H264);
            CAMHAL_LOGD("Using HW Decoder for H264");
            break;
        }
        default: {
            CAMHAL_LOGE("Unrecognized decoder type %d", type);
        }
    }

    return decoder;
}

}  // namespace Camera
}  // namespace Ti

