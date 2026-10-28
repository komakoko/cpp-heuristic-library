#pragma once
#include "../core/random.hpp"
#include "../core/timer.hpp"
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
// 同じ深さで同じハッシュを持つ状態をまとめる。nullopt は除外対象外。
template <class State>
struct HashDiversityFilter {
	std::unordered_set<std::uint64_t> seen;
	bool accept(const State &state) {
		const auto hash = state.diversity_hash();
		return !hash || seen.insert(*hash).second;
	}
};

struct BeamConfig {
	std::size_t width = 100; // 通常: 各層に残す数。chokudai: 1 周で各深さから展開する数（1 から調整）。
	double time_limit_ms = 1900.0;
	bool maximize = true;		// true: 大きいほど良い、false: 小さいほど良い。evaluate/score 共通。
	bool chokudai = false;		// true なら深さごとの候補を保持し、時間内で繰り返し探索する。
	bool use_diversity = false; // diversity_hash()/accept() の実装後、true にして有効化する。
};

template <class State, class Move>
struct BeamResult {
	State best;
	std::vector<Move> moves;
	int completed_depth = 0; // 通常: 採用済みの層の深さ。chokudai: 生成した状態の最大深さ。
	std::size_t expanded_nodes = 0;
};

template <class State, class Move, class DiversityFilter = HashDiversityFilter<State>>
BeamResult<State, Move> beam_search(State initial, int max_depth, const Timer &timer, const BeamConfig &config = {}) {
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
		std::size_t order; // 同点時も選別順を固定する。
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
	std::vector<History> history{{0, {}}}; // 0 は根。通常は採用した候補、chokudai は生成した候補を保存。
	std::size_t best_history_id = 0;
	const auto remember = [&](const State &state, std::size_t parent, const Move &move) {
		const std::size_t id = history.size();
		history.push_back({parent, move});
		result.completed_depth = max(result.completed_depth, state.turn);
		// この例では、どの深さでも「残りを全て選ばない」で有効な解になる。
		// TODO: 完成した状態だけが解になる問題では、完成判定・補完を変更する。
		if(is_better(state.score(), result.best.score())) {
			result.best = state;
			best_history_id = id;
		}
		return id;
	};

	if(config.chokudai) {
		// priority_queue は「優先度が低いか」を比較するので、better と向きが逆。
		const auto worse = [&is_better](const Node &a, const Node &b) {
			if(a.evaluation != b.evaluation) return is_better(b.evaluation, a.evaluation);
			if(a.state.score() != b.state.score()) return is_better(b.state.score(), a.state.score());
			return a.history_id > b.history_id;
		};
		using Queue = priority_queue<Node, std::vector<Node>, decltype(worse)>;
		std::vector<Queue> queues;
		// 各ターンで最初に展開した代表を、周回をまたいで保持する。
		std::vector<DiversityFilter> diversity(config.use_diversity ? max_depth : 0);
		queues.reserve(max_depth);
		for(int depth = 0; depth < max_depth; ++depth) queues.emplace_back(worse);
		if(max_depth > 0) queues[0].push({result.best, 0, result.best.evaluate()});

		bool timed_out = false;
		while(!timed_out && timer.elapsed_ms() < config.time_limit_ms) {
			bool progressed = false;
			for(int depth = 0; depth < max_depth; ++depth) {
				auto &queue = queues[depth];
				for(std::size_t count = 0; count < config.width && !queue.empty();) {
					if(timer.elapsed_ms() >= config.time_limit_ms) {
						timed_out = true;
						break;
					}
					Node node = queue.top();
					queue.pop(); // 展開した状態を次の周回で再展開しない。
					progressed = true;
					if(config.use_diversity && !diversity[depth].accept(node.state)) continue;
					++count; // 除外した候補は展開数に数えず、次の候補を取り出す。
					for(const auto &move : node.state.next_moves()) {
						if((result.expanded_nodes & 255ULL) == 0 && timer.elapsed_ms() >= config.time_limit_ms) {
							timed_out = true;
							break;
						}
						++result.expanded_nodes;
						State next = node.state;
						next.apply(move);
						const std::size_t id = remember(next, node.history_id, move);
						// 最終深さの状態は最良解との比較だけでよく、再展開しない。
						if(next.turn < max_depth) {
							const long double evaluation = next.evaluate();
							queues[next.turn].push({std::move(next), id, evaluation});
						}
					}
					if(timed_out) break;
				}
				if(timed_out) break;
			}
			if(!progressed) break; // 全ての候補を探索し尽くした。
		}
	} else {
		std::vector<Node> beam{{result.best, 0, 0}};
		for(int depth = 0; depth < max_depth; ++depth) {
			if(timer.elapsed_ms() >= config.time_limit_ms) break;
			std::vector<Candidate> candidates;
			bool timed_out = false;
			for(const auto &node : beam) {
				for(const auto &move : node.state.next_moves()) {
					if((result.expanded_nodes & 255ULL) == 0 && timer.elapsed_ms() >= config.time_limit_ms) {
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
			// 途中で打ち切った層は採用せず、直前までの最良解を返す。
			if(timed_out || candidates.empty() || timer.elapsed_ms() >= config.time_limit_ms) break;

			// 多様性判定を使う場合は、除外後も幅を埋められるよう候補を残す。
			if(!config.use_diversity && candidates.size() > config.width) {
				nth_element(candidates.begin(), candidates.begin() + config.width, candidates.end(), better);
				candidates.resize(config.width);
			}
			sort(candidates.begin(), candidates.end(), better);
			DiversityFilter diversity; // 各層の評価順で、最初の代表を残す。
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

} // namespace komaAHC
