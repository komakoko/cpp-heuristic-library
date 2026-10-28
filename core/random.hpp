#pragma once
#include <bits/stdc++.h>

namespace koma {
namespace detail {
inline std::uint64_t fast_random_bits() {
	static std::uint64_t state = 0x9e3779b97f4a7c15ULL;
	state ^= state >> 12;
	state ^= state << 25;
	state ^= state >> 27;
	return state * 0x2545f4914f6cdd1dULL;
}
} // namespace detail

/**
非負の long long の乱数を生成する。

返り値:
0 以上 LLONG_MAX 以下の long long。

計算量:
O(1)。

使用例:
```cpp
long long x = koma::fast_rand_ll() % 100;
```
*/
inline long long fast_rand_ll() { return static_cast<long long>(detail::fast_random_bits() >> 1); }

/**
0 以上 1 未満の double の乱数を生成する。

返り値:
0 以上 1 未満の double。

計算量:
O(1)。

使用例:
```cpp
double x = koma::fast_rand_double();
```
*/
inline double fast_rand_double() { return static_cast<double>(detail::fast_random_bits() >> 11) * 0x1.0p-53; }
} // namespace koma
