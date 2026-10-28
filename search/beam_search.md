# ビームサーチの使い方

`search/beam_search.hpp` を読み込み、問題に合わせて `State` と `Move` を作ります。C++23 を想定しています。

[template_beam.cpp](../template_beam.cpp) をコピーして始められます。テンプレートには状態、操作、入力・初期解生成・出力、設定のひな形があります。初期状態では候補を生成せず、出力も行いません。

## State と Move の条件

`State` はコピー構築・コピー代入が可能で、次のメンバーを持つ必要があります。

| メンバー | 条件 |
|---|---|
| `int turn` | 初期値は0。操作を適用するたびに1増やす |
| `score() const` | 最良解を選ぶためのスコアを返す |
| `evaluate() const` | 候補の優先順位を決める評価値を返す。`long double` に変換可能な値 |
| `next_moves() const` | 適用可能な操作の列を返す。例：`std::vector<Move>` |
| `apply(const Move&)` | 操作を適用し、`turn` を1増やす。返り値は使われない |
| `diversity_hash() const` | `std::optional<std::uint64_t>` 相当を返す。重複判定が不要なら `std::nullopt` |

`Move` は整数・ペア・自作構造体など自由に選べますが、既定構築とコピーが必要です。ライブラリが状態をコピーしてから操作を適用するため、焼きなましと違って `undo()` は不要です。適用失敗を検出する仕組みはないので、`next_moves()` では適用できる操作だけを返してください。

既定の重複判定型は `diversity_hash()` を参照するため、`use_diversity = false` でもこの関数を定義してください。

## 呼び出しと時間の単位

```cpp
double start_time = koma::now();
// 入力や初期解の作成
komaAHC::BeamConfig config;
config.width = 100;
config.maximize = true;
auto result = komaAHC::beam_search<State, Move>(
    initial, max_depth, start_time, config
);
```

`start_time` は `now()` の返り値（秒）です。`time_limit_ms` はその時刻からの制限時間（ミリ秒）です。入力も制限時間に含めるなら入力前に開始時刻を取得します。`max_depth` は0以上で指定します。

## score と evaluate

`evaluate()` は次に残す・展開する状態を選ぶための評価です。将来の見込みなどを含められます。`score()` は最終的に返す最良状態の比較に使います。

両方とも `maximize = true` なら大きいほど優先し、`false` なら小さいほど優先します。別々の方向にはできません。

**現在の実装は初期状態を含む、途中の深さの状態も最良解の対象にします。** 通常探索では採用した候補、chokudai探索では生成した候補を比較します。最大深さに到達した状態だけを返す仕組みではありません。途中状態も有効な解として扱えるスコア設計にするか、部分状態から解を補完できる形にしてください。

同点では最良状態を更新しません。最終層に到達しても、同点の初期状態や途中状態が `best` に残る場合があります。

## 設定

| `BeamConfig` のフィールド | 既定値 | 意味 |
|---|---|---|
| `width` | `100` | 通常探索で各層に残す状態数。正の整数 |
| `time_limit_ms` | `1900.0` | 開始時刻からの制限時間。0以上 |
| `maximize` | `true` | `true` は最大化、`false` は最小化 |
| `chokudai` | `false` | `true` なら深さ別の候補を保持して繰り返し探索する |
| `use_diversity` | `false` | `true` なら同じ深さでの重複候補を除外する |

通常探索は層ごとに候補を生成し、評価の良いものを最大 `width` 個残します。生成途中で時間切れになった層は採用しません。

chokudai探索では `width` は「1周で各深さから展開する数」です。候補の保持数の上限ではありません。通常探索と同じ値にする必要はなく、まず1などの小さな値から調整できます。候補と操作履歴を保持するため、生成数に応じてメモリが増えます。

時間確認の間に制限時間を超える場合があるので、出力の余裕を残してください。焼きなましの `update_interval` に相当する設定はありません。

## 多様性・重複除外

`use_diversity = true` にすると、`diversity_hash()` が返す値を使って同じ深さで重複を除外します。`std::nullopt` の状態は除外されません。ハッシュの衝突も同じ状態として扱うので、状態を適切に区別できる値を返してください。

通常探索は各層の評価順で最初の代表を残します。chokudai探索は各深さで最初に展開した代表を周回をまたいで保持します。必要なら第3テンプレート引数に、既定構築可能で `bool accept(const State&)` を持つ自作フィルターを渡せます。

## 返り値

| `BeamResult<State, Move>` のフィールド | 意味 |
|---|---|
| `best` | 探索中に得た最良状態。改善がなければ初期状態 |
| `moves` | 初期状態から `best` に至る操作列 |
| `completed_depth` | 通常探索は採用した状態の最大深さ。chokudai探索は生成した状態の最大深さ |
| `expanded_nodes` | 生成した子状態の数 |

`completed_depth` は `best.turn` と一致するとは限りません。

## 小さな実行例

各ターンで1または2を加え、3ターン以内で合計を最大化します。通常探索とchokudai探索のどちらでも使えます。

```cpp
#include <bits/stdc++.h>
#include "search/beam_search.hpp"

namespace koma {
struct State {
    int turn = 0;
    int total = 0;

    int score() const { return total; }
    long double evaluate() const { return total; }
    std::vector<int> next_moves() const { return {1, 2}; }

    void apply(int move) {
        total += move;
        ++turn;
    }

    std::optional<std::uint64_t> diversity_hash() const {
        return static_cast<std::uint64_t>(total);
    }
};
}

int main() {
    double start_time = koma::now();
    komaAHC::BeamConfig config;
    config.width = 2;
    config.use_diversity = true;
    auto result = komaAHC::beam_search<koma::State, int>(
        koma::State{}, 3, start_time, config
    );
    std::cout << result.best.total << '\n'; // 6
    for(int move : result.moves) std::cout << move << ' '; // 2 2 2
    std::cout << '\n';
}
```

ワークスペース直下で `main.cpp` に保存した場合は、次のようにコンパイルできます。

```sh
g++ -std=c++23 -O2 -I lib/komaAHC main.cpp -o /tmp/beam
```
