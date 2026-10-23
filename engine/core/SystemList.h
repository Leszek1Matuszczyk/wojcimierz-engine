// =============================================================================
// Wojcimierz Engine - engine/core/SystemList.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace wz {

// Uporzadkowana lista nie-wlascicielskich wskaznikow na systemy silnika.
// Mniejszy `order` -> wczesniej; przy rownym `order` decyduje kolejnosc dodania.
// Bezpieczna modyfikacja w trakcie ForEach(): dodane dzialaja od nastepnego przebiegu,
// usuniete nie sa juz wolane.
template <typename T>
class SystemList {
public:
    void Add(T* system, int order = 0) {
        if (!system || Contains(system)) return;
        entries_.push_back(Entry{system, order, nextSeq_++});
        if (running_ > 0) needsCompact_ = true;
        else Compact();
    }

    bool Remove(T* system) {
        for (auto& e : entries_) {
            if (e.system && e.system == system) {
                e.system = nullptr;
                needsCompact_ = true;
                if (running_ == 0) Compact();
                return true;
            }
        }
        return false;
    }

    bool Contains(const T* system) const {
        if (!system) return false;
        return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.system == system; });
    }

    std::size_t Size() const {
        return static_cast<std::size_t>(
            std::count_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.system != nullptr; }));
    }

    // Wywoluje fn(T&) dla systemow obecnych na poczatku przebiegu (w kolejnosci).
    template <typename Fn>
    void ForEach(Fn&& fn) {
        ++running_;
        const std::size_t count = entries_.size();
        for (std::size_t i = 0; i < count; ++i)
            if (T* s = entries_[i].system) fn(*s);
        --running_;
        if (running_ == 0 && needsCompact_) Compact();
    }

    // Jak ForEach, ale bez kompaktowania na koncu (do zagniezdzonych przebiegow Pre/Post).
    struct RunScope {
        explicit RunScope(SystemList& l) : list(l), count(l.entries_.size()) { ++list.running_; }
        ~RunScope() {
            if (--list.running_ == 0 && list.needsCompact_) list.Compact();
        }
        template <typename Fn>
        void ForEach(Fn&& fn) {
            for (std::size_t i = 0; i < count; ++i)
                if (T* s = list.entries_[i].system) fn(*s);
        }
        SystemList& list;
        std::size_t count;
    };

private:
    struct Entry {
        T* system;
        int order;
        unsigned long long seq;
    };
    void Compact() {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const Entry& e) { return !e.system; }),
                       entries_.end());
        std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
            return a.order != b.order ? a.order < b.order : a.seq < b.seq;
        });
        needsCompact_ = false;
    }

    std::vector<Entry> entries_;
    unsigned long long nextSeq_ = 0;
    int running_ = 0;
    bool needsCompact_ = false;
};

} // namespace wz
