// ============================================================
//  Wojcimierz Engine: tests/nonfinite/test_nonfinite_audio.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// AudioSource (pola REFLECT) i AudioSystem::SetMasterVolume dostaja NaN / +-Inf.
// Dwa poziomy:
//  1) backend nagrywajacy: zaden SetVolume/SetPitch/SetSpatial/SetMasterVolume
//     nie moze dostac nieskonczonej liczby (lapie kazde pole bez walidacji),
//  2) prawdziwy miniaudio w trybie noDevice: miks zostaje skonczony, a zdrowe
//     zrodlo nadal gra (B1 z review #9: jeden NaN wyciszal cala scene).
#include "NonFiniteUtils.h"
#include "audio/AudioSource.h"
#include "audio/AudioSystem.h"
#include "scene/Scene.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

using namespace wz;
using namespace wztest;

namespace {

constexpr std::uint32_t kRate = 48000;

std::string WriteSineWav(float seconds) {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string name = std::string("wz_nf_") + info->test_suite_name() + "_" + info->name() + ".wav";
    std::string safe = name;
    for (auto& ch : safe)
        if (ch == '/') ch = '_'; // testy parametryzowane maja '/' w nazwie
    const std::string path = (std::filesystem::temp_directory_path() / safe).string();
    const std::uint32_t n = std::uint32_t(seconds * kRate);
    std::vector<std::int16_t> pcm(n);
    for (std::uint32_t i = 0; i < n; ++i)
        pcm[i] = std::int16_t(0.5f * 32767.0f * std::sin(2.0f * 3.14159265f * 440.0f * float(i) / kRate));
    FILE* f = std::fopen(path.c_str(), "wb");
    auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    const std::uint32_t dataBytes = n * 2;
    std::fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    std::fwrite(pcm.data(), 2, n, f);
    std::fclose(f);
    return path;
}

std::unique_ptr<IAudioBackend> Silent() {
    AudioBackendConfig c;
    c.noDevice = true;
    c.sampleRate = kRate;
    c.channels = 2;
    return CreateMiniaudioBackend(c);
}

// Dekorator na prawdziwym backendzie: przepuszcza wszystko, ale zapamietuje
// kazda liczbe, ktora system audio probowal wyslac dalej.
class RecordingBackend final : public IAudioBackend {
public:
    explicit RecordingBackend(std::unique_ptr<IAudioBackend> inner) : inner_(std::move(inner)) {}
    std::vector<std::string> bad; // opis kazdej nieskonczonej wartosci

    AudioVoice CreateVoice(const std::string& p, bool s) override { return inner_->CreateVoice(p, s); }
    void DestroyVoice(AudioVoice v) override { inner_->DestroyVoice(v); }
    void Play(AudioVoice v) override { inner_->Play(v); }
    void Pause(AudioVoice v) override { inner_->Pause(v); }
    void Stop(AudioVoice v) override { inner_->Stop(v); }
    bool IsPlaying(AudioVoice v) const override { return inner_->IsPlaying(v); }
    void SetVolume(AudioVoice v, float x) override { Check("SetVolume", x); inner_->SetVolume(v, x); }
    void SetPitch(AudioVoice v, float x) override { Check("SetPitch", x); inner_->SetPitch(v, x); }
    void SetLooping(AudioVoice v, bool l) override { inner_->SetLooping(v, l); }
    void SetSpatial(AudioVoice v, bool s, float mn, float mx) override {
        Check("SetSpatial.min", mn);
        Check("SetSpatial.max", mx);
        inner_->SetSpatial(v, s, mn, mx);
    }
    void SetVoicePosition(AudioVoice v, const glm::vec3& p) override {
        if (!Finite(p)) bad.push_back("SetVoicePosition");
        inner_->SetVoicePosition(v, p);
    }
    void SetListener(const glm::vec3& p, const glm::vec3& f, const glm::vec3& u) override {
        if (!Finite(p) || !Finite(f) || !Finite(u)) bad.push_back("SetListener");
        inner_->SetListener(p, f, u);
    }
    void SetMasterVolume(float x) override { Check("SetMasterVolume", x); inner_->SetMasterVolume(x); }
    std::uint64_t ReadMix(float* out, std::uint64_t frames) override { return inner_->ReadMix(out, frames); }
    std::uint32_t GetChannels() const override { return inner_->GetChannels(); }
    std::uint32_t GetSampleRate() const override { return inner_->GetSampleRate(); }

private:
    void Check(const char* what, float x) {
        if (!std::isfinite(x)) bad.push_back(std::string(what) + "=" + BadName(x));
    }
    std::unique_ptr<IAudioBackend> inner_;
};

struct MixStats {
    bool finite = true;
    float rms = 0.0f;
};

MixStats Mix(AudioSystem& a, std::uint64_t frames = 4800) {
    std::vector<float> buf(frames * a.GetBackend().GetChannels());
    a.GetBackend().ReadMix(buf.data(), frames);
    MixStats st;
    double sum = 0;
    for (float s : buf) {
        if (!std::isfinite(s)) st.finite = false;
        else sum += double(s) * s;
    }
    st.rms = float(std::sqrt(sum / double(buf.size())));
    return st;
}

using Poison = void (*)(AudioSource&, float);
struct Field {
    const char* name;
    Poison poison;
};
const Field kFields[] = {
    {"volume", [](AudioSource& s, float b) { s.volume = b; }},
    {"pitch", [](AudioSource& s, float b) { s.pitch = b; }},
    {"minDistance", [](AudioSource& s, float b) { s.minDistance = b; }},
    {"maxDistance", [](AudioSource& s, float b) { s.maxDistance = b; }},
};

class AudioNonFinite : public ::testing::TestWithParam<float> {};

TEST_P(AudioNonFinite, BackendNeverReceivesNonFiniteSourceField) {
    const auto wav = WriteSineWav(0.5f);
    for (const auto& f : kFields)
        for (bool spatial : {false, true}) {
            SCOPED_TRACE(std::string(f.name) + (spatial ? " 3D" : " 2D"));
            Scene s;
            auto rec = std::make_unique<RecordingBackend>(Silent());
            auto* r = rec.get();
            AudioSystem audio(s, std::move(rec));
            auto& src = s.CreateGameObject().AddComponent<AudioSource>();
            src.clip = wav;
            src.spatial = spatial;
            audio.Update();             // glos utworzony z poprawnymi polami
            f.poison(src, GetParam());  // zmiana w trakcie grania
            audio.Update();
            EXPECT_TRUE(r->bad.empty()) << "pierwsza zla wartosc: " << (r->bad.empty() ? "" : r->bad.front());
        }
}

TEST_P(AudioNonFinite, BackendNeverReceivesNonFiniteMasterVolume) {
    Scene s;
    auto rec = std::make_unique<RecordingBackend>(Silent());
    auto* r = rec.get();
    AudioSystem audio(s, std::move(rec));
    audio.SetMasterVolume(GetParam());
    EXPECT_TRUE(r->bad.empty()) << (r->bad.empty() ? "" : r->bad.front());
}

// B1 z review #9: jedno zatrute zrodlo nie moze wyciszyc (ani zNaN-owac) calego miksu.
TEST_P(AudioNonFinite, PoisonedSourceDoesNotSilenceOthers) {
    const auto wav = WriteSineWav(1.0f);
    for (const auto& f : kFields) {
        SCOPED_TRACE(f.name);
        Scene s;
        AudioSystem audio(s, Silent());
        auto& good = s.CreateGameObject("Good").AddComponent<AudioSource>();
        good.clip = wav;
        good.spatial = false;
        auto& badSrc = s.CreateGameObject("Bad").AddComponent<AudioSource>();
        badSrc.clip = wav;
        badSrc.spatial = false;
        f.poison(badSrc, GetParam());
        audio.Update();
        const MixStats m = Mix(audio);
        EXPECT_TRUE(m.finite) << "NaN/Inf w buforze wyjsciowym";
        EXPECT_GT(m.rms, 0.05f) << "zdrowe zrodlo ucichlo";
    }
}

TEST_P(AudioNonFinite, MasterVolumeDoesNotPoisonMix) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    audio.Update();
    audio.SetMasterVolume(GetParam());
    EXPECT_TRUE(Mix(audio).finite);
    audio.SetMasterVolume(1.0f); // i da sie wrocic do normalnej glosnosci
    const MixStats after = Mix(audio);
    EXPECT_TRUE(after.finite);
    EXPECT_GT(after.rms, 0.05f);
}

INSTANTIATE_TEST_SUITE_P(AllFields, AudioNonFinite, ::testing::ValuesIn(kBadValues),
                         [](const auto& info) { return BadName(info.param); });

} // namespace
