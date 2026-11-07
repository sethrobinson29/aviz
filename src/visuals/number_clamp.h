#pragma once

#include <limits>
#include <type_traits>

namespace number_math {

template <typename T>
T clamp_number_to_range(double value, T min, T max) {
    static_assert(std::is_arithmetic_v<T>, "clamp_number_to_range requires an arithmetic target type");
    if (!(value >= static_cast<double>(min))) {
        return min;
    }
    if (value >= static_cast<double>(max)) {
        return max;
    }
    return static_cast<T>(value);
}

template <typename T>
T clamp_number_to_representable(double value) {
    return clamp_number_to_range<T>(value, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max());
}

}
