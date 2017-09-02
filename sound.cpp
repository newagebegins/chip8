#include "sound.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include <assert.h>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define PI 3.14159265359f
#define TWOPI (2.0f*PI)
#define REFTIMES_PER_SEC 10000000

IAudioClient *audioClient;
IAudioRenderClient *renderClient;
const int samplesPerSec = 48000;
const float maxBufferDurationSec = (1.0f / 60.0f)*2.0f;
UINT32 bufferFramesCount;

float frequency = 440.0f;
float phase = 0;
bool isPlaying = false;
#define MAX_AMPLITUDE 0x7FFF/20
short amplitude = 0;
short amplitudeStep = 1;

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

    REFERENCE_TIME duration = (REFERENCE_TIME)(maxBufferDurationSec*REFTIMES_PER_SEC);
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, duration, 0, &waveFormat, NULL);
    assert(SUCCEEDED(hr));

    hr = audioClient->GetBufferSize(&bufferFramesCount);
    assert(SUCCEEDED(hr));

    hr = audioClient->GetService(IID_IAudioRenderClient, (void**)&renderClient);
    assert(SUCCEEDED(hr));

    sound_update();
    audioClient->Start();
}

void sound_start() {
    isPlaying = true;
}

void sound_stop() {
    isPlaying = false;
}

void sound_update() {
    HRESULT hr;

    UINT32 numFramesPadding;
    hr = audioClient->GetCurrentPadding(&numFramesPadding);
    assert(SUCCEEDED(hr));

    UINT32 numFramesAvailable = bufferFramesCount - numFramesPadding;

    short *buffer;
    hr = renderClient->GetBuffer(numFramesAvailable, (BYTE**)&buffer);
    assert(SUCCEEDED(hr));

    float incr = TWOPI*frequency / samplesPerSec;

    for (UINT32 i = 0; i < numFramesAvailable; ++i) {
        buffer[i] = (short)(sinf(phase) * amplitude);
        phase += incr;
        if (phase >= TWOPI) phase -= TWOPI;
        if (isPlaying) {
            amplitude += amplitudeStep;
            if (amplitude > MAX_AMPLITUDE) {
                amplitude = MAX_AMPLITUDE;
            }
        }
        else {
            amplitude -= amplitudeStep;
            if (amplitude < 0) {
                amplitude = 0;
            }
        }
    }

    hr = renderClient->ReleaseBuffer(numFramesAvailable, 0);
    assert(SUCCEEDED(hr));
}
