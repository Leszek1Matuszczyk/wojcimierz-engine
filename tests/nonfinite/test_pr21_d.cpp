// =============================================================================
// Wojcimierz Engine - tests/nonfinite/test_pr21_d.cpp
// Na chwałę czcigodnego Wojcimierza - #21 f07e1d3: bitowy kontrakt no-op (Skytest1).
// =============================================================================
#include "scene/Scene.h"
#include <gtest/gtest.h>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <cstring>
#include <random>
using namespace wz;
namespace {
template <class T> bool Bits(const T& a, const T& b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }
struct H {
  Scene s; Transform *g, *p, *c;
  H() {
    g = &s.CreateGameObject("g").transform(); p = &s.CreateGameObject("p").transform(); c = &s.CreateGameObject("c").transform();
    p->SetParent(g, false); c->SetParent(p, false);
  }
  void Randomize(std::mt19937& r) {
    std::uniform_real_distribution<float> u(-1, 1), sc(0.3f, 3.0f), big(-500, 500);
    auto rq = [&] { return glm::normalize(glm::quat(u(r), u(r), u(r), u(r))); };
    g->SetLocalPosition({big(r), big(r), big(r)}); g->SetLocalRotation(rq()); g->SetLocalScale({sc(r), sc(r), sc(r)});
    p->SetLocalPosition({u(r) * 10, u(r) * 10, u(r) * 10}); p->SetLocalRotation(rq()); p->SetLocalScale({sc(r), sc(r), sc(r)});
    c->SetLocalPosition({u(r) * 10, u(r) * 10, u(r) * 10}); c->SetLocalRotation(rq());
  }
  // Uniewaznia cache calego poddrzewa bez zmiany wartosci (zmiana i powrot rodzica).
  void Invalidate() { const glm::vec3 pp = p->GetLocalPosition(); p->SetLocalPosition(pp + glm::vec3(1, 0, 0)); p->SetLocalPosition(pp); }
};
}  // namespace

// SetX(GetX()) z uniewaznieniem cache miedzy get a set: getter po przeliczeniu musi
// dac te same bity, a set - byc no-opem. 1000 losowych hierarchii x 10 powtorzen.
TEST(PR21d, CacheInvalidatedRoundTripIsBitExactNoOp) {
  std::mt19937 r(0x5EED); int bitsDiff = 0, bumps = 0, n = 0;
  for (int h = 0; h < 1000; ++h) {
    H x; x.Randomize(r);
    for (int k = 0; k < 10; ++k, ++n) {
      const glm::vec3 pos = x.c->GetPosition(); const glm::quat rot = x.c->GetRotation();
      const glm::vec3 eul = x.c->GetLocalEulerAngles();
      x.Invalidate();
      if (!Bits(pos, x.c->GetPosition()) || !Bits(rot, x.c->GetRotation())) ++bitsDiff;
      x.Invalidate();  // drugi raz: cache zimny przy samym SetX
      const auto v0 = x.c->GetChangeVersion();
      x.c->SetPosition(pos); x.c->SetRotation(rot); x.c->SetLocalEulerAngles(eul);
      x.c->SetLocalPosition(x.c->GetLocalPosition()); x.c->SetLocalRotation(x.c->GetLocalRotation());
      x.c->SetLocalScale(x.c->GetLocalScale());
      if (x.c->GetChangeVersion() != v0) ++bumps;
    }
  }
  std::printf("[PR21d] cache round-trip: %d prob, rozne bity %d, podbicia %d\n", n, bitsDiff, bumps);
  EXPECT_EQ(bitsDiff, 0); EXPECT_EQ(bumps, 0);
}

// Stara wartosc swiata po ruchu rodzica to realna zmiana: musi podbic wersje i trafic.
TEST(PR21d, OldWorldValueAfterParentMoveBumps) {
  std::mt19937 r(7); int noBumpPos = 0, noBumpRot = 0, n = 0; float maxErr = 0, maxAng = 0;
  for (int h = 0; h < 1000; ++h, ++n) {
    H x; x.Randomize(r);
    const glm::vec3 pos = x.c->GetPosition(); const glm::quat rot = x.c->GetRotation();
    x.p->Translate({0.01f, 0, 0}, true); x.p->Rotate({0, 1, 0}, false);
    auto v0 = x.c->GetChangeVersion(); x.c->SetPosition(pos); if (x.c->GetChangeVersion() == v0) ++noBumpPos;
    v0 = x.c->GetChangeVersion(); x.c->SetRotation(rot); if (x.c->GetChangeVersion() == v0) ++noBumpRot;
    maxErr = std::max(maxErr, glm::length(x.c->GetPosition() - pos) / std::max(1.0f, glm::length(pos)));
    { const glm::dquat a(x.c->GetRotation()), b(rot); const double d = std::min(1.0, std::abs(glm::dot(glm::normalize(a), glm::normalize(b))));
      maxAng = std::max(maxAng, float(2.0 * std::acos(d))); }
  }
  std::printf("[PR21d] stara wartosc po ruchu rodzica: %d prob, brak podbicia poz %d, rot %d, max blad wzgl %.3g, kat %.3g rad\n",
              n, noBumpPos, noBumpRot, maxErr, maxAng);
  EXPECT_EQ(noBumpPos, 0); EXPECT_EQ(noBumpRot, 0); EXPECT_LT(maxErr, 1e-5f); EXPECT_LT(maxAng, 1e-5f);
}

// LookAt co klatke na nieruchomy cel: ile klatek podbija wersje.
TEST(PR21d, LookAtStationaryTargetEveryFrame) {
  std::mt19937 r(11); std::uniform_real_distribution<float> u(-20, 20);
  const char* names[] = {"korzen", "pod hierarchia"};
  for (int mode = 0; mode < 2; ++mode) {
    int cfgBumpEvery = 0, cfgBumpSome = 0, cfgNever = 0, cycles2 = 0; long totalBumps = 0; const int N = 1000, F = 120;
    for (int i = 0; i < N; ++i) {
      H x; if (mode == 1) x.Randomize(r); else x.c->SetPosition({u(r), u(r), u(r)});
      Transform* t = mode == 1 ? x.c : x.g; if (mode == 0) t->SetPosition({u(r), u(r), u(r)});
      const glm::vec3 target{u(r), u(r), u(r)};
      t->LookAt(target);  // klatka 0 - realny obrot
      int b = 0; glm::quat prev2 = t->GetLocalRotation(), prev1 = prev2;
      bool cyc = false;
      for (int f = 1; f <= F; ++f) {
        const auto v0 = t->GetChangeVersion(); t->LookAt(target);
        if (t->GetChangeVersion() != v0) ++b;
        const glm::quat now = t->GetLocalRotation();
        if (f > 2 && Bits(now, prev2) && !Bits(now, prev1)) cyc = true;
        prev2 = prev1; prev1 = now;
      }
      totalBumps += b; if (cyc) ++cycles2;
      if (b == F) ++cfgBumpEvery; else if (b > 0) ++cfgBumpSome; else ++cfgNever;
    }
    std::printf("[PR21d] LookAt %s: %d konfiguracji x %d klatek: podbicie w kazdej %d, czasem %d, nigdy %d; cykl 2-klatkowy %d; srednio %.1f podbic/120\n",
                names[mode], N, F, cfgBumpEvery, cfgBumpSome, cfgNever, cycles2, double(totalBumps) / N);
  }
}

// Drobne: -0.0f i -q w seterach lokalnych.
TEST(PR21d, NegativeZeroAndNegatedQuat) {
  H x; auto v0 = x.c->GetChangeVersion();
  x.c->SetLocalPosition({-0.0f, 0, 0}); const bool negZero = x.c->GetChangeVersion() != v0;
  v0 = x.c->GetChangeVersion(); x.c->SetPosition({0, -0.0f, 0}); const bool negZeroW = x.c->GetChangeVersion() != v0;
  x.c->SetLocalRotation(glm::normalize(glm::quat(0.9f, 0.1f, 0.2f, 0.3f)));
  v0 = x.c->GetChangeVersion(); x.c->SetLocalRotation(-x.c->GetLocalRotation()); const bool negLocal = x.c->GetChangeVersion() != v0;
  v0 = x.c->GetChangeVersion(); x.c->SetRotation(-x.c->GetRotation()); const bool negWorld = x.c->GetChangeVersion() != v0;
  std::printf("[PR21d] podbicie: SetLocalPosition(-0)=%d SetPosition(-0)=%d SetLocalRotation(-q)=%d SetRotation(-q)=%d\n",
              negZero, negZeroW, negLocal, negWorld);
}

// Powtorka po ruchu rodzica: pierwsze wywolanie po ruchu podbija, kolejne 1000 juz nie
// (ani wersja, ani dirty, ani bity lokalne). 500 losowych hierarchii, 3 ruchy rodzica.
TEST(PR21d, RepeatAfterParentMoveBumpsOnceThenNoOp) {
  std::mt19937 r(0xB0B); std::uniform_real_distribution<float> u(-20, 20);
  int firstNoBump[3] = {0, 0, 0}, laterBumps[3] = {0, 0, 0}, laterDirty[3] = {0, 0, 0}, laterBits[3] = {0, 0, 0}, n = 0;
  const char* nm[] = {"LookAt", "SetRotation", "SetPosition"};
  for (int h = 0; h < 500; ++h) {
    H x; x.Randomize(r);
    const glm::vec3 target{u(r), u(r), u(r)}, p{u(r), u(r), u(r)};
    const glm::quat q = glm::normalize(glm::quat(u(r), u(r), u(r), u(r)));
    for (int k = 0; k < 3; ++k) {
      auto call = [&] { if (k == 0) x.c->LookAt(target); else if (k == 1) x.c->SetRotation(q); else x.c->SetPosition(p); };
      call(); (void)x.c->GetLocalToWorldMatrix();
      for (int move = 0; move < 3; ++move, ++n) {
        if (move == 0) x.p->Translate({0.5f, 0, 0}, true); else if (move == 1) x.p->Rotate({0, 7, 0}, false); else x.g->SetLocalScale(x.g->GetLocalScale() * 1.1f);
        // SetRotation: przesuniecie i skala rodzica nie zmieniaja obrotu swiata - tam brak podbicia jest poprawny.
        const bool mustBump = !(k == 1 && move != 1);
        auto v0 = x.c->GetChangeVersion(); call(); if ((x.c->GetChangeVersion() == v0) == mustBump) ++firstNoBump[k];
        (void)x.c->GetLocalToWorldMatrix();
        const glm::vec3 lp = x.c->GetLocalPosition(); const glm::quat lq = x.c->GetLocalRotation(); v0 = x.c->GetChangeVersion();
        for (int i = 0; i < 1000; ++i) { call(); laterDirty[k] += x.c->IsWorldMatrixDirty(); (void)x.c->GetLocalToWorldMatrix(); }
        laterBumps[k] += x.c->GetChangeVersion() != v0;
        laterBits[k] += !Bits(lp, x.c->GetLocalPosition()) || !Bits(lq, x.c->GetLocalRotation());
      }
    }
  }
  for (int k = 0; k < 3; ++k) {
    std::printf("[PR21d] %s po ruchu rodzica (%d prob/funkcje): pierwsze niezgodne z oczekiwaniem %d, powtorki z podbiciem %d, dirty %d, zmienione bity %d\n",
                nm[k], n / 3, firstNoBump[k], laterBumps[k], laterDirty[k], laterBits[k]);
    EXPECT_EQ(firstNoBump[k], 0); EXPECT_EQ(laterBumps[k], 0); EXPECT_EQ(laterDirty[k], 0); EXPECT_EQ(laterBits[k], 0);
  }
}
