/*
This implements a data source that decodes HE-AAC streams via uuac

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/
#ifndef miniaudio_fdkaac_h
#define miniaudio_fdkaac_h

#include <errno.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_FDKACC)
// #include <stdio.h>
// #include <stdint.h>
// #include <unistd.h>
// #include <stdlib.h>
#include <libavformat/avformat.h>
#include "libAACdec/include/aacdecoder_lib.h"
#endif

typedef struct
{
    ma_data_source_base ds;     /* The aac decoder can be used independently as a data source. */
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;           /* Will be either f32 or s16. */
#if !defined(MA_NO_FDKACC)
    HANDLE_AACDECODER handle;
    AVFormatContext *in;
	AVStream *st;
    INT_PCM *decode_buf;
    int output_size;
    CStreamInfo *info;
    // int bytesLeft;
    // int samplerate;
    // int channels;
#endif
} ma_fdkaac;

MA_API ma_result ma_fdkaac_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC);
MA_API ma_result ma_fdkaac_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC);
MA_API void ma_fdkaac_uninit(ma_fdkaac* pAAC, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_fdkaac_read_pcm_frames(ma_fdkaac* pAAC, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_fdkaac_seek_to_pcm_frame(ma_fdkaac* pAAC, ma_uint64 frameIndex);
MA_API ma_result ma_fdkaac_get_data_format(ma_fdkaac* pAAC, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_fdkaac_get_cursor_in_pcm_frames(ma_fdkaac* pAAC, ma_uint64* pCursor);
MA_API ma_result ma_fdkaac_get_length_in_pcm_frames(ma_fdkaac* pAAC, ma_uint64* pLength);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

static ma_result ma_fdkaac_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
	// return MA_NOT_IMPLEMENTED;
    return ma_fdkaac_read_pcm_frames((ma_fdkaac*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_fdkaac_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
	return MA_NOT_IMPLEMENTED;
    // return ma_fdkaac_seek_to_pcm_frame((ma_fdkaac*)pDataSource, frameIndex);
}

static ma_result ma_fdkaac_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_fdkaac_get_data_format((ma_fdkaac*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_fdkaac_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
	return MA_NOT_IMPLEMENTED;
    // return ma_fdkaac_get_cursor_in_pcm_frames((ma_fdkaac*)pDataSource, pCursor);
}

static ma_result ma_fdkaac_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
	return MA_NOT_IMPLEMENTED;
    // return ma_fdkaac_get_length_in_pcm_frames((ma_fdkaac*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_fdkaac_ds_vtable =
{
    ma_fdkaac_ds_read,
    ma_fdkaac_ds_seek,
    ma_fdkaac_ds_get_data_format,
    ma_fdkaac_ds_get_cursor,
    ma_fdkaac_ds_get_length
};


#if !defined(MA_NO_FDKACC)
// static int ma_fdkaac_of_callback__read(void* pUserData, unsigned char* pBufferOut, int bytesToRead)
// {
//     ma_fdkaac* pAAC = (ma_fdkaac*)pUserData;
//     ma_result result;
//     size_t bytesRead;

//     result = pAAC->onRead(pAAC->pReadSeekTellUserData, (void*)pBufferOut, bytesToRead, &bytesRead);

//     if (result != MA_SUCCESS) {
//         return -1;
//     }

//     return (int)bytesRead;
// }

// static int ma_fdkaac_of_callback__seek(void* pUserData, ogg_int64_t offset, int whence)
// {
//     ma_fdkaac* pAAC = (ma_fdkaac*)pUserData;
//     ma_result result;
//     ma_seek_origin origin;

//     if (whence == SEEK_SET) {
//         origin = ma_seek_origin_start;
//     } else if (whence == SEEK_END) {
//         origin = ma_seek_origin_end;
//     } else {
//         origin = ma_seek_origin_current;
//     }

//     result = pAAC->onSeek(pAAC->pReadSeekTellUserData, offset, origin);
//     if (result != MA_SUCCESS) {
//         return -1;
//     }

//     return 0;
// }

// static opus_int64 ma_fdkaac_of_callback__tell(void* pUserData)
// {
//     ma_fdkaac* pAAC = (ma_fdkaac*)pUserData;
//     ma_result result;
//     ma_int64 cursor;

//     if (pAAC->onTell == NULL) {
//         return -1;
//     }

//     result = pAAC->onTell(pAAC->pReadSeekTellUserData, &cursor);
//     if (result != MA_SUCCESS) {
//         return -1;
//     }

//     return cursor;
// }
#endif

static ma_result ma_fdkaac_init_internal(const ma_decoding_backend_config* pConfig, ma_fdkaac* pAAC)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (pAAC == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pAAC);
    pAAC->format = ma_format_f32; /* f32 by default. */

    if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16)) {
        pAAC->format = pConfig->preferredFormat;
    } else {
        /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
    }

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_fdkaac_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pAAC->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

// TODO @grish -- maybe we should be passing in the memory alloc callbacks into fdkaac?
MA_API ma_result ma_fdkaac_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libopus. */

    result = ma_fdkaac_init_internal(pConfig, pAAC);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pAAC->onRead = onRead;
    pAAC->onSeek = onSeek;
    pAAC->onTell = onTell;
    pAAC->pReadSeekTellUserData = pReadSeekTellUserData;

    #if !defined(MA_NO_FDKACC)
    {
 		pAAC->handle = aacDecoder_Open(TT_MP4_RAW, 1); // TODO: can these args vary? first one in particular
        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. */
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_fdkaac_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC)
{
    ma_result result;

    (void)pAllocationCallbacks; // TODO ?

    result = ma_fdkaac_init_internal(pConfig, pAAC);
    if (result != MA_SUCCESS) {
        return result;
    }


    #if !defined(MA_NO_FDKACC)
    {
    	int ret;
    	unsigned i;
    	AVFormatContext *in = NULL;
    	AVStream *st = NULL;
    	UINT input_length;
    	AAC_DECODER_ERROR err;

    #if LIBAVFORMAT_VERSION_MICRO < 100 || LIBAVFORMAT_VERSION_MAJOR < 58 || LIBAVFORMAT_VERSION_MINOR < 9
        av_register_all();
    #endif
        if (avformat_open_input(&in, pFilePath, NULL, NULL) < 0) {
            char buf[100];
            av_strerror(ret, buf, sizeof(buf));
            fprintf(stderr, "%s: %s\n", pFilePath, buf);
            return MA_INVALID_FILE;
        }
        for (i = 0; i < in->nb_streams && !st; i++) {
            if (in->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC)
                st = in->streams[i];
        }
        if (!st) {
            fprintf(stderr, "No AAC stream found\n");
            return MA_INVALID_DATA;
        }
        if (!st->codecpar->extradata_size) {
            fprintf(stderr, "No AAC ASC found\n");
            return MA_INVALID_DATA;
        }

        if (pAAC->handle == NULL) {
     		pAAC->handle = aacDecoder_Open(TT_MP4_RAW, 1); // TODO: can these args vary? first one in particular
        }
        pAAC->in = in;
        pAAC->st = st;
    	input_length = st->codecpar->extradata_size;
        err = aacDecoder_ConfigRaw(pAAC->handle, &st->codecpar->extradata, &input_length);
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Unable to decode the ASC\n");
            return MA_INVALID_DATA;
        }
        pAAC->info = NULL;
    	// pAAC->info = aacDecoder_GetStreamInfo(pAAC->handle); // TODO: can we call this before decoding?

    	pAAC->output_size = 8*sizeof(INT_PCM)*2048;
        pAAC->decode_buf = (INT_PCM*)malloc(pAAC->output_size);

        return MA_SUCCESS;
    }
    #else
    {
        /* fdkaac is disabled. */
        (void)pFilePath;
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API void ma_fdkaac_uninit(ma_fdkaac* pAAC, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pAAC == NULL) {
        return;
    }

    (void)pAllocationCallbacks;

    #if !defined(MA_NO_FDKACC)
    {
        if (pAAC->decode_buf != NULL) {
            free(pAAC->decode_buf);
        }
        avformat_close_input(&pAAC->in);
        aacDecoder_Close(pAAC->handle);
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
    }
    #endif

    ma_data_source_uninit(&pAAC->ds);
}

MA_API ma_result ma_fdkaac_read_pcm_frames(ma_fdkaac* pAAC, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pAAC == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FDKACC)
    {
        /* We always use floating point format. */
        ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
        ma_uint64 totalFramesRead;
        ma_format format;
        ma_uint32 channels;

        ma_fdkaac_get_data_format(pAAC, &format, &channels, NULL, NULL, 0);

        totalFramesRead = 0;
    	AAC_DECODER_ERROR err;
        while (totalFramesRead < frameCount) {
            UINT valid;
            AVPacket pkt = { 0 };
            int ret = av_read_frame(pAAC->in, &pkt);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN))
                    continue;
                result = MA_AT_END;  /* could be another error ? from avformat.h:
                                     *  @return 0 if OK, < 0 on error or end of file. On error, pkt will be blank
                                     *          (as if it came from av_packet_alloc()).
                                     */
                break;
            }
    		if (pkt.stream_index != pAAC->st->index) {
    			av_packet_unref(&pkt);
    			continue;
    		}

            valid = pkt.size;
            UINT input_length = pkt.size;

        	err = aacDecoder_Fill(pAAC->handle, &pkt.data, &input_length, &valid);
        	if (err != AAC_DEC_OK) {
                fprintf(stderr, "Fill failed: %x\n", err);
                result = MA_ERROR;
                break;
        	}

        	err = aacDecoder_DecodeFrame(pAAC->handle, pAAC->decode_buf, pAAC->output_size / sizeof(INT_PCM), 0);
        	av_packet_unref(&pkt);
        	if (err == AAC_DEC_NOT_ENOUGH_BITS) {
            	continue;
        	}
        	if (err != AAC_DEC_OK) {
            	fprintf(stderr, "Decode failed: %x\n", err);
				result = MA_ERROR;
				break;
        	}

			totalFramesRead += 1;

    		pAAC->info = aacDecoder_GetStreamInfo(pAAC->handle); // TODO: can we call this before decoding?
        }

        if (pFramesRead != NULL) {
            *pFramesRead = totalFramesRead;
        }

        if (result == MA_SUCCESS && totalFramesRead == 0) {
            result = MA_AT_END;
        }

        return result;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

// MA_API ma_result ma_uaac_seek_to_pcm_frame(ma_uaac* pAAC, ma_uint64 frameIndex)
// {
//     if (pAAC == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     #if !defined(MA_NO_FDKACC)
//     {
//         int libopusResult = op_pcm_seek(pAAC->of, (ogg_int64_t)frameIndex);
//         if (libopusResult != 0) {
//             if (libopusResult == OP_ENOSEEK) {
//                 return MA_INVALID_OPERATION;    /* Not seekable. */
//             } else if (libopusResult == OP_EINVAL) {
//                 return MA_INVALID_ARGS;
//             } else {
//                 return MA_ERROR;
//             }
//         }

//         return MA_SUCCESS;
//     }
//     #else
//     {
//         /* libopus is disabled. Should never hit this since initialization would have failed. */
//         MA_ASSERT(MA_FALSE);

//         (void)frameIndex;

//         return MA_NOT_IMPLEMENTED;
//     }
//     #endif
// }

MA_API ma_result ma_fdkaac_get_data_format(ma_fdkaac* pAAC, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    /* Defaults for safety. */
    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }
    if (pChannelMap != NULL) {
        MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
    }

    if (pAAC == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pAAC->format;
    }

    #if !defined(MA_NO_FDKACC)
    {
        if (pChannels != NULL) {
            if (pAAC->info == NULL) *pChannels = 2;
            else                    *pChannels = pAAC->info->numChannels;
            // printf("OI! %d\n", *pChannels);
        }

        if (pSampleRate != NULL) {
            if (pAAC->info == NULL) *pSampleRate = 44100;
            else                    *pSampleRate = pAAC->info->sampleRate;
            // printf("OI AGAIN! %d\n", *pSampleRate);
        }

        if (pChannelMap != NULL) {
            ma_channel_map_init_standard(ma_standard_channel_map_fdkaac, pChannelMap, channelMapCap, *pChannels);
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}


// MA_API ma_result ma_uaac_get_cursor_in_pcm_frames(ma_uaac* pAAC, ma_uint64* pCursor)
// {
//     if (pCursor == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     *pCursor = 0;   /* Safety. */

//     if (pAAC == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     #if !defined(MA_NO_FDKACC)
//     {
//         ogg_int64_t offset = op_pcm_tell(pAAC->of);
//         if (offset < 0) {
//             return MA_INVALID_FILE;
//         }

//         *pCursor = (ma_uint64)offset;

//         return MA_SUCCESS;
//     }
//     #else
//     {
//         /* libopus is disabled. Should never hit this since initialization would have failed. */
//         MA_ASSERT(MA_FALSE);
//         return MA_NOT_IMPLEMENTED;
//     }
//     #endif
// }

// MA_API ma_result ma_uaac_get_length_in_pcm_frames(ma_uaac* pAAC, ma_uint64* pLength)
// {
//     if (pLength == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     *pLength = 0;   /* Safety. */

//     if (pAAC == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     #if !defined(MA_NO_FDKACC)
//     {
//         ogg_int64_t length = op_pcm_total(pAAC->of, -1);
//         if (length < 0) {
//             return MA_ERROR;
//         }

//         *pLength = (ma_uint64)length;

//         return MA_SUCCESS;
//     }
//     #else
//     {
//         /* libopus is disabled. Should never hit this since initialization would have failed. */
//         MA_ASSERT(MA_FALSE);
//         return MA_NOT_IMPLEMENTED;
//     }
//     #endif
// }

#endif
