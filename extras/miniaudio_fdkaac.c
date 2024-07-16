/*
This implements a data source that decodes HE-AAC streams via uuac

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/
#include "miniaudio_fdkaac.h"

#ifndef miniaudio_fdkaac_c
#define miniaudio_fdkaac_c

#include <errno.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_FDKACC)

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include <libavformat/avformat.h>
#include <fdk-aac/aacdecoder_lib.h>
#endif

#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

static ma_result ma_fdkaac_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_fdkaac_read_pcm_frames((ma_fdkaac*)pDataSource, pFramesOut, frameCount, pFramesRead);
	// return MA_NOT_IMPLEMENTED;
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
    return ma_fdkaac_get_cursor_in_pcm_frames((ma_fdkaac*)pDataSource, pCursor);
    // *pCursor = 0;
	// return MA_NOT_IMPLEMENTED;
}

static ma_result ma_fdkaac_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
 //    *pLength = 0;
	// return MA_NOT_IMPLEMENTED;
    return ma_fdkaac_get_length_in_pcm_frames((ma_fdkaac*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_fdkaac_ds_vtable =
{
    ma_fdkaac_ds_read,
    ma_fdkaac_ds_seek,
    ma_fdkaac_ds_get_data_format,
    ma_fdkaac_ds_get_cursor,
    ma_fdkaac_ds_get_length
};

// Decodes a single AAC frame and stores the result in pAAC->decode_buf. This always writes from the start of the
// buffer and doesn't care if there's data in it already. After decoding the frame this queries the AAC decoder
// for info on the current stream (number of channels, frame size, etc.) and updates in pAAC->info.
ma_result decode_one_aac_frame(ma_fdkaac* pAAC) {
    ma_result result = MA_SUCCESS;
	AAC_DECODER_ERROR err;

    while (1) {
        UINT valid;
        AVPacket pkt = { 0 };
        int ret = av_read_frame(pAAC->in, &pkt);
            if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
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

        err = aacDecoder_DecodeFrame(pAAC->handle, pAAC->decode_buf, pAAC->decode_buf_size / sizeof(INT_PCM), 0);
        pAAC->decode_buf_start = 0;

        av_packet_unref(&pkt);
        if (err == AAC_DEC_NOT_ENOUGH_BITS) {
        	continue;
        }
        if (err != AAC_DEC_OK) {
        	fprintf(stderr, "Decode failed: %x\n", err);
        	result = MA_ERROR;
        	break;
        }

        if (!pAAC->info) {
        	pAAC->info = aacDecoder_GetStreamInfo(pAAC->handle);
        	if (!pAAC->info || pAAC->info->sampleRate <= 0) {
        		fprintf(stderr, "No stream info\n");
        		result = MA_ERROR;
        	}
        }

        break;
    }
    return result;
}

static ma_result ma_fdkaac_init_internal(const ma_decoding_backend_config* pConfig, ma_fdkaac* pAAC)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (pAAC == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pAAC);
    // pAAC->format = ma_format_f32; /* f32 by default. */
    pAAC->format = ma_format_s16; /* s16 by default. */

    // if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16)) {
        // pAAC->format = pConfig->preferredFormat;
    // } else {
    //     /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
    // }

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_fdkaac_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pAAC->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

#if !defined(MA_NO_FDKACC)
static int ma_fdkaac_of_callback__read(void* pUserData, unsigned char* pBufferOut, int bytesToRead)
{
    ma_result result;
    ma_fdkaac* pAAC = (ma_fdkaac*)pUserData;
    size_t bytesRead;

    result = pAAC->onRead(pAAC->pReadSeekTellUserData, (void*)pBufferOut, bytesToRead, &bytesRead);
    if (result != MA_SUCCESS) {
        if (result == MA_AT_END) {
            return AVERROR_EOF;
        }
        return -1;
    }

	printf("bytesRead: %d\n", (int)bytesRead);
    return (int)bytesRead;
}

static int64_t ma_fdkaac_of_callback__seek(void* pUserData, int64_t offset, int whence)
{
    ma_fdkaac* pAAC = (ma_fdkaac*)pUserData;
    ma_result result;
    ma_seek_origin origin;

    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else if (whence == AVSEEK_SIZE) {

		printf("pAAC->in->pb->seekable: %d\n", pAAC->in->pb->seekable);
		printf("pAAC->in->pb->buffer_size: %d\n", pAAC->in->pb->buffer_size);
        int64_t pos = pAAC->in->pb->pos;
        int ret = avio_seek(pAAC->in->pb, 0L, SEEK_END);
        if (ret < 0) {
            char buf[100];
            av_strerror(ret, buf, sizeof(buf));
            fprintf(stderr, "avio_seek to end failed: %s\n", buf);
            // return MA_INVALID_FILE;
            return -1;
        }
        int fsize = avio_tell(pAAC->in->pb);
        printf("fsize: %d\n");
        return fsize;


		// int original = pAAC->in->pb->pos;
  //       result = pAAC->onSeek(pAAC->pReadSeekTellUserData, 0, ma_seek_origin_end);
  //       if (result != MA_SUCCESS) {
  //           if (result == MA_AT_END) {
  //               printf("HERE 1\n");
  //               return pAAC->in->pb->pos;
  //           }
  //           return -1;
  //       }
		// int fsize = pAAC->in->pb->pos;
  //       result = pAAC->onSeek(pAAC->pReadSeekTellUserData, original, ma_seek_origin_start);
  //       if (result != MA_SUCCESS) {
  //           return -1;
  //       }
        // return fsize;

        // printf("AVSEEK_SIZE: %d\n", fsize);
        // return 177635;
        return -1;
    } else {
        origin = ma_seek_origin_current;
    }

    result = pAAC->onSeek(pAAC->pReadSeekTellUserData, offset, origin);
    if (result != MA_SUCCESS) {
            if (result == MA_AT_END) {
                printf("HERE 2\n");
                return pAAC->in->pb->pos;
            }
        return -1;
    }

    return pAAC->in->pb->pos;

	// TODO: return num bytes
	// ma_uint64 cursor;
 //    result = ma_fdkaac_get_cursor_in_pcm_frames(pAAC, &cursor);
 //    if (result != MA_SUCCESS) {
 //        return -1;
 //    }
 //    printf("cursor: %d\n", cursor);
    // return cursor;
}

// static uint64 ma_fdkaac_of_callback__tell(void* pUserData)
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

MA_API ma_result ma_fdkaac_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libopus. */
	// TODO: pAllocationCallbacks can be set in pConfig (if necessary)
	// pAllocationCallbacks->onMalloc = av_malloc;

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
        const int iBufSize = 32 * 1024; // 32 Kb
        // char* pBuffer = (char*)ma_malloc(iBufSize, pAllocationCallbacks);
        char* pBuffer = (char*)av_malloc(iBufSize);
        AVIOContext* pIOCtx = avio_alloc_context(pBuffer, iBufSize, 0, pAAC, ma_fdkaac_of_callback__read, 0, ma_fdkaac_of_callback__seek);
        if (pIOCtx == NULL) {
            fprintf(stderr, "Failed to allocated AVIO context\n");
            return MA_ERROR;
        }

        pAAC->in = avformat_alloc_context();
        pAAC->in->pb = pIOCtx;


        // Determining the input format:
        // long unsigned int ulReadBytes ulReadBytes = pAAC->onRead(pAAC->pReadSeekTellUserData, (void*)pBuffer, iBufSize, &ulReadBytes);
        int ulReadBytes = ma_fdkaac_of_callback__read((void*)pAAC, pBuffer, iBufSize);
        if (ulReadBytes < 0) {
            fprintf(stderr, "Initial read failed\n");
            return MA_ERROR;
        }
        // printf("pAAC->in->pb->file_size %d\n", pAAC->in->pb->file_size);

        // Don't forget to reset the data pointer back to the beginning!
        if (ma_fdkaac_of_callback__seek((void*)pAAC, 0, SEEK_SET) < 0) {
            fprintf(stderr, "Initial seek failed\n");
            return MA_ERROR;
        }

        // Now we set the ProbeData-structure for av_probe_input_format:
        AVProbeData probeData;
        probeData.buf = pBuffer;
        probeData.buf_size = ulReadBytes;
        probeData.filename = "";

        // Determine the input-format:
        pAAC->in->iformat = av_probe_input_format(&probeData, 1);
        // pAAC->in->iformat = av_find_input_format("m4a");
        // printf("AFTER pAAC->in->iformat: '%s'\n", pAAC->in->iformat->long_name);

        // pAAC->in->flags = AVFMT_FLAG_CUSTOM_IO | AVFMT_FLAG_GENPTS | AVFMT_FLAG_DISCARD_CORRUPT;
        pAAC->in->flags = AVFMT_FLAG_CUSTOM_IO;

		int ret;

        ret = avformat_open_input(&pAAC->in, NULL, NULL, NULL);
        // if (avformat_open_input(&pAAC->in, "", 0, 0) != 0) {
        if (ret != 0) {
            char buf[100];
            av_strerror(ret, buf, sizeof(buf));
            fprintf(stderr, "avformat_open_input failed: %s\n", buf);
            // return MA_INVALID_FILE;
            return MA_ERROR;
        }

 		// pAAC->handle = aacDecoder_Open(TT_MP4_RAW, 1); // TODO: can these args vary? first one in particular

    	unsigned i;
    	AVStream *st = NULL;
    	UINT input_length;
    	AAC_DECODER_ERROR err;

        for (i = 0; i < pAAC->in->nb_streams && !st; i++) {
            if (pAAC->in->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC)
                st = pAAC->in->streams[i];
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
        pAAC->pcm_frame_cursor = 0;
        // pAAC->in = in;
        pAAC->st = st;
    	input_length = st->codecpar->extradata_size;
        err = aacDecoder_ConfigRaw(pAAC->handle, &st->codecpar->extradata, &input_length);
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Unable to decode the ASC\n");
            return MA_INVALID_DATA;
        }
        pAAC->info = NULL;

    	pAAC->decode_buf_size = 8*2048*sizeof(INT_PCM); // larger than we probably need (maybe 2048 * 2 * sizeof(INT_PCM) is more realistic?)
    													// HE-AAC maxes at 2048 PCM frames in one AAC frame * 2 channels (maybe the example
    													// used 8 for 7.1 surround?)
        pAAC->decode_buf = (INT_PCM*)ma_malloc(pAAC->decode_buf_size, pAllocationCallbacks);
        pAAC->decode_buf_start = -1; // -1 means we don't currently have any valid data in the buffer

        // loads one frame into the buffer and initializes pAAC->info (so we have number of channels / etc.)
        return decode_one_aac_frame(pAAC);
        // return MA_SUCCESS;
    }
    #else
    {
        /* fdkaac is disabled. */
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_fdkaac_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_fdkaac* pAAC)
{
    ma_result result;

    // (void)pAllocationCallbacks; // TODO ?

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
        ret = avformat_open_input(&in, pFilePath, NULL, NULL);
        if (ret < 0) {
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
        pAAC->pcm_frame_cursor = 0;
        pAAC->in = in;
        pAAC->st = st;
    	input_length = st->codecpar->extradata_size;
        err = aacDecoder_ConfigRaw(pAAC->handle, &st->codecpar->extradata, &input_length);
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Unable to decode the ASC\n");
            return MA_INVALID_DATA;
        }
        pAAC->info = NULL;

    	pAAC->decode_buf_size = 8*2048*sizeof(INT_PCM); // larger than we probably need (maybe 2048 * 2 * sizeof(INT_PCM) is more realistic?)
    													// HE-AAC maxes at 2048 PCM frames in one AAC frame * 2 channels (maybe the example
    													// used 8 for 7.1 surround?)
        pAAC->decode_buf = (INT_PCM*)ma_malloc(pAAC->decode_buf_size, pAllocationCallbacks);
        pAAC->decode_buf_start = -1; // -1 means we don't currently have any valid data in the buffer

        // loads one frame into the buffer and initializes pAAC->info (so we have number of channels / etc.)
        return decode_one_aac_frame(pAAC);
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
    #if !defined(MA_NO_FDKACC)
    {
        if (pAAC == NULL) {
            return;
        }
        if (pAAC->decode_buf) ma_free(pAAC->decode_buf, pAllocationCallbacks);
        if (pAAC->in)         avformat_close_input(&pAAC->in);
        if (pAAC->handle)     aacDecoder_Close(pAAC->handle);
    }
    #else
    {
        /* fdkaac is disabled. Should never hit this since initialization would have failed. */
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
        ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
        ma_uint64 totalPCMFramesRead = 0;

    	INT_PCM* pcmOut = pFramesOut;

        while (1) {
            // these values can technically change on each DecodeFrame call (but shouldn't?) since pAAC->info is updated
            ma_uint32 numChannels = pAAC->info ? pAAC->info->numChannels : 2;
            ma_uint32 frameSize = pAAC->info ? pAAC->info->frameSize : 1024; // 1024 is common for AAC-LC
            int decode_buf_end = numChannels * frameSize; // index of the last valid decoded value in the decode buffer.
                                                          // this should never be > the total buffer size (since we fill/drain one frame at a time, and the buffer is big)

			for (int i = pAAC->decode_buf_start; i < decode_buf_end; i += numChannels) {
    			for (unsigned j = 0; j < numChannels; ++j)
            		*(pcmOut++) = pAAC->decode_buf[i + j];

				++totalPCMFramesRead;
    			pAAC->decode_buf_start += numChannels;

				if (totalPCMFramesRead == frameCount) goto DONE;
    		}

    		// buffer not full, decode another frame and continue
			result = decode_one_aac_frame(pAAC);
			if (result != MA_SUCCESS) {
    			break;
			}
        }
DONE:
        if (pFramesRead != NULL) {
            *pFramesRead = totalPCMFramesRead;
            pAAC->pcm_frame_cursor += totalPCMFramesRead; // TODO: is this off-by-one? not sure what exactly miniaudio expects this to mean
        }
        if (result == MA_SUCCESS && totalPCMFramesRead == 0) {
            result = MA_AT_END;
        }
        return result;
    }
    #else
    {
        /* fdkaac is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

// MA_API ma_result ma_fdkaac_seek_to_pcm_frame(ma_fdkaac* pAAC, ma_uint64 frameIndex)
// {
//     if (pAAC == NULL) {
//         return MA_INVALID_ARGS;
//     }

//     #if !defined(MA_NO_FDKACC)
//     {
//         // TODO: this may not be implemented correctly in the library itself (was a custom addition)
//         aacDecoder_SetBlockNumber(pAAC->handle, frameIndex);
//         return MA_SUCCESS;
//     }
//     #else
//     {
//         /* fdkaac is disabled. Should never hit this since initialization would have failed. */
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
        *pChannels = 2;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 48000;
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
            if (pAAC->info) *pChannels = pAAC->info->numChannels;
        }

        if (pSampleRate != NULL) {
            if (pAAC->info) *pSampleRate = pAAC->info->sampleRate;
        }

        if (pChannelMap != NULL) {
            ma_uint32 channelCount = *pChannels;
            for (ma_uint32 channelIndex = 0; channelIndex < channelCount; channelIndex += 1) {
                if (channelMapCap == 0) {
                    break;  /* Ran out of room. */
                }

                switch (*pChannels)
                {
                    case 0:
                        pChannelMap[0] = MA_CHANNEL_NONE;
                        break;

                    case 1:
                        pChannelMap[0] = MA_CHANNEL_MONO;
                        break;

                    case 2:
                    {
                        switch (channelIndex) {
                            case 0:
                                pChannelMap[0] = MA_CHANNEL_FRONT_LEFT;
                                break;
                            case 1:
                                pChannelMap[0] = MA_CHANNEL_FRONT_RIGHT;
                                break;
                            default:
                                pChannelMap[0] = MA_CHANNEL_NONE;
                                break;
                        }
                    } break;

                    default:
                        pChannelMap[0] = MA_CHANNEL_NONE;
                        break;
                }

                pChannelMap   += 1;
                channelMapCap -= 1;
            }
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* fdkaac is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_fdkaac_get_cursor_in_pcm_frames(ma_fdkaac* pAAC, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pAAC == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FDKACC)
    {
        // INT offset = aacDecoder_GetBlockNumber(pAAC->handle);
        *pCursor = (ma_uint64)pAAC->pcm_frame_cursor;
        return MA_SUCCESS;
    }
    #else
    {
        /* fdkaac is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_fdkaac_get_length_in_pcm_frames(ma_fdkaac* pAAC, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (!pAAC || !pAAC->st) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FDKACC)
    {
        // this is assuming nb_frames is the number of AAC frames -- which it seems to be, based on the sizes I'm seeing (9261 frames for a 3.5 min song)
        *pLength = (ma_uint64)pAAC->st->nb_frames * pAAC->info->frameSize;
        return MA_SUCCESS;
    }
    #else
    {
        /* fdkaac is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

/* PLUGIN FUNCTIONS */

MA_API ma_result ma_decoding_backend_init__fdkaac(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_fdkaac* pAAC;

    (void)pUserData;

    pAAC = (ma_fdkaac*)ma_malloc(sizeof(*pAAC), pAllocationCallbacks);
    if (pAAC == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_fdkaac_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pAAC);
    if (result != MA_SUCCESS) {
        ma_free(pAAC, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pAAC;

    return MA_SUCCESS;
}

MA_API ma_result ma_decoding_backend_init_file__fdkaac(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_fdkaac* pAAC;

    (void)pUserData;

    pAAC = (ma_fdkaac*)ma_malloc(sizeof(*pAAC), pAllocationCallbacks);
    if (pAAC == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_fdkaac_init_file(pFilePath, pConfig, pAllocationCallbacks, pAAC);
    if (result != MA_SUCCESS) {
        ma_free(pAAC, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pAAC;

    return MA_SUCCESS;
}

MA_API void ma_decoding_backend_uninit__fdkaac(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_fdkaac* pAAC = (ma_fdkaac*)pBackend;

    (void)pUserData;


	// TODO: call avformat_close_input here ?
    ma_fdkaac_uninit(pAAC, pAllocationCallbacks);
    ma_free(pAAC, pAllocationCallbacks);
}

// static ma_result ma_decoding_backend_get_channel_map__fdkaac(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
// {
//     ma_fdkaac* pAAC = (ma_fdkaac*)pBackend;

//     (void)pUserData;

//     return ma_fdkaac_get_data_format(pAAC, NULL, NULL, NULL, pChannelMap, channelMapCap);
// }

#endif
