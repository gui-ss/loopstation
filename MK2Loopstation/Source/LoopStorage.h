#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Two versions, with copy-on-write in fixed 64-frame stereo chunks. The small
// two-level index avoids copying either the entire phrase or its chunk table
// at a transport transition. All memory is allocated/touched in the constructor.
// Owned exclusively by the audio thread (or the stopped device lifecycle).
class LoopStorage final
{
public:
    explicit LoopStorage(int capacity)
        : frames(capacity), chunks((capacity + chunkSize - 1) / chunkSize),
          current((chunks + pageSize - 1) / pageSize, 0), previous(current),
          maps { std::vector<uint8_t>(current.size() * pageSize, 0),
                 std::vector<uint8_t>(current.size() * pageSize, 0) }
    {
        for (auto& bank : audio) { bank.setSize(2, chunks * chunkSize); bank.clear(); }
    }

    int capacity() const noexcept { return frames; }
    float get(int channel, int frame) const noexcept
    { return audio[bankAt(frame)].getSample(channel, frame); }
    void set(int frame, float left, float right) noexcept
    {
        const auto bank = bankAt(frame);
        audio[bank].setSample(0, frame, left);
        audio[bank].setSample(1, frame, right);
    }
    void snapshot() noexcept
    {
        // At the maximum supported size this copies only 704 bytes.
        std::copy(current.begin(), current.end(), previous.begin());
        preserving = true;
    }
    void restore() noexcept { current.swap(previous); preserving = false; }
    void discard() noexcept { preserving = false; }

    void makeWritable(int frame) noexcept
    {
        if (! preserving) return;
        const auto chunk = static_cast<size_t>(frame / chunkSize);
        const auto page = chunk / pageSize;
        auto& selectedPage = current[page];
        const auto savedPage = previous[page];
        if (selectedPage == savedPage)
        {
            selectedPage ^= 1;
            std::copy_n(maps[savedPage].data() + page * pageSize, pageSize,
                        maps[selectedPage].data() + page * pageSize); // 256 bytes
        }
        auto& bank = maps[selectedPage][chunk];
        if (bank == maps[savedPage][chunk])
        {
            const int start = static_cast<int>(chunk) * chunkSize;
            for (int ch = 0; ch < 2; ++ch)
                audio[bank ^ 1].copyFrom(ch, start, audio[bank], ch, start, chunkSize);
            bank ^= 1; // 512 bytes at most; subsequent writes need no copy.
        }
    }

private:
    size_t bankAt(int frame) const noexcept
    {
        const auto chunk = static_cast<size_t>(frame / chunkSize);
        return maps[current[chunk / pageSize]][chunk];
    }
    static constexpr int chunkSize = 64;
    static constexpr size_t pageSize = 256;
    int frames, chunks;
    std::vector<uint8_t> current, previous;
    std::array<std::vector<uint8_t>, 2> maps;
    std::array<juce::AudioBuffer<float>, 2> audio;
    bool preserving = false;
};
