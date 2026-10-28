#pragma once
#include <bits/stdc++.h>

#define sz(a) static_cast<long long>((a).size())
#define rep(i, n) for(int i = 0; i < (long long)(n); ++i)
#define rep1(i, n) for(int i = 1; i <= (long long)(n); ++i)
#define rrep(i, n) for(int i = (long long)(n) - 1; i >= 0; --i)
#define rrep1(i, n) for(int i = (long long)(n); i >= 1; --i)
#define all(x) (x).begin(), (x).end()
#define rall(x) (x).rbegin(), (x).rend()
#define vec std::vector

namespace koma {
template <typename T>
bool chmin(T &a, const T &b) {
	if(a > b) {
		a = b;
		return true;
	}
	return false;
}

template <typename T>
bool chmax(T &a, const T &b) {
	if(a < b) {
		a = b;
		return true;
	}
	return false;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &a) {
	rep(i, sz(a)) {
		if(i) os << " ";
		os << a[i];
	}
	return os;
}

template <typename T, typename S>
std::istream &operator>>(std::istream &is, std::pair<T, S> &a) { return is >> a.first >> a.second; }

template <typename T>
std::istream &operator>>(std::istream &is, std::vector<T> &a) {
	for(T &i : a) is >> i;
	return is;
}

inline double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
} // namespace koma
