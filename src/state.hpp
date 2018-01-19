#pragma once
#include <array>
#include <cstdint>
#include <cassert>

template <typename T>
class PointerRange {
public:
	using value_type = T;
	using const_iterator = const T *;
private:
	const value_type *m_begin;
	const value_type *m_end;
public:
	PointerRange(const value_type *begin, const value_type *end)
		: m_begin(begin)
		, m_end(end)
	{ }
	bool empty() const { return m_begin == m_end; }
	size_t size() const { return m_end - m_begin; }
	value_type operator[](size_t i) const { return m_begin[i]; }
	const_iterator cbegin() const { return m_begin; }
	const_iterator cend() const { return m_end; }
	const_iterator begin() const { return m_begin; }
	const_iterator end() const { return m_end; }
};


class ClassicBoard {

private:
	std::array<uint64_t, 2> m_stones;

	uint64_t clz(uint64_t x) const {
		if(x == 0){ return 36; }
		return __builtin_clzll(x) - (64 - 36);
	}

	uint64_t flip(int pos, uint64_t player, uint64_t other) const {
		uint64_t mask_x, mask_y, mask_z, mask_w;
		uint64_t outflank_x, outflank_y, outflank_z, outflank_w;
		uint64_t flipped = 0;
		const uint64_t om_x = other;
		const uint64_t om_y = other & 0363636363636ul;
		mask_x = 0004040404040ul >> (35 - pos);
		mask_y = 0370000000000ul >> (35 - pos);
		mask_z = 0010204102000ul >> (35 - pos);
		mask_w = 0002010040201ul >> (35 - pos);
		outflank_x = (0400000000000ul >> clz(~om_x & mask_x)) & player;
		outflank_y = (0400000000000ul >> clz(~om_y & mask_y)) & player;
		outflank_z = (0400000000000ul >> clz(~om_y & mask_z)) & player;
		outflank_w = (0400000000000ul >> clz(~om_y & mask_w)) & player;
		flipped |= (-outflank_x * 2) & mask_x;
		flipped |= (-outflank_y * 2) & mask_y;
		flipped |= (-outflank_z * 2) & mask_z;
		flipped |= (-outflank_w * 2) & mask_w;
		mask_x = 0010101010101ul << pos;
		mask_y = 0000000000076ul << pos;
		mask_z = 0000204102040ul << pos;
		mask_w = 0402010040200ul << pos;
		outflank_x = mask_x & ((om_x | ~mask_x) + 1) & player;
		outflank_y = mask_y & ((om_y | ~mask_y) + 1) & player;
		outflank_z = mask_z & ((om_y | ~mask_z) + 1) & player;
		outflank_w = mask_w & ((om_y | ~mask_w) + 1) & player;
		flipped |= (outflank_x - (outflank_x != 0)) & mask_x;
		flipped |= (outflank_y - (outflank_y != 0)) & mask_y;
		flipped |= (outflank_z - (outflank_z != 0)) & mask_z;
		flipped |= (outflank_w - (outflank_w != 0)) & mask_w;
		return flipped;
	}

public:
	ClassicBoard()
		: m_stones{0, 0}
	{ }

	bool operator==(const ClassicBoard& rhs) const {
		return m_stones == rhs.m_stones;
	}

	bool operator!=(const ClassicBoard& rhs) const {
		return m_stones != rhs.m_stones;
	}

	int get(int p) const {
		assert(0 <= p && p < 36);
		const uint64_t mask = (1ul << p);
		if(m_stones[0] & mask){ return  1; }
		if(m_stones[1] & mask){ return -1; }
		return 0;
	}

	void put(int p, int color){
		assert(get(p) == 0);
		const int t = (1 - color) >> 1;
		const auto f = flip(p, m_stones[t], m_stones[1 - t]);
		m_stones[0] ^= f;
		m_stones[1] ^= f;
		m_stones[t] |= (1ul << p);
	}

	void force_put(int p, int color){
		const int t = (1 - color) >> 1;
		m_stones[t] |= (1ul << p);
	}

	int count(int color) const {
		const int t = (1 - color) >> 1;
		return __builtin_popcountll(m_stones[t]);
	}

	uint64_t bitmap(int color) const {
		const int t = (1 - color) >> 1;
		return m_stones[t];
	}

};

std::ostream& operator<<(std::ostream& os, const ClassicBoard& b){
	for(int i = 0; i < 6; ++i){
		for(int j = 0; j < 6; ++j){ os << "x.o"[b.get(i * 6 + j) + 1]; }
		os << std::endl;
	}
	return os;
}


class State {

public:
	struct Edge {
		int8_t u, v, color;
		Edge() : u(0), v(0), color(0) { }
		Edge(int u, int v, int c) : u(u), v(v), color(c) { }
	};

private:
	ClassicBoard m_classic_board;
	size_t m_num_edges;
	std::array<Edge, 36> m_edges;

	uint64_t test_reachability(int root) const {
		uint64_t reachable = (1ul << root);
		while(true){
			const auto before = reachable;
			for(const auto& e : edges()){
				const uint64_t u = (1ul << e.u), v = (1ul << e.v);
				if(reachable & u){ reachable |= v; }
				if(reachable & v){ reachable |= u; }
			}
			if(reachable == before){ break; }
		}
		return reachable;
	}

public:
	State()
		: m_classic_board()
		, m_num_edges(0)
		, m_edges()
	{ }

	static State create_initial_state(){
		return State();
	}

	const ClassicBoard& classic_board() const {
		return m_classic_board;
	}

	PointerRange<Edge> edges() const {
		return PointerRange<Edge>(&m_edges[0], &m_edges[m_num_edges]);
	}

	bool test_entanglement(int p, int q) const {
		assert(m_classic_board.get(p) == 0);
		assert(m_classic_board.get(q) == 0);
		return (test_reachability(p) & (1ul << q)) != 0;
	}

	void select_entanglement(int p, int color){
		uint64_t reachable = (1ul << p);
		std::array<int, 36> fix_positions, fix_colors;
		std::fill(fix_colors.begin(), fix_colors.end(), 0);
		fix_positions[m_num_edges] = p;
		fix_colors[m_num_edges] = color;
		while(true){
			const auto before = reachable;
			for(size_t i = 0; i < m_num_edges; ++i){
				const auto& e = m_edges[i];
				const uint64_t u = (1ul << e.u), v = (1ul << e.v);
				if(reachable & u){
					if(!(reachable & v)){
						fix_positions[i] = e.v;
						fix_colors[i] = e.color;
						reachable |= v;
					}
				}else if(reachable & v){
					fix_positions[i] = e.u;
					fix_colors[i] = e.color;
					reachable |= u;
				}
			}
			if(reachable == before){ break; }
		}
		for(int i = static_cast<int>(m_num_edges); i >= 0; --i){
			if(fix_colors[i]){
				m_classic_board.put(fix_positions[i], fix_colors[i]);
			}
		}
		size_t tail = 0;
		for(size_t i = 0; i < m_num_edges; ++i){
			const auto& e = m_edges[i];
			if(!(reachable & (1ul << e.u))){ m_edges[tail++] = e; }
		}
		m_num_edges = tail;
	}

	void put(int p, int q, int color){
		assert(!test_entanglement(p, q));
		m_edges[m_num_edges++] = Edge(p, q, color);
	}

	void put_classic(int p, int color){
		m_classic_board.put(p, color);
	}

	void force_put_classic(int p, int color){
		m_classic_board.force_put(p, color);
	}

};

std::ostream& operator<<(std::ostream& os, const State& s){
	char lines[6][7] = { { 0 } };
	for(int i = 0; i < 6; ++i){
		for(int j = 0; j < 6; ++j){
			lines[i][j] = "x.o"[s.classic_board().get(i * 6 + j) + 1];
		}
	}
	for(const auto& e : s.edges()){
		const int ur = e.u / 6, uc = e.u % 6;
		const int vr = e.v / 6, vc = e.v % 6;
		const int c = e.color;
		lines[ur][uc] = lines[vr][vc] = '=';
		os << "(" << "x.o"[c + 1] << ", " << ur << uc << ", " << vr << vc << ") ";
	}
	if(s.edges().empty()){ os << "(no edges)"; }
	os << std::endl;
	for(int i = 0; i < 6; ++i){ os << lines[i] << std::endl; }
	return os;
}


struct History {
	int p;
	int q;
	int select;

	History() : p(0), q(0), select(-1) { }
	History(int p, int q, int s = -1) : p(p), q(q), select(s) { }
};
