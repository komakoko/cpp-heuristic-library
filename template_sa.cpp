// clang-format off
#include <bits/stdc++.h>
#include "common.hpp"
#include "core/random.hpp"
#include "search/simulated_annealing.hpp"
// #include <atcoder/all>
using namespace std; // using namespace atcoder; using mint = modint998244353;
using namespace koma;
using ll = long long; using ull = unsigned long long; using ld = long double; using pll = pair<ll, ll>;
// clang-format on

namespace koma {
struct Move {
};

struct State {
	double current_score = 0;

	double score() const {
		return current_score;
	}

	optional<Move> propose_move() const {
		return nullopt;
	}

	bool apply(const Move& move) {
		(void)move;
		return false;
	}

	void undo(const Move& move) {
		(void)move;
	}
};

void input() {
}

State make_initial_state() {
	return State{};
}

void output(const State& best) {
	(void)best;
}

void solve(double start_time) {
	State initial = make_initial_state();
	komaAHC::SAConfig config;
	config.maximize = true;
	config.time_limit_ms = 1900.0;
	config.start_temperature = 100.0;
	config.end_temperature = 0.1;
	config.iterations = -1;
	config.update_interval = 256;
	auto result = komaAHC::simulated_annealing(initial, start_time, config);
	output(result.best);
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
