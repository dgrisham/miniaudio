/*
Demonstrates how to implement a custom decoder.

This example implements two custom decoders:

  * Vorbis via libvorbis
  * Opus via libopus

A custom decoder must implement a data source. In this example, the libvorbis data source is called
`ma_libvorbis` and the Opus data source is called `ma_libopus`. These two objects are compatible
with the `ma_data_source` APIs and can be taken straight from this example and used in real code.

The custom decoding data sources (`ma_libvorbis` and `ma_libopus` in this example) are connected to
the decoder via the decoder config (`ma_decoder_config`). You need to implement a vtable for each
of your custom decoders. See `ma_decoding_backend_vtable` for the functions you need to implement.
The `onInitFile`, `onInitFileW` and `onInitMemory` functions are optional.
*/
// #define MA_DEBUG_OUTPUT 1
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include "../extras/miniaudio_fdkaac.h"

#include <stdio.h>

static ma_result ma_decoding_backend_init__fdkaac(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
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

static ma_result ma_decoding_backend_init_file__fdkaac(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
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

static void ma_decoding_backend_uninit__fdkaac(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_fdkaac* pAAC = (ma_fdkaac*)pBackend;

    (void)pUserData;

    ma_fdkaac_uninit(pAAC, pAllocationCallbacks);
    ma_free(pAAC, pAllocationCallbacks);
}

static ma_result ma_decoding_backend_get_channel_map__fdkaac(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_fdkaac* pAAC = (ma_fdkaac*)pBackend;

    (void)pUserData;

    return ma_fdkaac_get_data_format(pAAC, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_fdkaac =
{
    ma_decoding_backend_init__fdkaac,
    ma_decoding_backend_init_file__fdkaac,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoding_backend_uninit__fdkaac
};




void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == NULL) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, NULL);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority. The
    vtables will be passed in via the decoder config.
    */
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_fdkaac
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    /* Initialize the decoder. */
    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.pCustomBackendUserData = NULL;  /* In this example our backend objects are contained within a ma_decoder_ex object to avoid a malloc. Our vtables need to know about this. */
    decoderConfig.ppCustomBackendVTables = pCustomBackendVTables;
    decoderConfig.customBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);

    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.: %d\n", result);
        return -1;
    }

    ma_data_source_set_looping(&decoder, MA_TRUE);

    /* Initialize the device. */
    result = ma_data_source_get_data_format(&decoder, &format, &channels, &sampleRate, NULL, 0);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve decoder data format.");
        ma_decoder_uninit(&decoder);
        return -1;
    }
    result = ma_data_source_seek_to_pcm_frame(&decoder, 102400);

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format    = format;
    deviceConfig.playback.channels  = channels;
    deviceConfig.sampleRate         = sampleRate;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = &decoder;
    // deviceConfig.periodSizeInFrames = 1024;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open playback device.\n");
        ma_decoder_uninit(&decoder);
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return 0;
}
