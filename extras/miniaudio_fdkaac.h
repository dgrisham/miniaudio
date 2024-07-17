/*
This implements a data source that decodes HE-AAC streams via uuac

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/
#ifndef miniaudio_fdkaac_h
#define miniaudio_fdkaac_h

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_FDKACC)
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include <libavformat/avformat.h>
#include <fdk-aac/aacdecoder_lib.h>
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
	int fsize;

    INT_PCM *decode_buf;
    int decode_buf_size; // total size of the decode buffer
    int decode_buf_start; // index of the first unprocessed pcm value in the decode buffer

    uint64_t pcm_frame_cursor;

    CStreamInfo *info;
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
