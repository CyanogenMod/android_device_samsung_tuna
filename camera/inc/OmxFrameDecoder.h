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

#ifndef OMXFRAMEDECODER_H_
#define OMXFRAMEDECODER_H_


#include <utils/threads.h>
#include <utils/List.h>
#include "FrameDecoder.h"
#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "Decoder_libjpeg.h"

namespace Ti {
namespace Camera {

enum OmxDecoderState {
    OmxDecoderState_Unloaded = 0,
    OmxDecoderState_Loaded,
    OmxDecoderState_Idle,
    OmxDecoderState_Executing,
    OmxDecoderState_Error,
    OmxDecoderState_Invalid,
    OmxDecoderState_Reconfigure,
    OmxDecoderState_Exit
};

enum PortType {
    PortIndexInput  = 0,
    PortIndexOutput = 1
};


struct OmxMessage {
    enum {
        EVENT,
        EMPTY_BUFFER_DONE,
        FILL_BUFFER_DONE,
    }type;

    union {
        // if type == EVENT
        struct {
            OMX_PTR appData;
            OMX_EVENTTYPE event;
            OMX_U32 data1;
            OMX_U32 data2;
            OMX_PTR pEventData;
        } eventData;

        // if type == (EMPTY_BUFFER_DONE || FILL_BUFFER_DONE)
        struct {
            OMX_PTR appData;
            OMX_BUFFERHEADERTYPE* pBuffHead;
        } bufferData;
    } u;
};

class CallbackDispatcher;

struct CallbackDispatcherThread : public android::Thread {
    CallbackDispatcherThread(CallbackDispatcher *dispatcher)
        : mDispatcher(dispatcher) {
    }

private:
    CallbackDispatcher *mDispatcher;

    bool threadLoop();

    CallbackDispatcherThread(const CallbackDispatcherThread &);
    CallbackDispatcherThread &operator=(const CallbackDispatcherThread &);
};

class CallbackDispatcher
{

public:
    CallbackDispatcher();
    ~CallbackDispatcher();

    void post(const OmxMessage &msg);
    bool loop();

private:
    void dispatch(const OmxMessage &msg);

    CallbackDispatcher(const CallbackDispatcher &);
    CallbackDispatcher &operator=(const CallbackDispatcher &);

    android::Mutex mLock;
    android::Condition mQueueChanged;
    android::List<OmxMessage> mQueue;
    android::sp<CallbackDispatcherThread> mThread;
    bool mDone;
};

class OmxFrameDecoder : public FrameDecoder
{

public:
    OmxFrameDecoder(DecoderType type = DecoderType_MJPEG);
    virtual ~OmxFrameDecoder();

    OMX_ERRORTYPE eventHandler(const OMX_EVENTTYPE event, const OMX_U32 data1, const OMX_U32 data2,
                const OMX_PTR pEventData);
    OMX_ERRORTYPE fillBufferDoneHandler(OMX_BUFFERHEADERTYPE* pBuffHead);
    OMX_ERRORTYPE emptyBufferDoneHandler(OMX_BUFFERHEADERTYPE* pBuffHead);

    static OMX_ERRORTYPE eventCallback(const OMX_HANDLETYPE component,
                const OMX_PTR appData, const OMX_EVENTTYPE event, const OMX_U32 data1, const OMX_U32 data2,
                const OMX_PTR pEventData);
    static OMX_ERRORTYPE emptyBufferDoneCallback(OMX_HANDLETYPE hComponent, OMX_PTR appData, OMX_BUFFERHEADERTYPE* pBuffHead);
    static OMX_ERRORTYPE fillBufferDoneCallback(OMX_HANDLETYPE hComponent, OMX_PTR appData, OMX_BUFFERHEADERTYPE* pBuffHead);

    virtual bool getPaddedDimensions(size_t &width, size_t &height);

protected:
    virtual void doConfigure (const DecoderParameters& config);
    virtual void doProcessInputBuffer();
    virtual status_t doStart();
    virtual void doStop();
    virtual void doFlush();
    virtual void doRelease();

private:
    status_t setComponentRole();
    status_t enableGrallockHandles();
    status_t allocateBuffersOutput();
    void freeBuffersOnOutput();
    void freeBuffersOnInput();
    status_t doPortReconfigure();
    void dumpPortSettings(PortType port);
    status_t getAndConfigureDecoder();
    status_t configureJpegPorts(int width, int height);
    status_t switchToIdle();
    status_t allocateBuffersInput();
    status_t disablePortSync(int port);
    status_t enablePortSync(int port);
    void queueOutputBuffers();
    status_t setVideoOutputFormat(OMX_U32 width, OMX_U32 height);


    status_t omxInit();
    status_t omxGetHandle(OMX_HANDLETYPE *handle, OMX_PTR pAppData, OMX_CALLBACKTYPE & callbacks);
    OmxDecoderState getOmxState() { return mCurrentState; }
    status_t commitState(OmxDecoderState state) { mPreviousState = mCurrentState; mCurrentState = state; return NO_ERROR; }
    status_t setVideoPortFormatType(
            OMX_U32 portIndex,
            OMX_VIDEO_CODINGTYPE compressionFormat,
            OMX_COLOR_FORMATTYPE colorFormat);
    status_t omxGetParameter(OMX_INDEXTYPE index, OMX_PTR ptr);
    status_t omxSetParameter(OMX_INDEXTYPE index, OMX_PTR ptr);
    status_t omxSendCommand(OMX_COMMANDTYPE cmd, OMX_S32 param);
    status_t omxGetConfig(OMX_INDEXTYPE index, OMX_PTR ptr);
    status_t omxSetConfig(OMX_INDEXTYPE index, OMX_PTR ptr);
    status_t omxFillThisBuffer(OMX_BUFFERHEADERTYPE *pOutBufHdr);
    status_t omxEmptyThisBuffer(android::sp<MediaBuffer>& inBuffer, OMX_BUFFERHEADERTYPE *pInBufHdr);
    void omxDumpPortSettings(OMX_PARAM_PORTDEFINITIONTYPE& def);
    void omxDumpBufferHeader (OMX_BUFFERHEADERTYPE* bh);
    status_t omxSwitchToExecutingSync();

    bool mOmxInialized;

    OMX_HANDLETYPE mHandleComp;
    OmxDecoderState mCurrentState;
    OmxDecoderState mPreviousState;

    // Condition and Mutex used during OpenMAX state transitions & command completion
    android::Condition mStateCondition;
    android::Mutex mHwLock;

    android::Vector<OMX_BUFFERHEADERTYPE*> mOutBufferHeaders;
    android::Vector<OMX_BUFFERHEADERTYPE*> mInBufferHeaders;

    CallbackDispatcher mDispatcher;

    bool mStopping;
    DecoderType mDecoderType;

    // If true we will search for DHT in JPEG buffer
    bool mIsNeedCheckDHT;
    // If true we always append DHT to JPEG buffer
    bool mAlwaysAppendDHT;
};

} //namespace Camera
} //namespace Ti
#endif /* OMXFRAMEDECODER_H_ */
