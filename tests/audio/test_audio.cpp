// ============================================================
//  Wojcimierz Engine: tests/audio/test_audio.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "audio/AudioSource.h"
#include "audio/AudioSystem.h"
#include "scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace wz;

namespace {

constexpr std::uint32_t kRate = 48000;

// Zapisuje mono WAV 16-bit z sinusem 440 Hz.
// Kazdy test pisze do wlasnego pliku (nazwa z nazwy testu), zeby rownolegly ctest
// i --gtest_repeat nie nadpisywaly sobie klipow.
std::string WriteSineWav(float seconds, float amplitude = 0.5f) {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string name = std::string("wz_") + info->test_suite_name() + "_" + info->name() + ".wav";
    const auto path = (std::filesystem::temp_directory_path() / name).string();
    const std::uint32_t n = std::uint32_t(seconds * kRate);
    std::vector<std::int16_t> pcm(n);
    for (std::uint32_t i = 0; i < n; ++i)
        pcm[i] = std::int16_t(amplitude * 32767.0f * std::sin(2.0f * 3.14159265f * 440.0f * float(i) / kRate));
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

// Srednia moc (RMS) z 'frames' ramek wyjscia.
float MixRms(AudioSystem& a, std::uint64_t frames = 4800) {
    std::vector<float> buf(frames * a.GetBackend().GetChannels());
    a.GetBackend().ReadMix(buf.data(), frames);
    double sum = 0;
    for (float s : buf) sum += double(s) * s;
    return float(std::sqrt(sum / double(buf.size())));
}

} // namespace

TEST(Audio, PlayOnAwakeProducesSound) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& go = s.CreateGameObject("Speaker");
    auto& src = go.AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    EXPECT_LT(MixRms(audio), 1e-6f); // przed Update nic nie gra
    audio.Update();
    EXPECT_TRUE(src.IsPlaying());
    EXPECT_GT(MixRms(audio), 0.1f);
}

TEST(Audio, NoPlayOnAwakeIsSilentUntilPlay) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    src.playOnAwake = false;
    audio.Update();
    EXPECT_FALSE(src.IsPlaying());
    EXPECT_LT(MixRms(audio), 1e-6f);
    src.Play();
    EXPECT_GT(MixRms(audio), 0.1f);
}

TEST(Audio, PlayRequestedBeforeVoiceExists) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    src.playOnAwake = false;
    src.Play(); // jeszcze bez glosu - odroczone
    EXPECT_TRUE(src.IsPlaying());
    audio.Update();
    EXPECT_TRUE(src.IsPlaying());
    EXPECT_GT(MixRms(audio), 0.1f);
}

TEST(Audio, VolumeScalesOutput) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    audio.Update();
    const float full = MixRms(audio);
    src.volume = 0.25f;
    audio.Update();
    const float quarter = MixRms(audio);
    EXPECT_NEAR(quarter / full, 0.25f, 0.03f);
}

TEST(Audio, StopAndNonLoopingEnd) {
    const auto wav = WriteSineWav(0.05f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    audio.Update();
    (void)MixRms(audio, 9600); // 0.2 s - klip (0.05 s) sie konczy
    EXPECT_FALSE(src.IsPlaying());
    EXPECT_LT(MixRms(audio), 1e-6f);
    src.Play(); // ponowne odtworzenie od poczatku
    EXPECT_TRUE(src.IsPlaying());
    EXPECT_GT(MixRms(audio, 1200), 0.1f);
    src.Stop();
    EXPECT_FALSE(src.IsPlaying());
    EXPECT_LT(MixRms(audio), 1e-6f);
}

TEST(Audio, LoopKeepsPlaying) {
    const auto wav = WriteSineWav(0.05f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    src.loop = true;
    audio.Update();
    (void)MixRms(audio, 9600);
    EXPECT_TRUE(src.IsPlaying());
    EXPECT_GT(MixRms(audio), 0.1f);
}

TEST(Audio, SpatialDistanceAttenuates) {
    const auto wav = WriteSineWav(1.0f);
    auto rmsAt = [&](const glm::vec3& pos) {
        Scene s;
        AudioSystem audio(s, Silent());
        s.CreateGameObject("Camera").AddComponent<AudioListener>();
        auto& go = s.CreateGameObject("Speaker");
        go.transform().SetPosition(pos);
        auto& src = go.AddComponent<AudioSource>();
        src.clip = wav;
        audio.Update();
        return MixRms(audio);
    };
    const float near = rmsAt({0, 0, -1});
    const float far = rmsAt({0, 0, -50});
    EXPECT_GT(near, 0.05f);
    EXPECT_LT(far, near * 0.2f);
}

TEST(Audio, DisabledSourceStopsAndDestroyReleases) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& go = s.CreateGameObject();
    auto& src = go.AddComponent<AudioSource>();
    src.clip = wav;
    src.spatial = false;
    audio.Update();
    EXPECT_GT(MixRms(audio), 0.1f);
    go.SetActive(false);
    audio.Update();
    EXPECT_LT(MixRms(audio), 1e-6f);
    go.SetActive(true);
    s.Destroy(go);
    s.FlushDestroyed(); // OnDestroy zwalnia glos - nic nie moze wisiec
    audio.Update();
    EXPECT_LT(MixRms(audio), 1e-6f);
}

TEST(Audio, MissingFileDoesNotCrash) {
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = "/nie/ma/takiego/pliku.wav";
    audio.Update();
    audio.Update(); // drugi raz nie probujemy ladowac (blad zapamietany)
    EXPECT_FALSE(src.IsPlaying());
    EXPECT_LT(MixRms(audio), 1e-6f);
    // Poprawiony clip laduje sie od nowa.
    src.clip = WriteSineWav(1.0f);
    src.spatial = false;
    src.Play();
    audio.Update();
    EXPECT_TRUE(src.IsPlaying());
    EXPECT_GT(MixRms(audio), 0.1f);
}

TEST(Audio, GarbageFileIsRejected) {
    const auto path = (std::filesystem::temp_directory_path() / "wz_Audio_garbage.wav").string();
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs("to nie jest WAV, czcigodny Wojcimierzu", f);
    std::fclose(f);
    Scene s;
    AudioSystem audio(s, Silent());
    auto& src = s.CreateGameObject().AddComponent<AudioSource>();
    src.clip = path;
    audio.Update();
    EXPECT_FALSE(src.IsPlaying());
}

TEST(Audio, SystemDestroyedBeforeScene) {
    const auto wav = WriteSineWav(1.0f);
    Scene s;
    {
        AudioSystem audio(s, Silent());
        s.CreateGameObject().AddComponent<AudioSource>().clip = wav;
        audio.Update();
    }
    // Scena niszczy sie po systemie - AudioSource::OnDestroy nie moze siegnac do martwego systemu.
    SUCCEED();
}
