// ============================================================
//  Wojcimierz Engine: audio/IAudioBackend.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace wz {

using AudioVoice = std::uint32_t;
inline constexpr AudioVoice kInvalidVoice = 0;

struct AudioBackendConfig {
    // Bez urzadzenia: dzwiek nie idzie na glosniki, miksujemy recznie przez
    // ReadMix(). Uzywane w testach i CI (serwery bez karty dzwiekowej).
    bool noDevice = false;
    std::uint32_t sampleRate = 48000;
    std::uint32_t channels = 2;
};

// Abstrakcyjny backend audio. Gameplay widzi tylko AudioSource/AudioListener.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // Tworzy glos odtwarzajacy plik (WAV/MP3/FLAC). kInvalidVoice przy bledzie.
    virtual AudioVoice CreateVoice(const std::string& path, bool streaming) = 0;
    virtual void DestroyVoice(AudioVoice v) = 0;

    virtual void Play(AudioVoice v) = 0;  // od biezacej pozycji (po Stop - od poczatku)
    virtual void Pause(AudioVoice v) = 0;
    virtual void Stop(AudioVoice v) = 0;  // zatrzymanie i przewiniecie na poczatek
    virtual bool IsPlaying(AudioVoice v) const = 0;

    virtual void SetVolume(AudioVoice v, float volume) = 0;
    virtual void SetPitch(AudioVoice v, float pitch) = 0;
    virtual void SetLooping(AudioVoice v, bool loop) = 0;
    virtual void SetSpatial(AudioVoice v, bool spatial, float minDistance, float maxDistance) = 0;
    virtual void SetVoicePosition(AudioVoice v, const glm::vec3& p) = 0;

    virtual void SetListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) = 0;
    virtual void SetMasterVolume(float volume) = 0;

    // Tylko w trybie noDevice: miksuje 'frames' ramek do 'out' (przeplatane float).
    virtual std::uint64_t ReadMix(float* out, std::uint64_t frames) = 0;
    virtual std::uint32_t GetChannels() const = 0;
    virtual std::uint32_t GetSampleRate() const = 0;
};

std::unique_ptr<IAudioBackend> CreateMiniaudioBackend(const AudioBackendConfig& config = {});

} // namespace wz
