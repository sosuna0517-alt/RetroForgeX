#pragma once
// AudioOutput: XAudio2 sink for libretro's interleaved int16 stereo audio.
// Samples pushed during retro_run() are submitted as one buffer per frame.

namespace rfx
{
    class AudioOutput
    {
    public:
        bool Initialize();
        void Shutdown();

        void Configure(double sampleRate);
        void Push(const int16_t* interleavedStereo, size_t frames);
        void EndFrame();
        void Flush();
        void SetPaused(bool paused);

    private:
        static constexpr size_t kBufferCount = 8;

        winrt::com_ptr<IXAudio2> m_xaudio;
        IXAudio2MasteringVoice* m_master = nullptr;
        IXAudio2SourceVoice* m_source = nullptr;
        uint32_t m_sampleRate = 0;
        bool m_started = false;
        bool m_paused = false;

        std::array<std::vector<int16_t>, kBufferCount> m_buffers;
        size_t m_next = 0;
        std::vector<int16_t> m_pending;
    };
}
