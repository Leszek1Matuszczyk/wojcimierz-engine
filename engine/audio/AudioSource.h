// ============================================================
//  Wojcimierz Engine: audio/AudioSource.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "audio/IAudioBackend.h"
#include "scene/Component.h"

#include <string>

namespace wz {

class AudioSystem;

// Odpowiednik UnityEngine.AudioSource. Pola sa publiczne pod REFLECT;
// AudioSystem przenosi ich zmiany do backendu co klatke.
class AudioSource final : public Component {
public:
    std::string clip;          // sciezka do pliku (pozniej: GUID assetu)
    float volume = 1.0f;       // przycinane do [0, 1]; NaN/inf -> poprzednia poprawna wartość
    float pitch = 1.0f;
    bool loop = false;
    bool playOnAwake = true;   // start przy pierwszym pojawieniu sie w AudioSystem
    bool spatial = true;       // Unity: spatialBlend = 1 (3D) / 0 (2D)
    bool streaming = false;    // dlugie pliki (muzyka) strumieniowane z dysku
    float minDistance = 1.0f;
    float maxDistance = 500.0f;

    void Play();
    void Pause();
    void Stop();
    bool IsPlaying() const;

    void OnDestroy() override;

private:
    friend class AudioSystem;
    enum class Request { None, Play, Pause, Stop };
    Request pending_ = Request::None; // zadanie zlozone przed powstaniem glosu
    AudioSystem* system_ = nullptr;
    AudioVoice voice_ = kInvalidVoice;
    std::string loadedClip_;
    bool loadedStreaming_ = false;
    // Po porazce nie ladujemy ponownie, dopoki clip/streaming sie nie zmieni.
    // Uwaga (hot-reload): plik dograny pozniej pod ta sama sciezka nie zaladuje sie
    // sam - trzeba przypisac clip ponownie albo przebudowac komponent.
    // Ostatnia poprawna głośność. Przy NaN zostajemy przy niej, żeby nic nagle nie huknęło.
    // Na start obowiązuje domyślne 1.0 pola volume. Wojcimierz ceni sobie spokój.
    float lastVolume_ = 1.0f;
    bool loadAttempted_ = false;
    bool awakeHandled_ = false;
    bool wasDisabled_ = false; // po ponownej aktywacji playOnAwake gra znowu (Unity: OnEnable)
};

// Odpowiednik UnityEngine.AudioListener ("uszy" - zwykle na kamerze).
// Uzywany jest pierwszy aktywny listener w scenie.
class AudioListener final : public Component {};

} // namespace wz
