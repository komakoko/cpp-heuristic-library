#pragma once
#include "../core/random.hpp"
#include "../common.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

namespace komaAHC {
/**
焼きなまし探索の設定を保持する。

コンストラクタ:
`SAConfig()`: 制限時間 1900 ms、開始温度 100、終了温度 0.1、最大化、時間制限モード、時刻確認・温度更新間隔 256 試行で初期化する。O(1)。

関数:
なし。各フィールドを直接設定する。

使用例:
```cpp
komaAHC::SAConfig config;
config.iterations = 10000;
```
*/
struct SAConfig {
	/**
	開始時刻から測った制限時間（ミリ秒）。時間制限モードでは 0 以上。
	*/
	double time_limit_ms = 1900.0;
	/**
	探索開始時の温度。正の値を指定する。
	典型的な悪化量 d を受理確率 p で受け入れる温度は -d / log(p)。p = 0.5〜0.8 程度を目安に調整する。
	例えば d = 1000、p = 0.8 なら約 4500。
	*/
	double start_temperature = 100.0;
	/**
	探索終了時に向かう温度。正の値を指定する。
	典型的な悪化量 d を受理確率 p で受け入れる温度は -d / log(p)。p = 0.01〜0.05 程度を目安に調整する。
	例えば d = 1000、p = 0.01 なら約 220。
	*/
	double end_temperature = 0.1;
	/**
	true ならスコアを最大化し、false なら最小化する。
	*/
	bool maximize = true;
	/**
	SAConfig では -1 が時間制限モード、非負が固定試行数。SAResult では実行した試行数。
	*/
	std::int64_t iterations = -1;
	/**
	時刻確認と温度更新の試行間隔。正の整数を指定する。最初の試行前にも実行する。
	近傍が重い場合は小さくすると、時間制限の超過を抑えられる。
	*/
	std::uint64_t update_interval = 256;
};

/**
焼きなまし探索の最良状態と試行統計を保持する。

コンストラクタ:
`SAResult{best, iterations, accepted}`: 集成体として初期化する。試行数と受理数は省略時 0。State のコピー・移動コストに依存する。

関数:
なし。各フィールドを直接参照する。

使用例:
```cpp
auto result = komaAHC::simulated_annealing(initial, start_time);
auto best = result.best;
```
*/
template <class State>
struct SAResult {
	/**
	探索中に得た最良状態。改善がなければ初期状態。
	*/
	State best;
	/**
	SAConfig では -1 が時間制限モード、非負が固定試行数。SAResult では実行した試行数。
	*/
	std::uint64_t iterations = 0;
	/**
	適用に成功し受理した近傍の数。
	*/
	std::uint64_t accepted = 0;
};

/**
焼きなましでスコアを最大化または最小化する状態を探す。

引数:
`initial`: コピー可能・代入可能な初期状態。次の関数を持つこと。
  `score()`: 比較するスコアを返す。最大化・最小化は config.maximize で指定する。
  `propose_move()`: 変更を optional 相当で返す。変更がない場合は空を返す。
  `apply(move)`: 変更を適用し、成功時は true、失敗時は状態を変更せず false を返す。
  `undo(move)`: 直前に適用した変更を元に戻す。
`start_time`: `koma::now()` で取得した開始時刻（秒）。制限時間はこの時刻が基準。
`config`: 既定値は SAConfig{}。両温度は正。iterations は -1 または非負。-1 のとき time_limit_ms は非負で、時間制限を使う。非負のとき時間制限を使わない。

返り値:
初期状態を含む探索中の最良状態、試行数、受理数。近傍なし・適用失敗も試行数に含む。

計算量:
試行数 I、各状態操作のコスト C として O(I C)。最良状態の更新時にコピーが発生する。時間の確認は config.update_interval 試行ごとなので制限を超える場合がある。

使用例:
```cpp
double start_time = koma::now();
komaAHC::SAConfig config;
config.iterations = 10000;
auto result = komaAHC::simulated_annealing(initial, start_time, config);
```
*/
template <class State>
SAResult<State> simulated_annealing(State initial, double start_time, const SAConfig &config = {}) {
	using namespace std;
	assert(config.iterations >= -1);
	assert(config.iterations != -1 || config.time_limit_ms >= 0);
	assert(config.start_temperature > 0 && config.end_temperature > 0);
	assert(config.update_interval > 0);
	SAResult<State> result{std::move(initial)};
	State current = result.best;

	double temperature = config.start_temperature;
	std::uint64_t until_update = 0;
	while(config.iterations == -1 || result.iterations < static_cast<std::uint64_t>(config.iterations)) {
		if(until_update == 0) {
			double progress;
			if(config.iterations == -1) {
				const double elapsed = ((koma::now() - start_time) * 1000.0);
				if(elapsed >= config.time_limit_ms) break;
				progress = elapsed / config.time_limit_ms;
			} else {
				progress = static_cast<double>(result.iterations) / config.iterations;
			}
			progress = clamp(progress, 0.0, 1.0);
			temperature = config.start_temperature * pow(config.end_temperature / config.start_temperature, progress);
			until_update = config.update_interval;
		}
		--until_update;
		++result.iterations;
		const auto move = current.propose_move();
		if(!move) continue;
		const auto previous_score = current.score();
		if(!current.apply(*move)) continue;

		const double score_diff = static_cast<double>(static_cast<long double>(current.score()) - static_cast<long double>(previous_score));
		const double diff = config.maximize ? score_diff : -score_diff;
		if(diff >= 0.0 || koma::fast_rand_double() < exp(diff / temperature)) {
			++result.accepted;
			if(config.maximize ? current.score() > result.best.score() : current.score() < result.best.score()) result.best = current;
		} else {
			current.undo(*move);
		}
	}
	return result;
}

} // namespace komaAHC
