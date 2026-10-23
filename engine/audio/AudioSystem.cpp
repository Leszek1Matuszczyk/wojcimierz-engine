// ============================================================
//  Wojcimierz Engine: audio/AudioSystem.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "audio/AudioSystem.h"

#include "audio/AudioSource.h"
#include "audio/detail/Finite.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace wz {

namespace {
std::mutex gRegistryMutex;
std::unordered_map<const Scene*, AudioSystem*> gRegistry;
} // namespace

AudioSystem::AudioSystem(Scene& scene, std::unique_ptr<IAudioBackend> backend)
    : scene_(scene), backend_(backend ? std::move(backend) : CreateMiniaudioBackend()) {
    std::lock_guard lock(gRegistryMutex);
    gRegistry[&scene_] = this;
}

AudioSystem::~AudioSystem() {
    {
        std::lock_guard lock(gRegistryMutex);
        auto it = gRegistry.find(&scene_);
        if (it != gRegistry.end() && it->second == this) gRegistry.erase(it);
    }
    for (AudioSource* s : sources_) {
        if (s->voice_ != kInvalidVoice) backend_->DestroyVoice(s->voice_);
        s->voice_ = kInvalidVoice;
        s->system_ = nullptr;
        s->loadedClip_.clear();
        s->loadAttempted_ = false;
    }
}

AudioSystem* AudioSystem::For(const Scene& scene) {
    std::lock_guard lock(gRegistryMutex);
    auto it = gRegistry.find(&scene);
    return it == gRegistry.end() ? nullptr : it->second;
}

void AudioSystem::Release(AudioSource& src) {
    if (src.voice_ != kInvalidVoice) backend_->DestroyVoice(src.voice_);
    src.voice_ = kInvalidVoice;
    src.system_ = nullptr;
    src.loadedClip_.clear();
    src.loadAttempted_ = false;
    sources_.erase(&src);
}

void AudioSystem::Apply(AudioSource& src) {
    // Zmiana klipu (albo trybu strumieniowania) -> nowy glos. Nieudane wczytanie
    // zapamietujemy (loadAttempted_), zeby nie probowac co klatke.
    if (!src.loadAttempted_ || src.loadedClip_ != src.clip || src.loadedStreaming_ != src.streaming) {
        const bool wasPlaying = src.voice_ != kInvalidVoice && backend_->IsPlaying(src.voice_);
        if (src.voice_ != kInvalidVoice) backend_->DestroyVoice(src.voice_);
        src.voice_ = src.clip.empty() ? kInvalidVoice : backend_->CreateVoice(src.clip, src.streaming);
        src.loadedClip_ = src.clip;
        src.loadedStreaming_ = src.streaming;
        src.loadAttempted_ = true;
        if (wasPlaying && src.pending_ == AudioSource::Request::None) src.pending_ = AudioSource::Request::Play;
    }
    if (src.voice_ == kInvalidVoice) {
        // Pusty albo nieudany klip: zadania przepadaja, IsPlaying() == false.
        src.pending_ = AudioSource::Request::None;
        src.awakeHandled_ = true;
        return;
    }

    // Na chwałę czcigodnego Wojcimierza: jeden NaN w głośności zamieniłby cały miks
    // w NaN, więc każdą liczbę z gameplayu oczyszczamy przed backendem.
    using namespace audio_detail;
    const float maxD = std::max(SanitizeFloat(src.maxDistance, 500.0f), 1e-3f);
    const float minD = std::clamp(SanitizeFloat(src.minDistance, 1.0f), 1e-3f, maxD);
    src.lastVolume_ = std::clamp(SanitizeFloat(src.volume, src.lastVolume_), 0.0f, 1.0f);
    backend_->SetVolume(src.voice_, src.lastVolume_);
    backend_->SetPitch(src.voice_, std::clamp(SanitizeFloat(src.pitch, 1.0f), 0.01f, 100.0f));
    backend_->SetLooping(src.voice_, src.loop);
    backend_->SetSpatial(src.voice_, src.spatial, minD, maxD);
    const glm::vec3 pos = src.transform().GetPosition();
    if (src.spatial && IsFinite(pos)) backend_->SetVoicePosition(src.voice_, pos);

    // Na chwałę czcigodnego Wojcimierza, który uczy cierpliwości: zadania złożone,
    // zanim głos istniał, wykonujemy dopiero teraz.
    switch (src.pending_) {
    case AudioSource::Request::Play: backend_->Play(src.voice_); break;
    case AudioSource::Request::Pause: backend_->Pause(src.voice_); break;
    case AudioSource::Request::Stop: backend_->Stop(src.voice_); break;
    case AudioSource::Request::None:
        if (!src.awakeHandled_ && src.playOnAwake) backend_->Play(src.voice_);
        break;
    }
    src.pending_ = AudioSource::Request::None;
    src.awakeHandled_ = true;
}

void AudioSystem::SetMasterVolume(float v) {
    if (!audio_detail::IsFinite(v)) {
        std::fprintf(stderr, "[Wojcimierz] audio: SetMasterVolume dostal NaN/inf - odrzucone\n");
        return;
    }
    backend_->SetMasterVolume(std::max(v, 0.0f));
}

void AudioSystem::Update() {
    if (AudioListener* l = scene_.FindFirst<AudioListener>()) {
        const Transform& t = l->transform();
        if (audio_detail::IsFinite(t.GetPosition()) && audio_detail::IsFinite(t.Forward()) &&
            audio_detail::IsFinite(t.Up()))
            backend_->SetListener(t.GetPosition(), t.Forward(), t.Up());
    }

    std::unordered_set<AudioSource*> seen;
    scene_.ForEach<AudioSource>([&](AudioSource& src) {
        src.system_ = this;
        if (src.wasDisabled_) {
            src.wasDisabled_ = false;
            src.awakeHandled_ = false; // ponowna aktywacja: playOnAwake jeszcze raz
        }
        sources_.insert(&src);
        seen.insert(&src);
        Apply(src);
    });

    // Wylaczone albo nieaktywne zrodla milkna (jak w Unity: OnDisable zatrzymuje dzwiek).
    for (AudioSource* s : sources_) {
        if (seen.count(s)) continue;
        s->wasDisabled_ = true;
        if (s->voice_ != kInvalidVoice && backend_->IsPlaying(s->voice_)) backend_->Stop(s->voice_);
    }
}

} // namespace wz
