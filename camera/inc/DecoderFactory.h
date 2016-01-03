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

#ifndef DECODERFACTORY_H_
#define DECODERFACTORY_H_

#include "FrameDecoder.h"

namespace Ti {
namespace Camera {

class DecoderFactory {
    DecoderFactory();
    ~DecoderFactory();
public:
    static FrameDecoder* createDecoderByType(DecoderType type, bool forceSwDecoder = false);
};

}  // namespace Camera
}  // namespace Ti

#endif /* DECODERFACTORY_H_ */
