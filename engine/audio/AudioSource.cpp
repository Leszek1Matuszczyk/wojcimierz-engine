// ============================================================
//  Wojcimierz Engine: audio/AudioSource.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "audio/AudioSource.h"

#include "audio/AudioSystem.h"

namespace wz {

void AudioSource::Play() {
    if (system_ && voice_ != kInvalidVoice) system_->GetBackend().Play(voice_);
    else pending_ = Request::Play;
}

void AudioSource::Pause() {
    if (system_ && voice_ != kInvalidVoice) system_->GetBackend().Pause(voice_);
    else pending_ = Request::Pause;
}

void AudioSource::Stop() {
    if (system_ && voice_ != kInvalidVoice) system_->GetBackend().Stop(voice_);
    else pending_ = Request::Stop;
}

bool AudioSource::IsPlaying() const {
    if (system_ && voice_ != kInvalidVoice) return system_->GetBackend().IsPlaying(voice_);
    // Bez glosu "gra" tylko odroczony Play z niepustym klipem.
    return pending_ == Request::Play && !clip.empty();
}

void AudioSource::OnDestroy() {
    if (auto* s = AudioSystem::For(scene())) s->Release(*this);
}

// Rejestracja pol w REFLECT dojdzie po merge #5 (rdzen: UUID, ComponentRegistry, REFLECT).

} // namespace wz
