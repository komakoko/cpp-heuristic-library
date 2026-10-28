#pragma once
#include "../core/random.hpp"
#include "../core/timer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

namespace komaAHC {
struct SAConfig {
	double time_limit_ms = 1900.0; // 入力・初期解の構築も含む。出力用の余裕を残す。
	double start_temperature = 100.0;
	double end_temperature = 0.1;
	std::uint64_t seed = 1;
	std::int64_t iterations = -1; // -1: 時間制限まで。それ以外: 指定回数（時間制限は使わない）。
};

template <class State>
struct SAResult {
	State best;
	std::uint64_t iterations = 0;
	std::uint64_t accepted = 0;
};

template <class State>
SAResult<State> simulated_annealing(State initial, const Timer &timer, const SAConfig &config = {}) {
	using namespace std;
	assert(config.iterations >= -1);
	assert(config.iterations != -1 || config.time_limit_ms >= 0);
	assert(config.start_temperature > 0 && config.end_temperature > 0);
	SAResult<State> result{std::move(initial)};
	State current = result.best;
	Random random(config.seed);

	double temperature = config.start_temperature;
	while(config.iterations == -1 || result.iterations < static_cast<std::uint64_t>(config.iterations)) {
		// 時計・温度の更新は 256 回に 1 回。重い近傍なら間隔を小さくする。
		if((result.iterations & 255ULL) == 0) {
			double progress;
			if(config.iterations == -1) {
				const double elapsed = timer.elapsed_ms();
				if(elapsed >= config.time_limit_ms) break;
				progress = elapsed / config.time_limit_ms;
			} else {
				progress = static_cast<double>(result.iterations) / config.iterations;
			}
			progress = clamp(progress, 0.0, 1.0);
			// 指数的に冷却。開始温度は近傍のスコア差に合わせて調整する。
			temperature = config.start_temperature * pow(config.end_temperature / config.start_temperature, progress);
		}
		++result.iterations;
		const auto move = current.propose_move(random);
		if(!move) continue;
		const auto previous_score = current.score();
		if(!current.apply(*move)) continue;

		const double diff = static_cast<double>(static_cast<long double>(current.score()) - static_cast<long double>(previous_score));
		if(diff >= 0.0 || random.next_double() < exp(diff / temperature)) {
			++result.accepted;
			if(current.score() > result.best.score()) result.best = current;
		} else {
			current.undo(*move);
		}
	}
	return result; // 最後の状態ではなく、探索中の最良解を返す。
}

} // namespace komaAHC
