#include "sound.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include <assert.h>

static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
static const IID IID_IAudioClient = __uuidof(IAudioClient);
static const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define PI 3.14159265359f
#define TWO_PI (2.0f*PI)
#define REFTIMES_PER_SEC 10000000
#define SAMPLES_PER_SEC 48000
#define MAX_BUFFER_DURATION_SEC ((1.0f / 60.0f)*2.0f)
#define MAX_AMPLITUDE 0x7FFFFFFF
#define TONE_FREQUENCY 440.0f
#define PHASE_INCREMENT (TWO_PI*TONE_FREQUENCY / SAMPLES_PER_SEC)

static IAudioClient *audio_client;
static IAudioRenderClient *render_client;
static UINT32 buffer_frames_count;

static bool playing;
static float phase;
static float amplitude;

void sound_init() {
    HRESULT hr;

    IMMDeviceEnumerator *enumerator;
    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
    assert(SUCCEEDED(hr));

    IMMDevice *device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    assert(SUCCEEDED(hr));

    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audio_client);
    assert(SUCCEEDED(hr));

    WAVEFORMATEX wave_format = { 0 };
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = SAMPLES_PER_SEC;
    wave_format.wBitsPerSample = 32;
    wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

    REFERENCE_TIME duration = (REFERENCE_TIME)(MAX_BUFFER_DURATION_SEC*REFTIMES_PER_SEC);
    hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, duration, 0, &wave_format, NULL);
    assert(SUCCEEDED(hr));

    hr = audio_client->GetBufferSize(&buffer_frames_count);
    assert(SUCCEEDED(hr));

    hr = audio_client->GetService(IID_IAudioRenderClient, (void**)&render_client);
    assert(SUCCEEDED(hr));

    sound_update();
    audio_client->Start();
}

void sound_start() {
    playing = true;
}

void sound_stop() {
    playing = false;
}

void sound_update() {
    HRESULT hr;

    UINT32 padding_frames_count;
    hr = audio_client->GetCurrentPadding(&padding_frames_count);
    assert(SUCCEEDED(hr));

    UINT32 available_frames_count = buffer_frames_count - padding_frames_count;

    BYTE *buffer;
    hr = render_client->GetBuffer(available_frames_count, &buffer);
    assert(SUCCEEDED(hr));

    const float fade_time = 0.25f;
    const float fade_frames = SAMPLES_PER_SEC * fade_time;
    const float amplitude_step = 1.0f / fade_frames;
    int num_bytes = 4;

    for (UINT32 frame = 0, b = 0; frame < available_frames_count; ++frame) {
        INT32 val = sinf(phase) * amplitude * MAX_AMPLITUDE;
        for (int channel = 0; channel < 2; ++channel)
            for (int byte = 0; byte < num_bytes; ++byte)
                buffer[b++] = (val >> (byte * 8)) & 0xFF;
        
        phase += PHASE_INCREMENT;
        if (phase >= TWO_PI) phase -= TWO_PI;

        // fade in/out
        if (playing) {
            amplitude += amplitude_step;
            if (amplitude > 1.0f) amplitude = 1.0f;
        }
        else {
            amplitude -= amplitude_step;
            if (amplitude < 0) amplitude = 0;
        }
    }

    hr = render_client->ReleaseBuffer(available_frames_count, 0);
    assert(SUCCEEDED(hr));
}
