// ============================================================
//  Wojcimierz Engine: tests/test_type_name.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "core/TypeName.h"

#include <gtest/gtest.h>

namespace tn_test {
struct Probe {};
} // namespace tn_test

using wz::NormalizeTypeName;

// Stałe napisy w stylu MSVC: wynik nie zależy od kompilatora, na którym biegnie test.
TEST(TypeName, NormalizeStripsMsvcPrefixes) {
    EXPECT_EQ(NormalizeTypeName("class test_serializer::Unreflected"), "test_serializer::Unreflected");
    EXPECT_EQ(NormalizeTypeName("struct a::B"), "a::B");
    EXPECT_EQ(NormalizeTypeName("enum a::Mode"), "a::Mode");
    EXPECT_EQ(NormalizeTypeName("union U"), "U");
    EXPECT_EQ(NormalizeTypeName("class std::vector<struct a::B,class std::allocator<struct a::B> >"),
              "std::vector<a::B,std::allocator<a::B> >");
    EXPECT_EQ(NormalizeTypeName("class gra::Pair<struct a::X, enum b::Y>"), "gra::Pair<a::X, b::Y>");
    EXPECT_EQ(NormalizeTypeName("class std::function<void (struct a::B)>"), "std::function<void (a::B)>");
}

TEST(TypeName, NormalizeKeepsGccClangNamesAndLookalikes) {
    EXPECT_EQ(NormalizeTypeName("test_serializer::Unreflected"), "test_serializer::Unreflected");
    EXPECT_EQ(NormalizeTypeName("std::vector<int, std::allocator<int> >"), "std::vector<int, std::allocator<int> >");
    EXPECT_EQ(NormalizeTypeName("classy::structure"), "classy::structure");
    EXPECT_EQ(NormalizeTypeName("my::subclass Foo"), "my::subclass Foo");
    EXPECT_EQ(NormalizeTypeName(""), "");
}

// Na każdym kompilatorze ta sama nazwa (GCC/Clang: demangling, MSVC: obcięty prefiks).
TEST(TypeName, TypeNameOfIsPortable) {
    EXPECT_EQ(wz::TypeNameOf(typeid(tn_test::Probe)), "tn_test::Probe");
    EXPECT_EQ(wz::DemangleTypeName(nullptr), "");
}
