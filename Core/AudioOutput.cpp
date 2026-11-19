#include "pch.h"
#include "AudioOutput.h"

namespace rfx
{
    bool AudioOutput::Initialize()
    {
        if (FAILED(XAudio2Create(m_xaudio.put(), 0, XAUDIO2_DEFAULT_PROCESSOR)))
        {
            return false;
        }
        if (FAILED(m_xaudio->CreateMasteringVoice(&m_master)))
        {
            m_xaudio = nullptr;
            return false;
        }
        return true;
    }

    void AudioOutput::Shutdown()
    {
        if (m_source)
        {
            m_source->DestroyVoice();
            m_source = nullptr;
        }
        if (m_master)
        {
            m_master->DestroyVoice();
            m_master = nullptr;
        }
        m_xaudio = nullptr;
    }

    void AudioOutput::Configure(double sampleRate)
    {
        if (!m_xaudio)
        {
            return;
        }
        const uint32_t rate = static_cast<uint32_t>(std::lround(sampleRate > 0.0 ? sampleRate : 44100.0));
        if (m_source && rate == m_sampleRate)
        {
            return;
        }
        if (m_source)
        {
            m_source->DestroyVoice();
            m_source = nullptr;
        }

        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 2;
        wf.nSamplesPerSec = rate;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = static_cast<WORD>(wf.nChannels * wf.wBitsPerSample / 8);
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

        if (FAILED(m_xaudio->CreateSourceVoice(&m_source, &wf, 0, XAUDIO2_MAX_FREQ_RATIO)))
        {
            m_source = nullptr;
            return;
        }
        m_sampleRate = rate;
        m_started = false;
        m_pending.clear();
    }

    void AudioOutput::Push(const int16_t* interleavedStereo, size_t frames)
    {
        m_pending.insert(m_pending.end(), interleavedStereo, interleavedStereo + frames * 2);
    }

    void AudioOutput::EndFrame()
    {
        if (!m_source || m_pending.empty())
        {
            m_pending.clear();
            return;
        }

        XAUDIO2_VOICE_STATE state{};
        m_source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued >= kBufferCount - 1)
        {
            // Running ahead of the audio clock: drop this frame's audio rather than add latency.
            m_pending.clear();
            return;
        }

        auto& buffer = m_buffers[m_next];
        buffer.swap(m_pending);
        m_pending.clear();
        m_next = (m_next + 1) % kBufferCount;

        XAUDIO2_BUFFER xb{};
        xb.AudioBytes = static_cast<UINT32>(buffer.size() * sizeof(int16_t));
        xb.pAudioData = reinterpret_cast<const BYTE*>(buffer.data());
        m_source->SubmitSourceBuffer(&xb);

        // Start once a little audio is queued to avoid an initial underrun.
        if (!m_started && !m_paused && state.BuffersQueued + 1 >= 2)
        {
            m_source->Start();
            m_started = true;
        }
    }

    void AudioOutput::Flush()
    {
        if (m_source)
        {
            m_source->Stop();
            m_source->FlushSourceBuffers();
            m_started = false;
        }
        m_pending.clear();
    }

    void AudioOutput::SetPaused(bool paused)
    {
        if (paused == m_paused)
        {
            return;
        }
        m_paused = paused;
        if (!m_source)
        {
            return;
        }
        if (paused)
        {
            m_source->Stop();
            m_started = false;
        }
    }
}
