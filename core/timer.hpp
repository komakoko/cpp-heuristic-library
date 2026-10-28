#pragma once
#include <chrono>

namespace komaAHC {
struct Timer {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	double elapsed_ms() const {
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
};

} // namespace komaAHC
