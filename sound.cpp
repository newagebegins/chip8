#include "sound.h"
#include "debug.h"

#include <math.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#define assert(expression) if(!(expression)) *(int *)0 = 0;

static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
static const IID IID_IAudioClient = __uuidof(IAudioClient);
static const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define PI 3.14159265359f
#define TWOPI (2.0f*PI)
#define REFTIMES_PER_SEC 10000000

static IAudioClient *audioClient;
static IAudioRenderClient *renderClient;
static const int samplesPerSec = 48000;
static const float maxBufferDurationSec = (1.0f/60.0f)*2.0f;
static UINT32 bufferFramesCount;

void sound_init() {
    HRESULT hr;

    IMMDeviceEnumerator *enumerator;
    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&enumerator);
    assert(SUCCEEDED(hr));

    IMMDevice *device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    assert(SUCCEEDED(hr));

    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient);
    assert(SUCCEEDED(hr));

    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = samplesPerSec;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

    REFERENCE_TIME duration = maxBufferDurationSec*REFTIMES_PER_SEC;
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, duration, 0, &waveFormat, NULL);
    assert(SUCCEEDED(hr));

    hr = audioClient->GetBufferSize(&bufferFramesCount);
    assert(SUCCEEDED(hr));

    hr = audioClient->GetService(IID_IAudioRenderClient, (void**)&renderClient);
    assert(SUCCEEDED(hr));
}

static float frequency = 440.0f;
float phase = 0;

void load_sound() {
    HRESULT hr;

    UINT32 numFramesPadding;
    hr = audioClient->GetCurrentPadding(&numFramesPadding);
    assert(SUCCEEDED(hr));

    UINT32 numFramesAvailable = bufferFramesCount - numFramesPadding;

    short *buffer;
    hr = renderClient->GetBuffer(numFramesAvailable, (BYTE**)&buffer);
    assert(SUCCEEDED(hr));

    float incr = TWOPI*frequency / samplesPerSec;
    
    for (int i = 0; i < numFramesAvailable; ++i) {
        buffer[i] = sinf(phase) * 0x7FFF;
        phase += incr;
        if (phase >= TWOPI) phase -= TWOPI;
    }

    hr = renderClient->ReleaseBuffer(numFramesAvailable, 0);
    assert(SUCCEEDED(hr));
}

void sound_start() {
    debug_log("sound_start\n");
    phase = 0;
    load_sound();
    HRESULT hr = audioClient->Start();
    assert(SUCCEEDED(hr));
}

void sound_stop() {
    debug_log("sound_stop\n");
    HRESULT hr = audioClient->Stop();
    assert(SUCCEEDED(hr));
    hr = audioClient->Reset();
    assert(SUCCEEDED(hr));
}

void sound_update() {
    load_sound();
}
