#pragma once
#include "../core/random.hpp"
#include "../common.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

namespace komaAHC {
/**
同じハッシュの状態の重複採用を防ぐ。State は optional<uint64_t> 相当を返す diversity_hash() を持つこと。

コンストラクタ:
`HashDiversityFilter()`: 空の採用履歴を作る。O(1)。

関数:
`accept(state)`: ハッシュが未登録または nullopt なら採用する。ハッシュ計算を除き期待 O(1)。

使用例:
```cpp
komaAHC::HashDiversityFilter<State> filter;
bool accepted = filter.accept(state);
```
*/
template <class State>
struct HashDiversityFilter {
/**
採用済みのハッシュ値。
*/
	std::unordered_set<std::uint64_t> seen;
/**
状態を新しい代表として採用できるか判定する。

引数:
`state`: diversity_hash() を持つ状態。

返り値:
未登録のハッシュを登録した場合、または nullopt の場合は true。登録済みなら false。

計算量:
ハッシュ計算を除き期待 O(1)。

使用例:
```cpp
bool accepted = filter.accept(state);
```
*/
	bool accept(const State &state) {
		const auto hash = state.diversity_hash();
		return !hash || seen.insert(*hash).second;
	}
};

/**
ビーム探索の設定を保持する。

コンストラクタ:
`BeamConfig()`: 幅 100、制限時間 1900 ms、最大化、通常探索、多様性判定なしで初期化する。O(1)。

関数:
なし。各フィールドを直接設定する。

使用例:
```cpp
komaAHC::BeamConfig config;
config.width = 200;
```
*/
struct BeamConfig {
	/**
	通常探索で各層に残す数。chokudai 探索では 1 周で各深さから展開する数。正の値を指定する。
	*/
	std::size_t width = 100;
	/**
	開始時刻から測った制限時間（ミリ秒）。時間制限モードでは 0 以上。
	*/
	double time_limit_ms = 1900.0;
	/**
	true なら score と evaluate を最大化し、false なら最小化する。
	*/
	bool maximize = true;
	/**
	true なら深さ別の候補を保持し、繰り返し探索する。
	*/
	bool chokudai = false;
	/**
	true なら各深さで DiversityFilter による重複除外を使う。
	*/
	bool use_diversity = false;
};

/**
ビーム探索で得た最良状態と手順を保持する。

コンストラクタ:
`BeamResult{best, moves, completed_depth, expanded_nodes}`: 集成体として初期化する。末尾の深さと生成数は省略時 0。State と Move のコピー・移動コストに依存する。

関数:
なし。各フィールドを直接参照する。

使用例:
```cpp
auto result = komaAHC::beam_search<State, Move>(initial, 10, start_time);
auto moves = result.moves;
```
*/
template <class State, class Move>
struct BeamResult {
	/**
	探索中に得た最良状態。改善がなければ初期状態。
	*/
	State best;
	/**
	初期状態から最良状態に至る操作列。
	*/
	std::vector<Move> moves;
	/**
	通常探索では採用済みの層の最大深さ。chokudai 探索では生成した状態の最大深さ。
	*/
	int completed_depth = 0;
	/**
	生成した子状態の数。
	*/
	std::size_t expanded_nodes = 0;
};

/**
制限時間内にビーム探索または chokudai 探索で最良状態を探す。

引数:
`initial`: turn == 0 の初期状態。State はコピー可能で、turn、score()、evaluate()、next_moves()、apply(move) を持つ。apply は turn を 1 増やすこと。全ての状態の score() を有効な解として比較する。
`max_depth`: 最大深さ。0 以上。
`start_time`: `koma::now()` で取得した開始時刻（秒）。制限時間はこの時刻が基準。
`config`: 探索設定。既定値は BeamConfig{}。width > 0、time_limit_ms >= 0。
`Move`: 既定構築とコピーが可能な操作型。
`DiversityFilter`: 既定構築可能で accept(state) を持つ型。既定型では State::diversity_hash() が必要（多様性判定を無効にしても必要）。

返り値:
初期状態と採用状態のうち score() が最良の状態、その操作列、到達深さ、生成した子状態数。通常探索で打ち切られた層は採用しない。

計算量:
状態操作を O(1)、生成数を N として通常探索は O(N log(N+1)) が上界。chokudai 探索も O(N log(N+1))。空間は候補・履歴のため O(N + max_depth)。State のサイズや操作コストに応じて増加する。

使用例:
```cpp
double start_time = koma::now();
komaAHC::BeamConfig config;
auto result = komaAHC::beam_search<State, Move>(initial, 10, start_time, config);
```
*/
template <class State, class Move, class DiversityFilter = HashDiversityFilter<State>>
BeamResult<State, Move> beam_search(State initial, int max_depth, double start_time, const BeamConfig &config = {}) {
	using namespace std;
	assert(max_depth >= 0 && initial.turn == 0);
	assert(config.width > 0 && config.time_limit_ms >= 0);
	struct History {
		std::size_t parent;
		Move move;
	};
	struct Node {
		State state;
		std::size_t history_id;
		long double evaluation;
	};
	struct Candidate {
		State state;
		long double evaluation;
		std::size_t parent;
		Move move;
		std::size_t order;
	};
	const auto is_better = [&config](const auto &a, const auto &b) {
		return config.maximize ? a > b : a < b;
	};
	const auto better = [&is_better](const Candidate &a, const Candidate &b) {
		if(a.evaluation != b.evaluation) return is_better(a.evaluation, b.evaluation);
		if(a.state.score() != b.state.score()) return is_better(a.state.score(), b.state.score());
		return a.order < b.order;
	};

	BeamResult<State, Move> result{std::move(initial), {}};
	std::vector<History> history{{0, {}}};
	std::size_t best_history_id = 0;
	const auto remember = [&](const State &state, std::size_t parent, const Move &move) {
		const std::size_t id = history.size();
		history.push_back({parent, move});
		result.completed_depth = max(result.completed_depth, state.turn);
		if(is_better(state.score(), result.best.score())) {
			result.best = state;
			best_history_id = id;
		}
		return id;
	};

	if(config.chokudai) {
		const auto worse = [&is_better](const Node &a, const Node &b) {
			if(a.evaluation != b.evaluation) return is_better(b.evaluation, a.evaluation);
			if(a.state.score() != b.state.score()) return is_better(b.state.score(), a.state.score());
			return a.history_id > b.history_id;
		};
		using Queue = priority_queue<Node, std::vector<Node>, decltype(worse)>;
		std::vector<Queue> queues;
		std::vector<DiversityFilter> diversity(config.use_diversity ? max_depth : 0);
		queues.reserve(max_depth);
		for(int depth = 0; depth < max_depth; ++depth) queues.emplace_back(worse);
		if(max_depth > 0) queues[0].push({result.best, 0, result.best.evaluate()});

		bool timed_out = false;
		while(!timed_out && ((koma::now() - start_time) * 1000.0) < config.time_limit_ms) {
			bool progressed = false;
			for(int depth = 0; depth < max_depth; ++depth) {
				auto &queue = queues[depth];
				for(std::size_t count = 0; count < config.width && !queue.empty();) {
					if(((koma::now() - start_time) * 1000.0) >= config.time_limit_ms) {
						timed_out = true;
						break;
					}
					Node node = queue.top();
					queue.pop();
					progressed = true;
					if(config.use_diversity && !diversity[depth].accept(node.state)) continue;
					++count;
					for(const auto &move : node.state.next_moves()) {
						if((result.expanded_nodes & 255ULL) == 0 && ((koma::now() - start_time) * 1000.0) >= config.time_limit_ms) {
							timed_out = true;
							break;
						}
						++result.expanded_nodes;
						State next = node.state;
						next.apply(move);
						const std::size_t id = remember(next, node.history_id, move);
						if(next.turn < max_depth) {
							const long double evaluation = next.evaluate();
							queues[next.turn].push({std::move(next), id, evaluation});
						}
					}
					if(timed_out) break;
				}
				if(timed_out) break;
			}
			if(!progressed) break;
		}
	} else {
		std::vector<Node> beam{{result.best, 0, 0}};
		for(int depth = 0; depth < max_depth; ++depth) {
			if(((koma::now() - start_time) * 1000.0) >= config.time_limit_ms) break;
			std::vector<Candidate> candidates;
			bool timed_out = false;
			for(const auto &node : beam) {
				for(const auto &move : node.state.next_moves()) {
					if((result.expanded_nodes & 255ULL) == 0 && ((koma::now() - start_time) * 1000.0) >= config.time_limit_ms) {
						timed_out = true;
						break;
					}
					++result.expanded_nodes;
					State next = node.state;
					next.apply(move);
					const long double evaluation = next.evaluate();
					candidates.push_back({std::move(next), evaluation, node.history_id, move, candidates.size()});
				}
				if(timed_out) break;
			}
			if(timed_out || candidates.empty() || ((koma::now() - start_time) * 1000.0) >= config.time_limit_ms) break;

			if(!config.use_diversity && candidates.size() > config.width) {
				nth_element(candidates.begin(), candidates.begin() + config.width, candidates.end(), better);
				candidates.resize(config.width);
			}
			sort(candidates.begin(), candidates.end(), better);
			DiversityFilter diversity;
			std::vector<Node> next_beam;
			next_beam.reserve(min(candidates.size(), config.width));
			for(auto &candidate : candidates) {
				if(config.use_diversity && !diversity.accept(candidate.state)) continue;
				const std::size_t id = remember(candidate.state, candidate.parent, candidate.move);
				next_beam.push_back({std::move(candidate.state), id, candidate.evaluation});
				if(next_beam.size() == config.width) break;
			}
			beam = std::move(next_beam);
		}
	}

	for(std::size_t id = best_history_id; id != 0; id = history[id].parent) result.moves.push_back(history[id].move);
	reverse(result.moves.begin(), result.moves.end());
	return result;
}

}
