# 焼きなましの使い方

`search/simulated_annealing.hpp` を読み込み、問題に合わせた `State` と変更内容を表す `Move` を用意します。C++23 を想定しています。

[template_sa.cpp](../template_sa.cpp) をコピーして、`Move`、`State` の各操作、入力・初期解生成・出力を実装すると始められます。テンプレートの状態では近傍を生成せず、出力も行いません。

## State と Move の条件

`State` はコピー構築・コピー代入が可能で、次の関数を持つ必要があります。

| 関数 | 動作 |
|---|---|
| `score()` | 現在のスコアを返す。最大化・最小化は `config.maximize` で指定する |
| `propose_move()` | `std::optional<Move>` 相当を返す。変更を提案できなければ `std::nullopt` を返す |
| `apply(const Move&)` | 変更を適用して `true` を返す。失敗時は状態を変えず `false` を返す |
| `undo(const Move&)` | 直前の `apply()` による変更を元に戻す |

`Move` は自由に決められます。整数、ペア、自作の構造体などを使えます。ライブラリは `propose_move()` の返り値から変更内容を取り出し、`apply()` と `undo()` に渡します。変更内容は const として渡されるので、`apply()` で書き換えることはできません。

`propose_move()` は変更を提案するだけにし、解の更新は `apply()` で行います。改善または同点の変更は常に受理し、悪化した変更は温度に応じた確率で受理します。棄却した場合だけ `undo()` が呼ばれます。`undo()` では、スコアのキャッシュも含めて探索に必要な状態を元に戻してください。

最良解の保存では `State` 全体をコピーします。距離表などの不変データは、共有してコピー対象から外すと負担を減らせます。

## 呼び出しと時間の単位

```cpp
double start_time = koma::now();
// 入力や初期解の作成
komaAHC::SAConfig config;
config.maximize = false;
auto result = komaAHC::simulated_annealing(initial, start_time, config);
```

`now()` は秒単位です。`start_time` にはその返り値を渡します。一方、`time_limit_ms` はミリ秒単位です。入力も制限時間に含める場合は入力前に、探索だけを計測する場合は探索直前に開始時刻を取得します。

## 設定

| `SAConfig` のフィールド | 既定値 | 意味 |
|---|---|---|
| `maximize` | `true` | `true` は最大化、`false` は最小化 |
| `time_limit_ms` | `1900.0` | `start_time` からの制限時間。時間制限モードでは0以上 |
| `start_temperature` | `100.0` | 開始温度。正の値 |
| `end_temperature` | `0.1` | 終了温度。正の値 |
| `iterations` | `-1` | `-1` なら時間制限まで。0以上なら指定試行数で終了し、時間制限を使わない |
| `update_interval` | `256` | 時刻確認と温度更新の試行間隔。正の整数 |

時刻確認と温度更新は最初の試行前にも行います。近傍生成失敗・適用失敗も試行数に含まれます。確認間隔の間は制限時間を超えることがあるので、近傍が重い場合は `update_interval` を小さくし、出力時間にも余裕を残してください。

固定試行数で比較したい場合は、例えば `config.iterations = 100000;` とします。

### 温度の決め方

悪化量を正の値 `d`、受理したい確率を `p` とすると、温度は `T = -d / log(p)` で求められます。最大化でも最小化でも同じです。

開始時は典型的な悪化を50〜80%、終了時は1〜5%程度受け入れる設定を出発点にできます。典型的な悪化量が1000なら、開始温度4500で約80%、終了温度220で約1%の受理率になります。これは悪化量1000の変更に対する確率であり、全変更の受理率ではありません。

温度は開始温度から終了温度へ指数的に変化します。複数の入力で得点を比較して調整してください。

## 返り値

| `SAResult<State>` のフィールド | 意味 |
|---|---|
| `best` | 初期状態を含む、探索中に得られた最良状態 |
| `iterations` | 近傍生成失敗・適用失敗も含む試行数 |
| `accepted` | 適用に成功し、受理した変更の数 |

## 使用例：巡回セールスマン問題

都市の訪問順を状態とし、区間を反転する2-optを近傍にします。対称なユークリッド距離を想定しています。入力は都市数 `N` と各都市の座標 `x y` です。`N >= 3` としてください。

この例の温度は仮の値です。座標の大きさと典型的な距離の悪化量に合わせて調整してください。

```cpp
#include <bits/stdc++.h>
#include "search/simulated_annealing.hpp"

namespace koma {
struct Move {
    int l, r;
};

struct State {
    static inline std::vector<std::vector<double>> dist;
    std::vector<int> tour;
    double length = 0;
    double previous_length = 0;

    double score() const { return length; }

    std::optional<Move> propose_move() const {
        int n = tour.size();
        // 都市0の位置は固定する。
        int l = 1 + fast_rand_ll() % (n - 1);
        int r = 1 + fast_rand_ll() % (n - 1);
        if(l == r) return std::nullopt;
        if(l > r) std::swap(l, r);
        return Move{l, r};
    }

    bool apply(const Move& move) {
        auto [l, r] = move;
        int n = tour.size();
        int a = tour[l - 1], b = tour[l];
        int c = tour[r], d = tour[(r + 1) % n];
        previous_length = length;
        length += dist[a][c] + dist[b][d] - dist[a][b] - dist[c][d];
        std::reverse(tour.begin() + l, tour.begin() + r + 1);
        return true;
    }

    void undo(const Move& move) {
        std::reverse(tour.begin() + move.l, tour.begin() + move.r + 1);
        length = previous_length;
    }
};
}

int main() {
    std::cin.tie(nullptr);
    std::ios::sync_with_stdio(false);
    double start_time = koma::now();

    int n;
    std::cin >> n;
    assert(n >= 3);
    std::vector<double> x(n), y(n);
    for(int i = 0; i < n; ++i) std::cin >> x[i] >> y[i];

    koma::State::dist.assign(n, std::vector<double>(n));
    for(int i = 0; i < n; ++i) {
        for(int j = 0; j < n; ++j) {
            koma::State::dist[i][j] = std::hypot(x[i] - x[j], y[i] - y[j]);
        }
    }

    koma::State initial;
    initial.tour.resize(n);
    std::iota(initial.tour.begin(), initial.tour.end(), 0);
    for(int i = 0; i < n; ++i) {
        initial.length += koma::State::dist[i][(i + 1) % n];
    }

    komaAHC::SAConfig config;
    config.maximize = false;
    config.time_limit_ms = 1900;
    config.start_temperature = 100;
    config.end_temperature = 0.1;
    config.update_interval = 256;
    auto result = komaAHC::simulated_annealing(initial, start_time, config);

    for(int city : result.best.tour) std::cout << city << ' ';
    std::cout << result.best.tour.front() << '\n';
    std::cerr << "距離: " << result.best.length << '\n';
    std::cerr << "試行数: " << result.iterations << '\n';
}
```

ライブラリのルートをインクルードパスに指定します。ワークスペース直下で `main.cpp` に例を保存した場合は次のようにコンパイルできます。

```sh
g++ -std=c++23 -O2 -I lib/komaAHC main.cpp -o /tmp/tsp
```

距離表の構築には時間・空間ともに O(N²) が必要です。差分の距離計算は O(1) ですが、区間反転とその取り消しは区間長に比例します。浮動小数点の差分更新は誤差が蓄積するため、厳密な出力スコアが必要なら最良巡回路の距離を最後に再計算してください。
