#pragma once
#include <tuple>
#include <array>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdint>

static constexpr int BOARD_WIDTH = 6;
static constexpr int BOARD_HEIGHT = 6;
static constexpr int BOARD_SIZE = BOARD_WIDTH * BOARD_HEIGHT;

static constexpr int dy_table[] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static constexpr int dx_table[] = { 1, 1, 0, -1, -1, -1, 0, 1 };

static constexpr bool between(int a, int b, int c){
	return a <= b && b < c;
}

class BitBoard {
private:
	uint64_t m_state;
public:
	BitBoard() : m_state(0) { }
	bool operator()(int p) const { return (m_state >> p) & 1; }
	bool operator()(int r, int c) const {
		if(!between(0, r, BOARD_HEIGHT)){ return false; }
		if(!between(0, c, BOARD_WIDTH)){ return false; }
		return (*this)(r * BOARD_WIDTH + c);
	}
	void set(int p){ m_state |= (1ul << p); }
	void set(int r, int c){
		assert(between(0, r, BOARD_HEIGHT));
		assert(between(0, c, BOARD_WIDTH));
		set(r * BOARD_WIDTH + c);
	}
	void reset(int p){ m_state &= ~(1ul << p); }
	void reset(int r, int c){
		assert(between(0, r, BOARD_HEIGHT));
		assert(between(0, c, BOARD_WIDTH));
		reset(r * BOARD_WIDTH + c);
	}
	int count() const {
		return static_cast<int>(__builtin_popcountll(m_state));
	}
};

class AdjacencyMatrix {
private:
	using row_type = std::array<int8_t, BOARD_SIZE>;
	std::array<row_type, BOARD_SIZE> m_data;
public:
	AdjacencyMatrix() : m_data() {
		for(auto& row : m_data){
			for(auto& elem : row){ elem = -1; }
		}
	}
	int operator()(int p, int q) const { return m_data[p][q]; }
	int operator()(int pr, int pc, int qr, int qc) const {
		if(!between(0, pr, BOARD_HEIGHT)){ return false; }
		if(!between(0, pc, BOARD_WIDTH)){ return false; }
		const int p = pr * BOARD_HEIGHT + pc;
		const int q = qr * BOARD_HEIGHT + pc;
		return (*this)(p, q);
	}
	void connect(int p, int q, int step){
		m_data[p][q] = step;
		m_data[q][p] = step;
	}
	void connect(int pr, int pc, int qr, int qc, int step){
		assert(between(0, pr, BOARD_HEIGHT));
		assert(between(0, pc, BOARD_WIDTH));
		const int p = pr * BOARD_HEIGHT + pc;
		const int q = qr * BOARD_HEIGHT + pc;
		connect(p, q, step);
	}
};

class DisjointSet {
private:
	mutable std::array<uint8_t, BOARD_SIZE> m_parents;
	std::array<uint8_t, BOARD_SIZE> m_ranks;
public:
	DisjointSet()
		: m_parents()
		, m_ranks()
	{
		for(int i = 0; i < BOARD_SIZE; ++i){
			m_parents[i] = i;
			m_ranks[i] = 0;
		}
	}
	int find(int x) const {
		if(m_parents[x] == x){ return x; }
		m_parents[x] = find(m_parents[x]);
		return m_parents[x];
	}
	int unite(int x, int y){
		x = find(x);
		y = find(y);
		if(x == y){ return x; }
		if(m_ranks[x] < m_ranks[y]){
			m_parents[x] = y;
			return y;
		}else if(m_ranks[x] > m_ranks[y]){
			m_parents[y] = x;
			return x;
		}else{
			m_parents[y] = x;
			++m_ranks[x];
			return x;
		}
	}
	bool same(int x, int y) const {
		return find(x) == find(y);
	}
};

struct State {
	BitBoard classic[2];
	AdjacencyMatrix quantum_graph;
	DisjointSet quantum_disjoint_set;

	void put_classic(int p, int step){
		const int pr = p / BOARD_WIDTH, pc = p % BOARD_HEIGHT;
		const int color = step & 1;
		auto& x = classic[color];
		auto& y = classic[1 - color];
		assert(!x(p) && !y(p));
		x.set(p);
		for(int d = 0; d < 8; ++d){
			const int dx = dx_table[d], dy = dy_table[d];
			int qr = pr + dy, qc = pc + dx;
			while(y(qr, qc)){ qr += dy; qc += dx; }
			if(x(qr, qc)){
				qr -= dy; qc -= dx;
				while(qr != pr || qc != pc){
					x.set(qr, qc);
					y.reset(qr, qc);
					qr -= dy;
					qc -= dx;
				}
			}
		}
	}

	bool test_entanglement(int p, int q) const {
		return quantum_disjoint_set.same(p, q);
	}

	void select_entanglement(int p, int step){
		int head = 0, tail = 0;
		std::array<int8_t, BOARD_SIZE> queue;
		queue[tail++] = p;

		BitBoard vis;
		vis.set(p);

		std::array<int8_t, BOARD_SIZE> fix;
		std::fill(fix.begin(), fix.end(), -1);
		fix[step] = p;

		while(head < tail){
			const int u = queue[head++];
			for(int v = 0; v < BOARD_SIZE; ++v){
				const int s = quantum_graph(u, v);
				if(s < 0 || vis(v)){ continue; }
				queue[tail++] = v;
				vis.set(v);
				fix[s] = v;
			}
		}
		for(int i = BOARD_SIZE - 1; i >= 0; --i){
			if(fix[i] >= 0){ put_classic(fix[i], i); }
		}
	}

	void put_quantum(int p, int q, int step){
		quantum_graph.connect(p, q, step);
		quantum_disjoint_set.unite(p, q);
	}
};

inline std::ostream& operator<<(std::ostream& os, const State& s){
	char board_str[BOARD_HEIGHT][BOARD_WIDTH + 1] = { { '\0' } };
	for(int i = 0; i < BOARD_HEIGHT; ++i){
		for(int j = 0; j < BOARD_WIDTH; ++j){
			if(s.classic[0](i, j)){
				board_str[i][j] = 'o';
			}else if(s.classic[1](i, j)){
				board_str[i][j] = 'x';
			}else{
				board_str[i][j] = '.';
			}
		}
	}
	std::vector<std::tuple<int, int, int>> edges;
	for(int p = 0; p < BOARD_SIZE; ++p){
		if(s.classic[0](p) || s.classic[1](p)){ continue; }
		for(int q = p + 1; q < BOARD_SIZE; ++q){
			if(s.classic[0](q) || s.classic[1](q)){ continue; }
			const int step = s.quantum_graph(p, q);
			if(step >= 0){
				edges.emplace_back(step, p, q);
				board_str[p / BOARD_WIDTH][p % BOARD_WIDTH] = '?';
				board_str[q / BOARD_WIDTH][q % BOARD_WIDTH] = '?';
			}
		}
	}
	std::sort(edges.begin(), edges.end());
	for(const auto& e : edges){
		const int color = std::get<0>(e) & 1;
		const int p = std::get<1>(e), q = std::get<2>(e);
		os << "(" << "ox"[color] << ", "
		   << static_cast<char>('a' + p % BOARD_WIDTH)
		   << static_cast<char>('1' + p / BOARD_WIDTH) << ", "
		   << static_cast<char>('a' + q % BOARD_WIDTH)
		   << static_cast<char>('1' + q / BOARD_WIDTH) << ") ";
	}
	if(!edges.empty()){ os << std::endl; }
	os << "  abcdef" << std::endl;
	for(int i = 0; i < BOARD_HEIGHT; ++i){
		os << (i + 1) << " " << board_str[i] << std::endl;
	}
	return os;
}
