// ============================================================
//  Wojcimierz Engine: audio/miniaudio/MiniaudioBackend.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
//
// Implementacja IAudioBackend na miniaudio (ma_engine + ma_sound).
// Jedyny plik silnika, ktory widzi miniaudio.h.

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#include <miniaudio.h>

#include "audio/IAudioBackend.h"

#include <cstdio>
#include <memory>
#include <unordered_map>

namespace wz {
namespace {

class MiniaudioBackend final : public IAudioBackend {
public:
    explicit MiniaudioBackend(const AudioBackendConfig& cfg) {
        ma_engine_config ec = ma_engine_config_init();
        ec.noDevice = cfg.noDevice ? MA_TRUE : MA_FALSE;
        ec.channels = cfg.channels;
        ec.sampleRate = cfg.sampleRate;
        ok_ = ma_engine_init(&ec, &engine_) == MA_SUCCESS;
        if (!ok_ && !cfg.noDevice) {
            // Brak karty dzwiekowej (serwer, CI) - przechodzimy w tryb bez urzadzenia.
            std::fprintf(stderr, "[Wojcimierz] audio: brak urzadzenia, tryb cichy\n");
            ec.noDevice = MA_TRUE;
            ok_ = ma_engine_init(&ec, &engine_) == MA_SUCCESS;
        }
        if (ok_) {
            channels_ = ma_engine_get_channels(&engine_);
            sampleRate_ = ma_engine_get_sample_rate(&engine_);
        }
    }

    ~MiniaudioBackend() override {
        for (auto& [id, snd] : voices_) ma_sound_uninit(snd.get());
        voices_.clear();
        if (ok_) ma_engine_uninit(&engine_);
    }

    AudioVoice CreateVoice(const std::string& path, bool streaming) override {
        if (!ok_) return kInvalidVoice;
        auto snd = std::make_unique<ma_sound>();
        const ma_uint32 flags = streaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
        if (ma_sound_init_from_file(&engine_, path.c_str(), flags, nullptr, nullptr, snd.get()) != MA_SUCCESS) {
            std::fprintf(stderr, "[Wojcimierz] audio: nie mozna wczytac '%s'\n", path.c_str());
            return kInvalidVoice;
        }
        const AudioVoice id = nextId_++;
        voices_.emplace(id, std::move(snd));
        return id;
    }

    void DestroyVoice(AudioVoice v) override {
        auto it = voices_.find(v);
        if (it == voices_.end()) return;
        ma_sound_uninit(it->second.get());
        voices_.erase(it);
    }

    void Play(AudioVoice v) override {
        if (ma_sound* s = Get(v)) {
            if (ma_sound_at_end(s)) ma_sound_seek_to_pcm_frame(s, 0);
            ma_sound_start(s);
        }
    }
    void Pause(AudioVoice v) override { if (ma_sound* s = Get(v)) ma_sound_stop(s); }
    void Stop(AudioVoice v) override {
        if (ma_sound* s = Get(v)) {
            ma_sound_stop(s);
            ma_sound_seek_to_pcm_frame(s, 0);
        }
    }
    bool IsPlaying(AudioVoice v) const override {
        auto it = voices_.find(v);
        return it != voices_.end() && ma_sound_is_playing(it->second.get());
    }

    void SetVolume(AudioVoice v, float vol) override { if (ma_sound* s = Get(v)) ma_sound_set_volume(s, vol); }
    void SetPitch(AudioVoice v, float p) override { if (ma_sound* s = Get(v)) ma_sound_set_pitch(s, p); }
    void SetLooping(AudioVoice v, bool loop) override { if (ma_sound* s = Get(v)) ma_sound_set_looping(s, loop); }
    void SetSpatial(AudioVoice v, bool spatial, float minD, float maxD) override {
        if (ma_sound* s = Get(v)) {
            ma_sound_set_spatialization_enabled(s, spatial ? MA_TRUE : MA_FALSE);
            ma_sound_set_min_distance(s, minD);
            ma_sound_set_max_distance(s, maxD);
        }
    }
    void SetVoicePosition(AudioVoice v, const glm::vec3& p) override {
        if (ma_sound* s = Get(v)) ma_sound_set_position(s, p.x, p.y, p.z);
    }

    void SetListener(const glm::vec3& p, const glm::vec3& f, const glm::vec3& u) override {
        if (!ok_) return;
        ma_engine_listener_set_position(&engine_, 0, p.x, p.y, p.z);
        ma_engine_listener_set_direction(&engine_, 0, f.x, f.y, f.z);
        ma_engine_listener_set_world_up(&engine_, 0, u.x, u.y, u.z);
    }
    void SetMasterVolume(float vol) override { if (ok_) ma_engine_set_volume(&engine_, vol); }

    std::uint64_t ReadMix(float* out, std::uint64_t frames) override {
        if (!ok_) return 0;
        ma_uint64 read = 0;
        ma_engine_read_pcm_frames(&engine_, out, frames, &read);
        return read;
    }
    std::uint32_t GetChannels() const override { return channels_; }
    std::uint32_t GetSampleRate() const override { return sampleRate_; }

private:
    ma_sound* Get(AudioVoice v) {
        auto it = voices_.find(v);
        return it == voices_.end() ? nullptr : it->second.get();
    }

    ma_engine engine_{};
    bool ok_ = false;
    std::uint32_t channels_ = 0;
    std::uint32_t sampleRate_ = 0;
    std::unordered_map<AudioVoice, std::unique_ptr<ma_sound>> voices_;
    AudioVoice nextId_ = 1;
};

} // namespace

// Na chwałę czcigodnego Wojcimierza: tu silnik wybiera, czym zabrzmi.
std::unique_ptr<IAudioBackend> CreateMiniaudioBackend(const AudioBackendConfig& config) {
    return std::make_unique<MiniaudioBackend>(config);
}

} // namespace wz
