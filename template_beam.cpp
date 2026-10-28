// clang-format off
#include <bits/stdc++.h>
#include "common.hpp"
#include "core/random.hpp"
#include "search/beam_search.hpp"
// #include <atcoder/all>
using namespace std; // using namespace atcoder; using mint = modint998244353;
using namespace koma;
using ll = long long; using ull = unsigned long long; using ld = long double; using pll = pair<ll, ll>;
// clang-format on

namespace koma {
struct Move {
};

struct State {
	int turn = 0;
	double current_score = 0;

	double score() const {
		return current_score;
	}

	long double evaluate() const {
		return current_score;
	}

	vector<Move> next_moves() const {
		return {};
	}

	void apply(const Move& move) {
		(void)move;
		++turn;
	}

	optional<uint64_t> diversity_hash() const {
		return nullopt;
	}
};

void input() {
}

State make_initial_state() {
	return State{};
}

void output(const State& best, const vector<Move>& moves) {
	(void)best;
	(void)moves;
}

void solve(double start_time) {
	State initial = make_initial_state();
	int max_depth = 100;
	komaAHC::BeamConfig config;
	config.width = 100;
	config.time_limit_ms = 1900.0;
	config.maximize = true;
	config.chokudai = false;
	config.use_diversity = false;
	auto result = komaAHC::beam_search<State, Move>(initial, max_depth, start_time, config);
	output(result.best, result.moves);
}
}

int main() {
	cin.tie(nullptr);
	ios::sync_with_stdio(false);
	double start_time = now();
	input();
	solve(start_time);
	return 0;
}
