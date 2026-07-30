#pragma once

#include <cstddef>

// Qt 5 still references these legacy MSVC STL helpers. They were removed
// from the VS 2026 STL, so provide the unchecked pointer semantics expected
// by Qt's release builds when compiling Qt 5 headers with that toolset.
namespace stdext {
template <typename Iterator> Iterator make_checked_array_iterator(Iterator iterator, std::size_t) {
    return iterator;
}

template <typename Iterator> Iterator make_unchecked_array_iterator(Iterator iterator) {
    return iterator;
}
} // namespace stdext
