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

#ifdef OMAP_ENHANCEMENT_CPCAM

#include "BufferSourceAdapter.h"
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <hal_public.h>

namespace Ti {
namespace Camera {

static int getANWFormat(const char* parameters_format)
{
    int format = HAL_PIXEL_FORMAT_TI_NV12;

    if (parameters_format != NULL) {
        if (strcmp(parameters_format, android::CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
            CAMHAL_LOGDA("CbYCrY format selected");
            // TODO(XXX): not defined yet
            format = -1;
        } else if (strcmp(parameters_format, android::CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            CAMHAL_LOGDA("YUV420SP format selected");
            format = HAL_PIXEL_FORMAT_TI_NV12;
        } else if (strcmp(parameters_format, android::CameraParameters::PIXEL_FORMAT_RGB565) == 0) {
            CAMHAL_LOGDA("RGB565 format selected");
            // TODO(XXX): not defined yet
            format = -1;
        } else if (strcmp(parameters_format, android::CameraParameters::PIXEL_FORMAT_BAYER_RGGB) == 0) {
            format = HAL_PIXEL_FORMAT_TI_Y16;
        } else {
            CAMHAL_LOGDA("Invalid format, NV12 format selected as default");
            format = HAL_PIXEL_FORMAT_TI_NV12;
        }
    }

    return format;
}

static int getUsageFromANW(int format)
{
    int usage = GRALLOC_USAGE_SW_READ_RARELY |
                GRALLOC_USAGE_SW_WRITE_NEVER;

    switch (format) {
        case HAL_PIXEL_FORMAT_TI_NV12:
            // This usage flag indicates to gralloc we want the
            // buffers to come from system heap
            usage |= GRALLOC_USAGE_PRIVATE_0;
            break;
        case HAL_PIXEL_FORMAT_TI_Y16:
        default:
            // No special flags needed
            break;
    }
    return usage;
}

static const char* getFormatFromANW(int format)
{
    switch (format) {
        case HAL_PIXEL_FORMAT_TI_NV12:
            // Assuming NV12 1D is RAW or Image frame
            return android::CameraParameters::PIXEL_FORMAT_YUV420SP;
        case HAL_PIXEL_FORMAT_TI_Y16:
            return android::CameraParameters::PIXEL_FORMAT_BAYER_RGGB;
        default:
            break;
    }
    return android::CameraParameters::PIXEL_FORMAT_YUV420SP;
}

static CameraFrame::FrameType formatToOutputFrameType(const char* format) {
    switch (getANWFormat(format)) {
        case HAL_PIXEL_FORMAT_TI_NV12:
        case HAL_PIXEL_FORMAT_TI_Y16:
            // Assuming NV12 1D is RAW or Image frame
            return CameraFrame::RAW_FRAME;
        default:
            break;
    }
    return CameraFrame::RAW_FRAME;
}

static int getHeightFromFormat(const char* format, int stride, int size) {
    CAMHAL_ASSERT((NULL != format) && (0 <= stride) && (0 <= size));
    switch (getANWFormat(format)) {
        case HAL_PIXEL_FORMAT_TI_NV12:
            return (size / (3 * stride)) * 2;
        case HAL_PIXEL_FORMAT_TI_Y16:
            return (size / stride) / 2;
        default:
            break;
    }
    return 0;
}

/*--------------------BufferSourceAdapter Class STARTS here-----------------------------*/


/**
 * Display Adapter class STARTS here..
 */
BufferSourceAdapter::BufferSourceAdapter() : mBufferCount(0)
{
    LOG_FUNCTION_NAME;

    mPixelFormat = NULL;
    mBuffers = NULL;
    mFrameProvider = NULL;
    mBufferSource = NULL;

    mFrameWidth = 0;
    mFrameHeight = 0;
    mPreviewWidth = 0;
    mPreviewHeight = 0;

    LOG_FUNCTION_NAME_EXIT;
}

BufferSourceAdapter::~BufferSourceAdapter()
{
    LOG_FUNCTION_NAME;

    destroy();

    if (mFrameProvider) {
        // Unregister with the frame provider
        mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);
        delete mFrameProvider;
        mFrameProvider = NULL;
    }

    if (mQueueFrame.get()) {
        mQueueFrame->requestExit();
        mQueueFrame.clear();
    }

    if (mReturnFrame.get()) {
        mReturnFrame->requestExit();
        mReturnFrame.clear();
    }

    if( mBuffers != NULL)
    {
        delete [] mBuffers;
        mBuffers = NULL;
    }

    LOG_FUNCTION_NAME_EXIT;
}

status_t BufferSourceAdapter::initialize()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

    mReturnFrame.clear();
    mReturnFrame = new ReturnFrame(this);
    mReturnFrame->run();

    mQueueFrame.clear();
    mQueueFrame = new QueueFrame(this);
    mQueueFrame->run();

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

int BufferSourceAdapter::setPreviewWindow(preview_stream_ops_t *source)
{
    LOG_FUNCTION_NAME;

    if (!source) {
        CAMHAL_LOGEA("NULL window object passed to DisplayAdapter");
        LOG_FUNCTION_NAME_EXIT;
        return BAD_VALUE;
    }

    if ( source == mBufferSource ) {
        return ALREADY_EXISTS;
    }

    // Destroy the existing source, if it exists
    destroy();

    // Move to new source obj
    mBufferSource = source;

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;
}

int BufferSourceAdapter::setFrameProvider(FrameNotifier *frameProvider)
{
    LOG_FUNCTION_NAME;

    if ( !frameProvider ) {
        CAMHAL_LOGEA("NULL passed for frame provider");
        LOG_FUNCTION_NAME_EXIT;
        return BAD_VALUE;
    }

    if ( NULL != mFrameProvider ) {
        delete mFrameProvider;
    }

    mFrameProvider = new FrameProvider(frameProvider, this, frameCallback);

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;
}

int BufferSourceAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

    if ( NULL == errorNotifier ) {
        CAMHAL_LOGEA("Invalid Error Notifier reference");
        return -EINVAL;
    }

    mErrorNotifier = errorNotifier;

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

int BufferSourceAdapter::enableDisplay(int width, int height,
                                       struct timeval *refTime)
{
    LOG_FUNCTION_NAME;
    CameraFrame::FrameType frameType;

    if (mFrameProvider == NULL) {
        // no-op frame provider not set yet
        return NO_ERROR;
    }

    if (mBufferSourceDirection == BUFFER_SOURCE_TAP_IN) {
        // only supporting one type of input frame
        frameType = CameraFrame::REPROCESS_INPUT_FRAME;
    } else {
        frameType = formatToOutputFrameType(mPixelFormat);
    }

    mFrameProvider->enableFrameNotification(frameType);
    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;
}

int BufferSourceAdapter::disableDisplay(bool cancel_buffer)
{
    LOG_FUNCTION_NAME;

    if (mFrameProvider) mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);

    LOG_FUNCTION_NAME_EXIT;

    return NO_ERROR;
}

status_t BufferSourceAdapter::pauseDisplay(bool pause)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

    // no-op for BufferSourceAdapter

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}


void BufferSourceAdapter::destroy()
{
    LOG_FUNCTION_NAME;

    mBufferCount = 0;

    LOG_FUNCTION_NAME_EXIT;
}

CameraBuffer* BufferSourceAdapter::allocateBufferList(int width, int dummyHeight, const char* format,
                                                      int &bytes, int numBufs)
{
    LOG_FUNCTION_NAME;
    status_t err;
    int i = -1;
    const int lnumBufs = numBufs;
    int undequeued = 0;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();

    mBuffers = new CameraBuffer [lnumBufs];
    memset (mBuffers, 0, sizeof(CameraBuffer) * lnumBufs);

    if ( NULL == mBufferSource ) {
        return NULL;
    }

    int pixFormat = getANWFormat(format);
    int usage = getUsageFromANW(pixFormat);

    // Set gralloc usage bits for window.
    err = mBufferSource->set_usage(mBufferSource, usage);
    if (err != 0) {
        CAMHAL_LOGE("native_window_set_usage failed: %s (%d)", strerror(-err), -err);

        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }

        return NULL;
    }

    CAMHAL_LOGDB("Number of buffers set to ANativeWindow %d", numBufs);
    // Set the number of buffers needed for this buffer source
    err = mBufferSource->set_buffer_count(mBufferSource, numBufs);
    if (err != 0) {
        CAMHAL_LOGE("native_window_set_buffer_count failed: %s (%d)", strerror(-err), -err);

        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }

        return NULL;
    }

    CAMHAL_LOGDB("Configuring %d buffers for ANativeWindow", numBufs);
    mBufferCount = numBufs;

    // re-calculate height depending on stride and size
    int height = getHeightFromFormat(format, width, bytes);

    // Set window geometry
    err = mBufferSource->set_buffers_geometry(mBufferSource,
                                              width, height,
                                              pixFormat);

    if (err != 0) {
        CAMHAL_LOGE("native_window_set_buffers_geometry failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }
        return NULL;
    }

    if ( mBuffers == NULL ) {
        CAMHAL_LOGEA("Couldn't create array for ANativeWindow buffers");
        LOG_FUNCTION_NAME_EXIT;
        return NULL;
    }

    mBufferSource->get_min_undequeued_buffer_count(mBufferSource, &undequeued);

    for (i = 0; i < mBufferCount; i++ ) {
        buffer_handle_t *handle;
        int stride;  // dummy variable to get stride
        // TODO(XXX): Do we need to keep stride information in camera hal?

        err = mBufferSource->dequeue_buffer(mBufferSource, &handle, &stride);

        if (err != 0) {
            CAMHAL_LOGEB("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                CAMHAL_LOGEA("Preview surface abandoned!");
                mBufferSource = NULL;
            }
            goto fail;
        }

        CAMHAL_LOGDB("got handle %p", handle);
        mBuffers[i].opaque = (void *)handle;
        mBuffers[i].type = CAMERA_BUFFER_ANW;
        mFramesWithCameraAdapterMap.add(handle, i);

        bytes = getBufSize(format, width, height);
    }

    for( i = 0;  i < mBufferCount-undequeued; i++ ) {
        void *y_uv[2];
        android::Rect bounds(width, height);

        buffer_handle_t *handle = (buffer_handle_t *) mBuffers[i].opaque;
        mBufferSource->lock_buffer(mBufferSource, handle);
        mapper.lock(*handle, CAMHAL_GRALLOC_USAGE, bounds, y_uv);
        mBuffers[i].mapped = y_uv[0];
    }

    // return the rest of the buffers back to ANativeWindow
    for(i = (mBufferCount-undequeued); i >= 0 && i < mBufferCount; i++) {
        buffer_handle_t *handle = (buffer_handle_t *) mBuffers[i].opaque;
        void *y_uv[2];
        android::Rect bounds(width, height);

        mapper.lock(*handle, CAMHAL_GRALLOC_USAGE, bounds, y_uv);
        mBuffers[i].mapped = y_uv[0];
        mapper.unlock(*handle);

        err = mBufferSource->cancel_buffer(mBufferSource, handle);
        if (err != 0) {
            CAMHAL_LOGEB("cancel_buffer failed: %s (%d)", strerror(-err), -err);
            if ( ENODEV == err ) {
                CAMHAL_LOGEA("Preview surface abandoned!");
                mBufferSource = NULL;
            }
            goto fail;
        }
        mFramesWithCameraAdapterMap.removeItem((buffer_handle_t *) mBuffers[i].opaque);
    }

    mPixelFormat = getPixFormatConstant(format);
    mFrameWidth = width;
    mFrameHeight = height;
    mBufferSourceDirection = BUFFER_SOURCE_TAP_OUT;

    return mBuffers;

 fail:
    // need to cancel buffers if any were dequeued
    for (int start = 0; start < i && i > 0; start++) {
        int err = mBufferSource->cancel_buffer(mBufferSource,
                (buffer_handle_t *) mBuffers[start].opaque);
        if (err != 0) {
          CAMHAL_LOGEB("cancelBuffer failed w/ error 0x%08x", err);
          break;
        }
        mFramesWithCameraAdapterMap.removeItem((buffer_handle_t *) mBuffers[start].opaque);
    }

    freeBufferList(mBuffers);

    CAMHAL_LOGEA("Error occurred, performing cleanup");

    if (NULL != mErrorNotifier.get()) {
        mErrorNotifier->errorNotify(-ENOMEM);
    }

    LOG_FUNCTION_NAME_EXIT;
    return NULL;

}

CameraBuffer* BufferSourceAdapter::getBufferList(int *num) {
    LOG_FUNCTION_NAME;
    status_t err;
    const int lnumBufs = 1;
    int formatSource;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();
    buffer_handle_t *handle;

    // TODO(XXX): Only supporting one input buffer at a time right now
    *num = 1;
    mBuffers = new CameraBuffer [lnumBufs];
    memset (mBuffers, 0, sizeof(CameraBuffer) * lnumBufs);

    if ( NULL == mBufferSource ) {
        return NULL;
    }

    err = extendedOps()->update_and_get_buffer(mBufferSource, &handle, &mBuffers[0].stride);
    if (err != 0) {
        CAMHAL_LOGEB("update and get buffer failed: %s (%d)", strerror(-err), -err);
        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }
        goto fail;
    }

    CAMHAL_LOGD("got handle %p", handle);
    mBuffers[0].opaque = (void *)handle;
    mBuffers[0].type = CAMERA_BUFFER_ANW;
    mFramesWithCameraAdapterMap.add(handle, 0);

    err = extendedOps()->get_buffer_dimension(mBufferSource, &mBuffers[0].width, &mBuffers[0].height);
    err = extendedOps()->get_buffer_format(mBufferSource, &formatSource);

    // lock buffer
    {
        void *y_uv[2];
        android::Rect bounds(mBuffers[0].width, mBuffers[0].height);
        mapper.lock(*handle, CAMHAL_GRALLOC_USAGE, bounds, y_uv);
        mBuffers[0].mapped = y_uv[0];
    }

    mFrameWidth = mBuffers[0].width;
    mFrameHeight = mBuffers[0].height;
    mPixelFormat = getFormatFromANW(formatSource);

    mBuffers[0].format = mPixelFormat;
    mBufferSourceDirection = BUFFER_SOURCE_TAP_IN;

    return mBuffers;

 fail:
    // need to cancel buffers if any were dequeued
    freeBufferList(mBuffers);

    if (NULL != mErrorNotifier.get()) {
        mErrorNotifier->errorNotify(-ENOMEM);
    }

    LOG_FUNCTION_NAME_EXIT;
    return NULL;
}

uint32_t * BufferSourceAdapter::getOffsets()
{
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;

    return NULL;
}

int BufferSourceAdapter::minUndequeueableBuffers(int& undequeueable) {
    LOG_FUNCTION_NAME;
    int ret = NO_ERROR;

    if(!mBufferSource)
    {
        ret = INVALID_OPERATION;
        goto end;
    }

    ret = mBufferSource->get_min_undequeued_buffer_count(mBufferSource, &undequeueable);
    if ( NO_ERROR != ret ) {
        CAMHAL_LOGEB("get_min_undequeued_buffer_count failed: %s (%d)", strerror(-ret), -ret);
        if ( ENODEV == ret ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }
        return -ret;
    }

 end:
    return ret;
    LOG_FUNCTION_NAME_EXIT;

}

int BufferSourceAdapter::maxQueueableBuffers(unsigned int& queueable)
{
    LOG_FUNCTION_NAME;
    int ret = NO_ERROR;
    int undequeued = 0;

    if(mBufferCount == 0) {
        ret = INVALID_OPERATION;
        goto end;
    }

    ret = minUndequeueableBuffers(undequeued);
    if (ret != NO_ERROR) {
        goto end;
    }

    queueable = mBufferCount - undequeued;

 end:
    return ret;
    LOG_FUNCTION_NAME_EXIT;
}

int BufferSourceAdapter::getFd()
{
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;

    return -1;

}

status_t BufferSourceAdapter::returnBuffersToWindow()
{
    status_t ret = NO_ERROR;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();

    //Give the buffers back to display here -  sort of free it
    if (mBufferSource) {
        for(unsigned int i = 0; i < mFramesWithCameraAdapterMap.size(); i++) {
            int value = mFramesWithCameraAdapterMap.valueAt(i);
            buffer_handle_t *handle = (buffer_handle_t *) mBuffers[value].opaque;

            // if buffer index is out of bounds skip
            if ((value < 0) || (value >= mBufferCount)) {
                CAMHAL_LOGEA("Potential out bounds access to handle...skipping");
                continue;
            }

            // unlock buffer before giving it up
            mapper.unlock(*handle);

            ret = mBufferSource->cancel_buffer(mBufferSource, handle);
            if ( ENODEV == ret ) {
                CAMHAL_LOGEA("Preview surface abandoned!");
                mBufferSource = NULL;
                return -ret;
            } else if ( NO_ERROR != ret ) {
                CAMHAL_LOGEB("cancel_buffer() failed: %s (%d)",
                              strerror(-ret),
                              -ret);
                return -ret;
            }
        }
    } else {
         CAMHAL_LOGE("mBufferSource is NULL");
    }

     ///Clear the frames with camera adapter map
     mFramesWithCameraAdapterMap.clear();

     return ret;

}

int BufferSourceAdapter::freeBufferList(CameraBuffer * buflist)
{
    LOG_FUNCTION_NAME;

    status_t ret = NO_ERROR;

    android::AutoMutex lock(mLock);

    if (mBufferSourceDirection == BUFFER_SOURCE_TAP_OUT) returnBuffersToWindow();

    if ( NULL != buflist )
    {
        delete [] buflist;
        mBuffers = NULL;
    }

    if( mBuffers != NULL)
    {
        delete [] mBuffers;
        mBuffers = NULL;
    }

    return NO_ERROR;
}


bool BufferSourceAdapter::supportsExternalBuffering()
{
    return false;
}

void BufferSourceAdapter::addFrame(CameraFrame* frame)
{
    if (mQueueFrame.get()) {
        mQueueFrame->addFrame(frame);
    }
}

void BufferSourceAdapter::handleFrameCallback(CameraFrame* frame)
{
    status_t ret = NO_ERROR;
    buffer_handle_t *handle = NULL;
    int i;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();

    if (!mBuffers || !frame->mBuffer) {
        CAMHAL_LOGEA("Adapter sent BufferSourceAdapter a NULL frame?");
        return;
    }

    android::AutoMutex lock(mLock);

    for ( i = 0; i < mBufferCount; i++ ) {
        if (frame->mBuffer == &mBuffers[i]) {
            break;
        }
    }

    handle = (buffer_handle_t *) mBuffers[i].opaque;

    // Handle input buffers
    // TODO(XXX): Move handling of input buffers out of here if
    // it becomes more complex
    if (frame->mFrameType == CameraFrame::REPROCESS_INPUT_FRAME) {
        CAMHAL_LOGD("Unlock %p (buffer #%d)", handle, i);
        mapper.unlock(*handle);
        return;
    }

    if ( NULL != frame->mMetaData.get() ) {
        camera_memory_t *extMeta = frame->mMetaData->getExtendedMetadata();
        if ( NULL != extMeta ) {
            camera_metadata_t *metaData = static_cast<camera_metadata_t *> (extMeta->data);
            metaData->timestamp = frame->mTimestamp;
            ret = extendedOps()->set_metadata(mBufferSource, extMeta);
            if (ret != 0) {
                CAMHAL_LOGE("Surface::set_metadata returned error %d", ret);
            }
        }
    }

    // unlock buffer before enqueueing
    mapper.unlock(*handle);

    ret = mBufferSource->enqueue_buffer(mBufferSource, handle);
    if (ret != 0) {
        CAMHAL_LOGE("Surface::queueBuffer returned error %d", ret);
    }

    mFramesWithCameraAdapterMap.removeItem((buffer_handle_t *) frame->mBuffer->opaque);

    // signal return frame thread that it can dequeue a buffer now
    mReturnFrame->signal();
}


bool BufferSourceAdapter::handleFrameReturn()
{
    status_t err;
    buffer_handle_t *buf;
    int i = 0;
    int stride;  // dummy variable to get stride
    CameraFrame::FrameType type;
    android::GraphicBufferMapper &mapper = android::GraphicBufferMapper::get();
    void *y_uv[2];
    android::Rect bounds(mFrameWidth, mFrameHeight);

    if ( NULL == mBufferSource ) {
        return false;
    }

    err = mBufferSource->dequeue_buffer(mBufferSource, &buf, &stride);
    if (err != 0) {
        CAMHAL_LOGEB("dequeueBuffer failed: %s (%d)", strerror(-err), -err);

        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }

        return false;
    }

    err = mBufferSource->lock_buffer(mBufferSource, buf);
    if (err != 0) {
        CAMHAL_LOGEB("lockbuffer failed: %s (%d)", strerror(-err), -err);

        if ( ENODEV == err ) {
            CAMHAL_LOGEA("Preview surface abandoned!");
            mBufferSource = NULL;
        }

        return false;
    }

    mapper.lock(*buf, CAMHAL_GRALLOC_USAGE, bounds, y_uv);

    for(i = 0; i < mBufferCount; i++) {
        if (mBuffers[i].opaque == buf)
            break;
    }

    if (i >= mBufferCount) {
        CAMHAL_LOGEB("Failed to find handle %p", buf);
    }

    mFramesWithCameraAdapterMap.add((buffer_handle_t *) mBuffers[i].opaque, i);

    CAMHAL_LOGVB("handleFrameReturn: found graphic buffer %d of %d", i, mBufferCount - 1);

    mFrameProvider->returnFrame(&mBuffers[i], formatToOutputFrameType(mPixelFormat));
    return true;
}

void BufferSourceAdapter::frameCallback(CameraFrame* caFrame)
{
    if ((NULL != caFrame) && (NULL != caFrame->mCookie)) {
        BufferSourceAdapter *da = (BufferSourceAdapter*) caFrame->mCookie;
        da->addFrame(caFrame);
    } else {
        CAMHAL_LOGEB("Invalid Cookie in Camera Frame = %p, Cookie = %p",
                    caFrame, caFrame ? caFrame->mCookie : NULL);
    }
}

/*--------------------BufferSourceAdapter Class ENDS here-----------------------------*/

} // namespace Camera
} // namespace Ti

#endif
