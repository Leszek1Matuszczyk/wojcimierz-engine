// ============================================================
//  Wojcimierz Engine: audio/AudioSystem.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "audio/IAudioBackend.h"

#include <memory>
#include <unordered_set>

namespace wz {

class Scene;
class AudioSource;

// System audio przypiety do sceny. Wolany raz na klatke, po LateUpdate:
//   audio.Update();
// Tworzy glosy dla nowych AudioSource, przenosi ich pola i pozycje do backendu
// (pozycja tylko gdy Transform::GetChangeVersion() sie zmienil) i ustawia
// listenera z pierwszego aktywnego AudioListener.
class AudioSystem {
public:
    explicit AudioSystem(Scene& scene, std::unique_ptr<IAudioBackend> backend = nullptr);
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    static AudioSystem* For(const Scene& scene);
    IAudioBackend& GetBackend() const { return *backend_; }

    void Update();
    void SetMasterVolume(float v) { backend_->SetMasterVolume(v); }

private:
    friend class AudioSource;
    void Release(AudioSource& src);
    void Apply(AudioSource& src);

    Scene& scene_;
    std::unique_ptr<IAudioBackend> backend_;
    std::unordered_set<AudioSource*> sources_;
};

} // namespace wz
