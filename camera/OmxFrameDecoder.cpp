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

#include "ErrorUtils.h"
#include "OmxFrameDecoder.h"
#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Index.h"
#include "Decoder_libjpeg.h"


namespace Ti {
namespace Camera {

const static uint32_t kMaxColorFormatSupported = 1000;
const static int kMaxStateSwitchTimeOut = 1 * 1000 * 1000 * 1000; // 1 sec

static const char* gDecoderRole[2] = {"video_decoder.mjpeg", "video_decoder.avc"};
static const OMX_VIDEO_CODINGTYPE gCompressionFormat[2] = {OMX_VIDEO_CodingMJPEG, OMX_VIDEO_CodingAVC};


template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}



CallbackDispatcher::CallbackDispatcher()
: mDone(false) {
    mThread = new CallbackDispatcherThread(this);
    mThread->run("OMXCallbackDisp", ANDROID_PRIORITY_FOREGROUND);
}

CallbackDispatcher::~CallbackDispatcher() {
    {
        android::Mutex::Autolock autoLock(mLock);

        mDone = true;
        mQueueChanged.signal();
    }

    status_t status = mThread->join();
    if (status != WOULD_BLOCK) {
        //CAMHAL_ASSERT(status, (status_t)NO_ERROR);
    }
}

void CallbackDispatcher::post(const OmxMessage &msg) {
    android::Mutex::Autolock autoLock(mLock);

    mQueue.push_back(msg);
    mQueueChanged.signal();
}

void CallbackDispatcher::dispatch(const OmxMessage &msg) {

    switch(msg.type)
    {
        case OmxMessage::EVENT :
        {
            static_cast<OmxFrameDecoder*>(msg.u.eventData.appData)->eventHandler(msg.u.eventData.event, msg.u.eventData.data1, msg.u.eventData.data2, msg.u.eventData.pEventData);
            break;
        }

        case OmxMessage::EMPTY_BUFFER_DONE:
        {
            static_cast<OmxFrameDecoder*>(msg.u.bufferData.appData)->emptyBufferDoneHandler(msg.u.bufferData.pBuffHead);
            break;
        }

        case OmxMessage::FILL_BUFFER_DONE:
        {
            static_cast<OmxFrameDecoder*>(msg.u.bufferData.appData)->fillBufferDoneHandler(msg.u.bufferData.pBuffHead);
            break;
        }
    };
}

bool CallbackDispatcher::loop() {
    for (;;) {
        OmxMessage msg;

        {
            android::Mutex::Autolock autoLock(mLock);
            while (!mDone && mQueue.empty()) {
                mQueueChanged.wait(mLock);
            }

            if (mDone) {
                break;
            }

            msg = *mQueue.begin();
            mQueue.erase(mQueue.begin());
        }

        dispatch(msg);
    }

    return false;
}

bool CallbackDispatcherThread::threadLoop() {
    return mDispatcher->loop();
}

//Static
OMX_ERRORTYPE OmxFrameDecoder::eventCallback(const OMX_HANDLETYPE component,
        const OMX_PTR appData, const OMX_EVENTTYPE event, const OMX_U32 data1, const OMX_U32 data2,
        const OMX_PTR pEventData) {
    OmxMessage msg;
    msg.type = OmxMessage::EVENT;
    msg.u.eventData.appData = appData;
    msg.u.eventData.event = event;
    msg.u.eventData.data1 = data1;
    msg.u.eventData.data2 = data2;
    ((OmxFrameDecoder *)appData)->mDispatcher.post(msg);
    return OMX_ErrorNone;
}

//Static
OMX_ERRORTYPE OmxFrameDecoder::emptyBufferDoneCallback(OMX_HANDLETYPE hComponent,
        OMX_PTR appData, OMX_BUFFERHEADERTYPE* pBuffHead) {
    OmxMessage msg;
    msg.type = OmxMessage::EMPTY_BUFFER_DONE;
    msg.u.bufferData.appData = appData;
    msg.u.bufferData.pBuffHead = pBuffHead;
    ((OmxFrameDecoder *)appData)->mDispatcher.post(msg);
    return OMX_ErrorNone;
}

//Static
OMX_ERRORTYPE OmxFrameDecoder::fillBufferDoneCallback(OMX_HANDLETYPE hComponent,
        OMX_PTR appData, OMX_BUFFERHEADERTYPE* pBuffHead) {
    OmxMessage msg;
    msg.type = OmxMessage::FILL_BUFFER_DONE;
    msg.u.bufferData.appData = appData;
    msg.u.bufferData.pBuffHead = pBuffHead;
    ((OmxFrameDecoder *)appData)->mDispatcher.post(msg);
    return OMX_ErrorNone;
}

OmxFrameDecoder::OmxFrameDecoder(DecoderType type)
    : mOmxInialized(false), mCurrentState(OmxDecoderState_Unloaded), mPreviousState(OmxDecoderState_Unloaded),
    mStopping(false), mDecoderType(type), mIsNeedCheckDHT(true), mAlwaysAppendDHT(false) {
}

OmxFrameDecoder::~OmxFrameDecoder() {
}

OMX_ERRORTYPE OmxFrameDecoder::emptyBufferDoneHandler(OMX_BUFFERHEADERTYPE* pBuffHead) {
    LOG_FUNCTION_NAME;
    android::AutoMutex lock(mHwLock);

    int bufferIndex = reinterpret_cast<int>(pBuffHead->pAppPrivate);
    CAMHAL_LOGD("Got header %p id = %d", pBuffHead, bufferIndex);
    android::sp<MediaBuffer>& in = mInBuffers->editItemAt(bufferIndex);

    android::AutoMutex itemLock(in->getLock());
    in->setStatus((getOmxState() == OmxDecoderState_Executing) ? BufferStatus_InDecoded : BufferStatus_InQueued);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxFrameDecoder::fillBufferDoneHandler(OMX_BUFFERHEADERTYPE* pBuffHead) {
    LOG_FUNCTION_NAME;
    android::AutoMutex lock(mHwLock);

    int index = (int)pBuffHead->pAppPrivate;
    android::sp<MediaBuffer>& out = mOutBuffers->editItemAt(index);

    android::AutoMutex itemLock(out->getLock());
    CameraBuffer* frame = static_cast<CameraBuffer*>(out->buffer);
    out->setOffset(pBuffHead->nOffset);
    out->setTimestamp(pBuffHead->nTimeStamp);
    out->setStatus((getOmxState() == OmxDecoderState_Executing) ? BufferStatus_OutFilled : BufferStatus_OutQueued);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxFrameDecoder::eventHandler(const OMX_EVENTTYPE event, const OMX_U32 data1, const OMX_U32 data2,
            const OMX_PTR pEventData) {

    LOG_FUNCTION_NAME;

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    android::AutoMutex lock(mHwLock);

    switch(event) {

        case OMX_EventCmdComplete:
        {
            if ((data1 == OMX_CommandStateSet) && (data2 == OMX_StateIdle)) {
                CAMHAL_LOGD("Component State Changed To OMX_StateIdle\n");
                commitState(OmxDecoderState_Idle);
                mStateCondition.signal();
            }
            else if ((data1 == OMX_CommandStateSet) && (data2 == OMX_StateExecuting)) {
                CAMHAL_LOGD("Component State Changed To OMX_StateExecuting\n");
                commitState(OmxDecoderState_Executing);
                mStateCondition.signal();
            }
            else if ((data1 == OMX_CommandStateSet) && (data2 == OMX_StateLoaded)) {
                CAMHAL_LOGD("Component State Changed To OMX_StateLoaded\n");
                if(getOmxState() == OmxDecoderState_Executing)
                    commitState(OmxDecoderState_Loaded);
                mStateCondition.signal();
            }
            else if (data1 == OMX_CommandFlush) {
                CAMHAL_LOGD("OMX_CommandFlush done on %d port\n", data2);
                mStateCondition.signal();
            }
            else if (data1 == OMX_CommandPortDisable) {
                CAMHAL_LOGD("OMX_CommandPortDisable done on %d port\n", data2);
                mStateCondition.signal();
            }
            else if (data1 == OMX_CommandPortEnable) {
                CAMHAL_LOGD("OMX_CommandPortEnable done on %d port\n", data2);
                mStateCondition.signal();
            } else {
                CAMHAL_LOGD("Event %d done on %d port\n", data1, data2);
            }
            break;
        }
        case OMX_EventError:
        {
            CAMHAL_LOGD("\n\n\nOMX Component  reported an Error!!!! 0x%x 0x%x\n\n\n", data1, data2);
            commitState(OmxDecoderState_Error);
            omxSendCommand(OMX_CommandStateSet, OMX_StateInvalid);
            mStateCondition.signal();
            break;
        }
        case OMX_EventPortSettingsChanged:
        {
            CAMHAL_LOGD("\n\n\nOMX_EventPortSettingsChanged(port=%ld, data2=0x%08lx)\n\n\n",
                                                              data1, data2);
            if (data2 == 0) {
                // This means that some serious change to port happens
                commitState(OmxDecoderState_Reconfigure);
            } else if (data2 == OMX_IndexConfigCommonOutputCrop) {
#if 0
                OMX_CONFIG_RECTTYPE rect;
                InitOMXParams(&rect);
                rect.nPortIndex = PortIndexOutput;
                status_t ret = omxGetConfig(OMX_IndexConfigCommonOutputCrop, &rect);
                if (ret != NO_ERROR) {
                    CAMHAL_LOGE("Can't get new crop parameters 0x%x", ret);
                    break;
                }

                CAMHAL_LOGV("Crop should change to %d %d %d %d", rect.nLeft, rect.nTop, rect.nLeft + rect.nWidth, rect.nTop + rect.nHeight);
#endif
            }
            break;
        }
        default:
        {
            CAMHAL_LOGD("\n\n\nOMX Unhandelled event ID=0x%x!!!!\n\n\n", event);
        }
    }

    LOG_FUNCTION_NAME_EXIT;

    return ret;
    }

void OmxFrameDecoder::doConfigure(const DecoderParameters& config) {
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;
}

status_t OmxFrameDecoder::enableGrallockHandles() {
    OMX_TI_PARAMUSENATIVEBUFFER domxUseGrallocHandles;
    InitOMXParams(&domxUseGrallocHandles);

    domxUseGrallocHandles.nPortIndex = PortIndexOutput;
    domxUseGrallocHandles.bEnable = OMX_TRUE;

    return omxSetParameter((OMX_INDEXTYPE)OMX_TI_IndexUseNativeBuffers, &domxUseGrallocHandles);
}

status_t OmxFrameDecoder::omxSwitchToExecutingSync() {
    CAMHAL_LOGV("Try set OMX_StateExecuting");
    android::AutoMutex lock(mHwLock);
    omxSendCommand(OMX_CommandStateSet, OMX_StateExecuting);
    status_t ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to EXECUTING ERROR 0x%x", ret);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

void OmxFrameDecoder::dumpPortSettings(PortType port) {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = port;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);
    omxDumpPortSettings(def);
}

status_t OmxFrameDecoder::disablePortSync(int port) {
    OMX_ERRORTYPE eError;
    android::AutoMutex lock(mHwLock);
    eError = OMX_SendCommand(mHandleComp, OMX_CommandPortDisable, port, NULL);
    if (eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_CommandPortDisable OMX_ALL returned error 0x%x", eError);
        return Utils::ErrorUtils::omxToAndroidError(eError);
    }
    status_t ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to OMX_StateLoaded ERROR 0x%x", ret);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t OmxFrameDecoder::enablePortSync(int port) {
    android::AutoMutex lock(mHwLock);
    OMX_ERRORTYPE eError = OMX_SendCommand(mHandleComp, OMX_CommandPortEnable, port, NULL);
    status_t ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_SendCommand OMX_CommandPortEnable OUT returned error 0x%x", eError);
        return Utils::ErrorUtils::omxToAndroidError(eError);
    }
    return NO_ERROR;
}


status_t OmxFrameDecoder::doPortReconfigure() {
    OMX_ERRORTYPE eError;
    status_t ret = NO_ERROR;

    CAMHAL_LOGD("Starting port reconfiguration !");
    dumpPortSettings(PortIndexInput);
    dumpPortSettings(PortIndexOutput);

    android::AutoMutex lock(mHwLock);

    omxSendCommand(OMX_CommandFlush, PortIndexOutput);
    ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to OMX_CommandFlush ERROR 0x%x", ret);
        return UNKNOWN_ERROR;
    }

    omxSendCommand(OMX_CommandFlush, PortIndexInput);
    ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to OMX_CommandFlush ERROR 0x%x", ret);
        return UNKNOWN_ERROR;
    }

    ret = omxSendCommand(OMX_CommandPortDisable, PortIndexOutput);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("OMX_CommandPortDisable PortIndexOutput returned error 0x%x", ret);
        return ret;
    }

    freeBuffersOnOutput();

    ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to OMX_StateLoaded ERROR 0x%x", ret);
        return UNKNOWN_ERROR;
    }

    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = PortIndexOutput;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);
    def.nBufferCountActual = mParams.outputBufferCount;
    CAMHAL_LOGD("Will set def.nBufferSize=%d stride=%d height=%d", def.nBufferSize , def.format.video.nStride, def.format.video.nFrameHeight);
    omxSetParameter(OMX_IndexParamPortDefinition, &def);



    ret = omxSendCommand(OMX_CommandPortEnable, PortIndexOutput);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("omxSendCommand OMX_CommandPortEnable returned error 0x%x", ret);
        return ret;
    }

    allocateBuffersOutput();

    ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("omxSendCommand OMX_CommandPortEnable timeout 0x%x", ret);
        return UNKNOWN_ERROR;
    }

    CAMHAL_LOGD("Port reconfiguration DONE!");
    //dumpPortSettings(PortIndexOutput);

    return NO_ERROR;
}

void OmxFrameDecoder::queueOutputBuffers() {

    LOG_FUNCTION_NAME;

    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();

    for (size_t i = 0; i < mOutQueue.size(); i++) {
        int index = mOutQueue[i];
        android::sp<MediaBuffer> &outBuffer = mOutBuffers->editItemAt(index);
        android::AutoMutex bufferLock(outBuffer->getLock());
        if (outBuffer->getStatus() == BufferStatus_OutQueued) {
            outBuffer->setStatus(BufferStatus_OutWaitForFill);
            CameraBuffer* frame = static_cast<CameraBuffer*>(outBuffer->buffer);
            OMX_BUFFERHEADERTYPE *pOutBufHdr = mOutBufferHeaders[outBuffer->bufferId];
            CAMHAL_LOGV("Fill this buffer cf=%p bh=%p id=%d", frame, pOutBufHdr, outBuffer->bufferId);
            status_t status = omxFillThisBuffer(pOutBufHdr);
            CAMHAL_ASSERT(status == NO_ERROR);
        }
    }

    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::doProcessInputBuffer() {

    LOG_FUNCTION_NAME;

    if (getOmxState() == OmxDecoderState_Reconfigure) {
        if (doPortReconfigure() == NO_ERROR) {
            commitState(OmxDecoderState_Executing);
            queueOutputBuffers();
        } else {
            commitState(OmxDecoderState_Error);
            return;
        }

    }

    if (getOmxState() == OmxDecoderState_Idle) {
        CAMHAL_ASSERT(omxSwitchToExecutingSync() == NO_ERROR);
        queueOutputBuffers();
    }

    if (getOmxState() == OmxDecoderState_Executing) {
        for (size_t i = 0; i < mInQueue.size(); i++) {
            int index = mInQueue[i];
            CAMHAL_LOGD("Got in inqueue[%d] buffer id=%d", i, index);
            android::sp<MediaBuffer> &inBuffer = mInBuffers->editItemAt(index);
            android::AutoMutex bufferLock(inBuffer->getLock());
            if (inBuffer->getStatus() == BufferStatus_InQueued) {
                OMX_BUFFERHEADERTYPE *pInBufHdr = mInBufferHeaders[index];
                inBuffer->setStatus(BufferStatus_InWaitForEmpty);
                omxEmptyThisBuffer(inBuffer, pInBufHdr);
            }
        }
        queueOutputBuffers();
    }

    LOG_FUNCTION_NAME_EXIT;
}

status_t OmxFrameDecoder::omxInit() {

    LOG_FUNCTION_NAME;

    OMX_ERRORTYPE eError = OMX_Init();
    if (eError != OMX_ErrorNone) {
        CAMHAL_LOGEB("OMX_Init() failed, error: 0x%x", eError);
    }
    else mOmxInialized = true;

    LOG_FUNCTION_NAME_EXIT;
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxFillThisBuffer(OMX_BUFFERHEADERTYPE *pOutBufHdr) {
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;

    pOutBufHdr->nFilledLen = 0;
    pOutBufHdr->nOffset = 0;
    pOutBufHdr->nFlags = 0;

    eError = OMX_FillThisBuffer(mHandleComp, pOutBufHdr);
    if (eError != OMX_ErrorNone) {
         CAMHAL_LOGE("OMX_FillThisBuffer ERROR 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}


status_t OmxFrameDecoder::omxGetHandle(OMX_HANDLETYPE *handle, OMX_PTR pAppData,
        OMX_CALLBACKTYPE & callbacks) {
    LOG_FUNCTION_NAME;

    OMX_ERRORTYPE eError = OMX_ErrorUndefined;

    eError = OMX_GetHandle(handle, (OMX_STRING)"OMX.TI.DUCATI1.VIDEO.DECODER", pAppData, &callbacks);
    if((eError != OMX_ErrorNone) ||  (handle == NULL)) {
        handle = NULL;
        return Utils::ErrorUtils::omxToAndroidError(eError);
    }
    commitState(OmxDecoderState_Loaded);

    LOG_FUNCTION_NAME_EXIT;

    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxEmptyThisBuffer(android::sp<MediaBuffer>& inBuffer, OMX_BUFFERHEADERTYPE *pInBufHdr) {

    LOG_FUNCTION_NAME;

    OMX_PARAM_PORTDEFINITIONTYPE def;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    InitOMXParams(&def);
    def.nPortIndex = PortIndexInput;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);
    CAMHAL_LOGD("Founded id for empty is %d ", inBuffer->bufferId);
    if (inBuffer->filledLen > def.nBufferSize) {
        CAMHAL_LOGE("Can't copy IN buffer due to it too small %d than needed %d", def.nBufferSize, inBuffer->filledLen);
        return UNKNOWN_ERROR;
    }

    int filledLen = inBuffer->filledLen;
    unsigned char* dataBuffer = reinterpret_cast<unsigned char*>(inBuffer->buffer);

    //If decoder type MJPEG we check if append DHT forced and if true append it
    //in other case we check mIsNeedCheckDHT and if true search for DHT in buffer
    //if we don't found it - will do append
    //once we find that buffer not contain DHT we will append it each time
    if ((mDecoderType == DecoderType_MJPEG) && ((mAlwaysAppendDHT) || ((mIsNeedCheckDHT) &&
            (mIsNeedCheckDHT = !Decoder_libjpeg::isDhtExist(dataBuffer, filledLen))))) {
        CAMHAL_LOGV("Will append DHT to buffer");
        Decoder_libjpeg::appendDHT(dataBuffer, filledLen, pInBufHdr->pBuffer, filledLen + Decoder_libjpeg::readDHTSize());
        filledLen += Decoder_libjpeg::readDHTSize();
        mIsNeedCheckDHT = false;
        mAlwaysAppendDHT = true;
    } else {
        memcpy(pInBufHdr->pBuffer, dataBuffer, filledLen);
    }

    CAMHAL_LOGV("Copied %d bytes into In buffer with bh=%p", filledLen, pInBufHdr);
    CAMHAL_LOGV("Empty this buffer id=%d timestamp %lld offset=%d", inBuffer->bufferId, pInBufHdr->nTimeStamp, pInBufHdr->nOffset);
    pInBufHdr->nFilledLen = filledLen;
    pInBufHdr->nTimeStamp = inBuffer->getTimestamp();
    pInBufHdr->nFlags = 16;
    pInBufHdr->nOffset = 0;
    eError = OMX_EmptyThisBuffer(mHandleComp, pInBufHdr);
    if (eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_EmptyThisBuffer ERROR 0x%x", eError);
        Utils::ErrorUtils::omxToAndroidError(eError);
    }

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;
}


status_t OmxFrameDecoder::allocateBuffersOutput() {
    LOG_FUNCTION_NAME;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = PortIndexOutput;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);
    def.nBufferCountActual = mParams.outputBufferCount;

    CAMHAL_LOGD("Will set def.nBufferSize=%d stride=%d height=%d", def.nBufferSize , def.format.video.nStride, def.format.video.nFrameHeight);

    OMX_BUFFERHEADERTYPE *pOutBufHdr;
    mOutBufferHeaders.clear();
    for (size_t i = 0; i < mOutBuffers->size(); i++) {
        android::sp<MediaBuffer>& outBuffer = mOutBuffers->editItemAt(i);
        android::AutoMutex lock(outBuffer->getLock());
        CameraBuffer* cb = static_cast<CameraBuffer*>(outBuffer->buffer);
        OMX_U8 * outPtr = static_cast<OMX_U8*>(camera_buffer_get_omx_ptr(cb));
        CAMHAL_LOGV("Try to set OMX_UseBuffer [0x%x] for output port with length %d ", outPtr, def.nBufferSize);
        eError = OMX_UseBuffer(mHandleComp, &pOutBufHdr,  PortIndexOutput, (void*)i, def.nBufferSize, outPtr);

        if (eError != OMX_ErrorNone) {
            ALOGE("OMX_UseBuffer failed with error %d (0x%08x)", eError, eError);
            commitState(OmxDecoderState_Error);
            return UNKNOWN_ERROR;
        }

        CAMHAL_LOGD("Got buffer header %p", pOutBufHdr);
        mOutBufferHeaders.add(pOutBufHdr);
    }

    omxDumpPortSettings(def);
    LOG_FUNCTION_NAME_EXIT;
    return NO_ERROR;

}

status_t OmxFrameDecoder::allocateBuffersInput() {
    LOG_FUNCTION_NAME;

    OMX_PARAM_PORTDEFINITIONTYPE def;
    OMX_BUFFERHEADERTYPE *pInBufHdr;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    InitOMXParams(&def);
    def.nPortIndex = PortIndexInput;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);

    // TODO: Will be changed since port reconfiguration will be handled
    def.nBufferCountActual = mInBuffers->size();
    def.bEnabled = OMX_TRUE;
    omxSetParameter(OMX_IndexParamPortDefinition, &def);

    mInBufferHeaders.clear();

    for (size_t i = 0; i < mInBuffers->size(); i++) {
        CAMHAL_LOGD("Will do OMX_AllocateBuffer for input port with size %d id=%d", def.nBufferSize, i);
        eError = OMX_AllocateBuffer(mHandleComp, &pInBufHdr,  PortIndexInput, (void*)i, def.nBufferSize);
        if (eError != OMX_ErrorNone) {
            ALOGE("OMX_AllocateBuffer failed with error %d (0x%08x)", eError, eError);
            commitState(OmxDecoderState_Error);
            return UNKNOWN_ERROR;
        }
        CAMHAL_LOGD("Got new buffer header [%p] for IN port", pInBufHdr);
        mInBufferHeaders.push_back(pInBufHdr);
    }

    LOG_FUNCTION_NAME_EXIT;
    return NO_ERROR;
}

status_t OmxFrameDecoder::getAndConfigureDecoder() {
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError;

    ret = omxInit();
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("OMX_Init returned error 0x%x", ret);
        return ret;
    }
    OMX_CALLBACKTYPE callbacks;
    callbacks.EventHandler = OmxFrameDecoder::eventCallback;
    callbacks.EmptyBufferDone = OmxFrameDecoder::emptyBufferDoneCallback;
    callbacks.FillBufferDone = OmxFrameDecoder::fillBufferDoneCallback;
    ret = omxGetHandle(&mHandleComp, this, callbacks);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("OMX_GetHandle returned error 0x%x", ret);
        OMX_Deinit();
        mOmxInialized = false;
        return ret;
    }
    ret = setComponentRole();
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("setComponentRole returned error 0x%x", ret);
        OMX_Deinit();
        mOmxInialized = false;
        return ret;
    }
    disablePortSync(PortIndexOutput);
    ret = setVideoOutputFormat(mParams.width, mParams.height);
    enablePortSync(PortIndexOutput);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("Can't set output format error 0x%x", ret);
        OMX_Deinit();
        mOmxInialized = false;
        return ret;
    }
    enableGrallockHandles();
    return NO_ERROR;
}

status_t OmxFrameDecoder::switchToIdle() {
    CAMHAL_ASSERT(getOmxState() == OmxDecoderState_Loaded);
    CAMHAL_LOGD("Try set OMX_StateIdle");
    android::AutoMutex lock(mHwLock);
    status_t ret = omxSendCommand(OMX_CommandStateSet, OMX_StateIdle);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("Can't omxSendCommandt error 0x%x", ret);
        OMX_Deinit();
        mOmxInialized = false;
        return ret;
    }

    allocateBuffersInput();

    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = PortIndexOutput;
    omxGetParameter(OMX_IndexParamPortDefinition, &def);
    def.nBufferCountActual = mParams.outputBufferCount;
    omxSetParameter(OMX_IndexParamPortDefinition, &def);

    allocateBuffersOutput();

    ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
    if (ret != NO_ERROR) {
        CAMHAL_LOGE("State transition to IDLE ERROR 0x%x", ret);
        return ret;
    }
    commitState(OmxDecoderState_Idle);
    return NO_ERROR;
}

status_t OmxFrameDecoder::doStart() {
    LOG_FUNCTION_NAME;

    status_t ret = NO_ERROR;
    mStopping = false;
    OMX_ERRORTYPE eError;

    ret = getAndConfigureDecoder();

#if 0
    OMX_TI_PARAM_ENHANCEDPORTRECONFIG tParamStruct;
    tParamStruct.nSize = sizeof(OMX_TI_PARAM_ENHANCEDPORTRECONFIG);
    tParamStruct.nVersion.s.nVersionMajor = 0x1;
    tParamStruct.nVersion.s.nVersionMinor = 0x1;
    tParamStruct.nVersion.s.nRevision = 0x0;
    tParamStruct.nVersion.s.nStep = 0x0;
    tParamStruct.nPortIndex = PortIndexOutput;
    tParamStruct.bUsePortReconfigForCrop = OMX_TRUE;
    tParamStruct.bUsePortReconfigForPadding = OMX_FALSE;
    omxSetParameter((OMX_INDEXTYPE)OMX_TI_IndexParamUseEnhancedPortReconfig, &tParamStruct);
#endif

    // Transition to IDLE
    ret = switchToIdle();
    dumpPortSettings(PortIndexInput);
    dumpPortSettings(PortIndexOutput);

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

status_t OmxFrameDecoder::omxGetParameter(OMX_INDEXTYPE index, OMX_PTR ptr) {
    OMX_ERRORTYPE  eError = OMX_GetParameter(mHandleComp, index, ptr);
    if(eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_GetParameter - error 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxGetConfig(OMX_INDEXTYPE index, OMX_PTR ptr) {
    OMX_ERRORTYPE  eError = OMX_GetConfig(mHandleComp, index, ptr);
    if(eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_GetConfig - error 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxSetParameter(OMX_INDEXTYPE index, OMX_PTR ptr) {
    OMX_ERRORTYPE  eError = OMX_SetParameter(mHandleComp, index, ptr);
    if(eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_SetParameter - error 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxSetConfig(OMX_INDEXTYPE index, OMX_PTR ptr) {
    OMX_ERRORTYPE  eError = OMX_SetConfig(mHandleComp, index, ptr);
    if(eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_SetConfig - error 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::omxSendCommand(OMX_COMMANDTYPE cmd, OMX_S32 param) {
    OMX_ERRORTYPE  eError = OMX_SendCommand(mHandleComp, cmd, param, NULL);
    if(eError != OMX_ErrorNone) {
        CAMHAL_LOGE("OMX_SendCommand - error 0x%x", eError);
    }
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::setVideoOutputFormat(OMX_U32 width, OMX_U32 height) {
    LOG_FUNCTION_NAME;

    CAMHAL_LOGV("setVideoOutputFormat width=%ld, height=%ld", width, height);

    OMX_VIDEO_CODINGTYPE compressionFormat = gCompressionFormat[mDecoderType];

    status_t err = setVideoPortFormatType(
            PortIndexInput, compressionFormat, OMX_COLOR_FormatUnused);

    if (err != NO_ERROR) {
        CAMHAL_LOGE("Error during setVideoPortFormatType 0x%x", err);
        return err;
    }

    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = PortIndexInput;

    OMX_VIDEO_PORTDEFINITIONTYPE *video_def = &def.format.video;

    err = omxGetParameter(OMX_IndexParamPortDefinition, &def);

    if (err != NO_ERROR) {
        return err;
    }

    video_def->nFrameWidth = width;
    video_def->nFrameHeight = height;

    video_def->eCompressionFormat = compressionFormat;
    video_def->eColorFormat = OMX_COLOR_FormatUnused;


    err = omxSetParameter(OMX_IndexParamPortDefinition, &def);


    if (err != OK) {
        return err;
    }

    OMX_PARAM_PORTDEFINITIONTYPE odef;
    OMX_VIDEO_PORTDEFINITIONTYPE *out_video_def = &odef.format.video;

    InitOMXParams(&odef);
    odef.nPortIndex = PortIndexOutput;

    err = omxGetParameter(OMX_IndexParamPortDefinition, &odef);
    if (err != NO_ERROR) {
        return err;
    }

    out_video_def->nFrameWidth = width;
    out_video_def->nFrameHeight = height;
    out_video_def->xFramerate = 30<< 16;//((width >= 720) ? 60 : 30) << 16;
    out_video_def->nStride = 4096;

    err = omxSetParameter(OMX_IndexParamPortDefinition, &odef);
    CAMHAL_LOGD("OUT port is configured");
    dumpPortSettings(PortIndexOutput);

    LOG_FUNCTION_NAME_EXIT;
    return err;
}

status_t OmxFrameDecoder::setVideoPortFormatType(
        OMX_U32 portIndex,
        OMX_VIDEO_CODINGTYPE compressionFormat,
        OMX_COLOR_FORMATTYPE colorFormat) {

    LOG_FUNCTION_NAME;

    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    InitOMXParams(&format);
    format.nPortIndex = portIndex;
    format.nIndex = 0;
    bool found = false;

    OMX_U32 index = 0;
    for (;;) {
        CAMHAL_LOGV("Will check index = %d", index);
        format.nIndex = index;
        OMX_ERRORTYPE  eError = OMX_GetParameter(
                mHandleComp, OMX_IndexParamVideoPortFormat,
                &format);

        CAMHAL_LOGV("format.eCompressionFormat=0x%x format.eColorFormat=0x%x", format.eCompressionFormat, format.eColorFormat);

        if (format.eCompressionFormat == compressionFormat
                && format.eColorFormat == colorFormat) {
            found = true;
            break;
        }

        ++index;
        if (index >= kMaxColorFormatSupported) {
            CAMHAL_LOGE("color format %d or compression format %d is not supported",
                colorFormat, compressionFormat);
            return UNKNOWN_ERROR;
        }
    }

    if (!found) {
        return UNKNOWN_ERROR;
    }

    CAMHAL_LOGV("found a match.");
    OMX_ERRORTYPE  eError = OMX_SetParameter(
            mHandleComp, OMX_IndexParamVideoPortFormat,
            &format);

    LOG_FUNCTION_NAME_EXIT;
    return Utils::ErrorUtils::omxToAndroidError(eError);
}

status_t OmxFrameDecoder::setComponentRole() {
    OMX_PARAM_COMPONENTROLETYPE roleParams;
    const char *role = gDecoderRole[mDecoderType];
    InitOMXParams(&roleParams);

    strncpy((char *)roleParams.cRole,
                    role, OMX_MAX_STRINGNAME_SIZE - 1);
    roleParams.cRole[OMX_MAX_STRINGNAME_SIZE - 1] = '\0';

    return omxSetParameter(OMX_IndexParamStandardComponentRole, &roleParams);
}

void OmxFrameDecoder::freeBuffersOnOutput() {
    LOG_FUNCTION_NAME;
    for (size_t i = 0; i < mOutBufferHeaders.size(); i++) {
        OMX_BUFFERHEADERTYPE* header = mOutBufferHeaders[i];
        CAMHAL_LOGD("Freeing OUT buffer header %p", header);
        OMX_FreeBuffer(mHandleComp, PortIndexOutput, header);
    }
    mOutBufferHeaders.clear();
    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::freeBuffersOnInput() {
    LOG_FUNCTION_NAME;
    for (size_t i = 0; i < mInBufferHeaders.size(); i++) {
        OMX_BUFFERHEADERTYPE* header = mInBufferHeaders[i];
        CAMHAL_LOGD("Freeing IN buffer header %p", header);
        OMX_FreeBuffer(mHandleComp, PortIndexInput, header);
    }
    mInBufferHeaders.clear();
    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::doStop() {
    LOG_FUNCTION_NAME;

    mStopping = true;
    android::AutoMutex lock(mHwLock);

    CAMHAL_LOGD("HwFrameDecoder::doStop state id=%d", getOmxState());

    if ((getOmxState() == OmxDecoderState_Executing) || (getOmxState() == OmxDecoderState_Reconfigure)) {

        CAMHAL_LOGD("Try set OMX_StateIdle");
        status_t ret = omxSendCommand(OMX_CommandStateSet, OMX_StateIdle);
        if (ret != NO_ERROR) {
            CAMHAL_LOGE("Can't omxSendCommandt error 0x%x", ret);
        }

        ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
        if (ret != NO_ERROR) {
            CAMHAL_LOGE("State transition to IDLE ERROR 0x%x", ret);
        }
        commitState(OmxDecoderState_Idle);
    }

    if (getOmxState() == OmxDecoderState_Idle) {

       CAMHAL_LOGD("Try set OMX_StateLoaded");
       status_t ret = omxSendCommand(OMX_CommandStateSet, OMX_StateLoaded);
       if (ret != NO_ERROR) {
           CAMHAL_LOGE("Can't omxSendCommandt error 0x%x", ret);
           return;
       }
       freeBuffersOnOutput();
       freeBuffersOnInput();
       ret = mStateCondition.waitRelative(mHwLock, kMaxStateSwitchTimeOut);
       if (ret != NO_ERROR) {
           CAMHAL_LOGE("State transition to OMX_StateLoaded ERROR 0x%x", ret);
       }
       commitState(OmxDecoderState_Loaded);

    }

    if (getOmxState() == OmxDecoderState_Error) {
        CAMHAL_LOGD("In state ERROR will try to free buffers!");
        freeBuffersOnOutput();
        freeBuffersOnInput();
    }

    CAMHAL_LOGD("Before OMX_FreeHandle ....");
    OMX_FreeHandle(mHandleComp);
    CAMHAL_LOGD("After OMX_FreeHandle ....");

    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::doFlush() {
    LOG_FUNCTION_NAME;
    mIsNeedCheckDHT = true;
    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::doRelease() {
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;
}

void OmxFrameDecoder::omxDumpPortSettings(OMX_PARAM_PORTDEFINITIONTYPE& def) {
    CAMHAL_LOGD("----------Port settings start--------------------");
    CAMHAL_LOGD("nSize=%d nPortIndex=%d eDir=%d nBufferCountActual=%d", def.nSize, def.nPortIndex, def.eDir, def.nBufferCountActual);
    CAMHAL_LOGD("nBufferCountMin=%d nBufferSize=%d bEnabled=%d bPopulated=%d bBuffersContiguous=%d nBufferAlignment=%d", def.nBufferCountMin, def.nBufferSize, def.bEnabled, def.bPopulated, def.bBuffersContiguous, def.nBufferAlignment);

    CAMHAL_LOGD("eDomain = %d",def.eDomain);

    if (def.eDomain == OMX_PortDomainVideo) {
        CAMHAL_LOGD("===============Video Port===================");
        CAMHAL_LOGD("cMIMEType=%s",def.format.video.cMIMEType);
        CAMHAL_LOGD("nFrameWidth=%d nFrameHeight=%d", def.format.video.nFrameWidth, def.format.video.nFrameHeight);
        CAMHAL_LOGD("nStride=%d nSliceHeight=%d", def.format.video.nStride, def.format.video.nSliceHeight);
        CAMHAL_LOGD("nBitrate=%d xFramerate=%d", def.format.video.nBitrate, def.format.video.xFramerate>>16);
        CAMHAL_LOGD("bFlagErrorConcealment=%d eCompressionFormat=%d", def.format.video.bFlagErrorConcealment, def.format.video.eCompressionFormat);
        CAMHAL_LOGD("eColorFormat=0x%x pNativeWindow=%p", def.format.video.eColorFormat, def.format.video.pNativeWindow);
        CAMHAL_LOGD("===============END Video Part===================");
    }
    else if (def.eDomain == OMX_PortDomainImage) {
        CAMHAL_LOGD("===============Image Port===================");
        CAMHAL_LOGD("cMIMEType=%s",def.format.image.cMIMEType);
        CAMHAL_LOGD("nFrameWidth=%d nFrameHeight=%d", def.format.image.nFrameWidth, def.format.image.nFrameHeight);
        CAMHAL_LOGD("nStride=%d nSliceHeight=%d", def.format.image.nStride, def.format.image.nSliceHeight);
        CAMHAL_LOGD("bFlagErrorConcealment=%d eCompressionFormat=%d", def.format.image.bFlagErrorConcealment, def.format.image.eCompressionFormat);
        CAMHAL_LOGD("eColorFormat=0x%x pNativeWindow=%p", def.format.image.eColorFormat, def.format.image.pNativeWindow);
        CAMHAL_LOGD("===============END Image Part===================");
    }
    CAMHAL_LOGD("----------Port settings end--------------------");
}

void OmxFrameDecoder::omxDumpBufferHeader(OMX_BUFFERHEADERTYPE* bh) {
    CAMHAL_LOGD("==============OMX_BUFFERHEADERTYPE start==============");
    CAMHAL_LOGD("nAllocLen=%d nFilledLen=%d nOffset=%d nFlags=0x%x", bh->nAllocLen, bh->nFilledLen, bh->nOffset, bh->nFlags);
    CAMHAL_LOGD("pBuffer=%p nOutputPortIndex=%d nInputPortIndex=%d nSize=0x%x", bh->pBuffer, bh->nOutputPortIndex, bh->nInputPortIndex, bh->nSize);
    CAMHAL_LOGD("nVersion=0x%x", bh->nVersion);
    CAMHAL_LOGD("==============OMX_BUFFERHEADERTYPE end==============");
}

bool OmxFrameDecoder::getPaddedDimensions(size_t &width, size_t &height) {

    switch (height) {

        case 480: {
            height = 576;
            if (width == 640) {
                width = 768;
            }
            break;
        }
        case 720: {
            height = 832;
            if (width == 1280) {
                width = 1408;
            }
            break;
        }
        case 1080: {
            height = 1184;
            if (width == 1920) {
                width = 2048;
            }
            break;
        }

    }

    CAMHAL_LOGD("WxH updated to padded values : %d x %d", width, height);
    return true;
}

}  // namespace Camera
}  // namespace Ti

