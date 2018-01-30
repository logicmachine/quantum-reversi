#pragma once
#include <vector>
#include <limits>
#include <memory>
#include <chrono>
#include <cmath>
#include <cassert>
#include "state.hpp"
#include "random.hpp"
#include "playout.hpp"

namespace mcts {

static constexpr int NUM_PLAYOUTS = 40000;
static constexpr int PLAYOUT_BLOCK_SIZE = 100;
static constexpr int PLAYOUT_SCALE = 4;
static constexpr int EXPAND_THRESHOLD = 80;
static constexpr double TIME_LIMIT = 9.8;
static constexpr double TIME_PER_TURN = 0.2;

struct Move {
	int p, q;
	Move() : p(0), q(0) { }
	Move(int p, int q) : p(p), q(q) { }
};

class MCTSNode {

public:
	using pointer_type = std::unique_ptr<MCTSNode>;

private:
	MCTSNode *m_parent;
	std::vector<pointer_type> m_children;

	State m_state;
	int m_last_color;
	Move m_last_move;
	bool m_has_entanglement;

	int m_num_wins;
	int m_num_playouts;

public:
	MCTSNode()
		: m_parent(nullptr)
		, m_children()
		, m_state()
		, m_last_color(0)
		, m_last_move()
		, m_has_entanglement(false)
		, m_num_wins(0)
		, m_num_playouts(0)
	{ }

	MCTSNode(
		MCTSNode *parent,
		State state,
		int last_color,
		Move last_move,
		bool has_entanglement)
		: m_parent(parent)
		, m_children()
		, m_state(state)
		, m_last_color(last_color)
		, m_last_move(last_move)
		, m_has_entanglement(has_entanglement)
		, m_num_wins(0)
		, m_num_playouts(0)
	{ }

	void expand(){
		if(!m_children.empty()){ return; }
		const auto& board = m_state.classic_board();
		const auto& last_move = m_last_move;
		if(board.count(1) + board.count(-1) == 36){
			// this is a leaf
		}else if(m_has_entanglement){
			// select entanglement
			const int next_color = m_last_color * -1;
			const int p = last_move.p, q = last_move.q;
			{	// select p
				State s = m_state;
				s.select_entanglement(p, next_color);
				m_children.push_back(std::make_unique<MCTSNode>(
					this, s, next_color, Move(p, p), false));
			}
			{	// select q
				State s = m_state;
				s.select_entanglement(q, next_color);
				m_children.push_back(std::make_unique<MCTSNode>(
					this, s, next_color, Move(q, q), false));
			}
		}else{
			// put quantum-stone
			const int next_color =
				m_last_color * (last_move.p == last_move.q ? 1 : -1);
			// list unoccupied cells
			const uint64_t unused =
				((1ul << 36) - 1ul) & ~(board.bitmap(1) | board.bitmap(-1));
			std::array<int, 36> plist;
			int pcount = 0;
			for(uint64_t b = unused; b > 0; b &= b - 1){
				plist[pcount++] = __builtin_ctzll(b);
			}
			// check for the last turn
			if(pcount == 1){
				const int p = plist[0];
				State s = m_state;
				s.select_entanglement(p, next_color);
				m_children.push_back(std::make_unique<MCTSNode>(
					this, s, next_color, Move(p, p), true));
				return;
			}
			// enumerate all valid moves
			for(int i = 0; i < pcount; ++i){
				for(int j = i + 1; j < pcount; ++j){
					const int p = std::min(plist[i], plist[j]);
					const int q = std::max(plist[i], plist[j]);
					if(m_state.test_entanglement(p, q)){
						// entanglement
						m_children.push_back(std::make_unique<MCTSNode>(
							this, m_state, next_color, Move(p, q), true));
					}else{
						// put quantum-stones
						State s = m_state;
						s.put(p, q, next_color);
						m_children.push_back(std::make_unique<MCTSNode>(
							this, s, next_color, Move(p, q), false));
					}
				}
			}
		}
	}

	std::array<int, 3> update(){
		std::array<int, 3> result_counter = { 0, 0, 0 };
		if(m_children.empty() && m_num_playouts == EXPAND_THRESHOLD){
			expand();
		}
		if(m_children.empty()){
			// random playout
			for(int i = 0; i < PLAYOUT_SCALE; ++i){
				++result_counter[playout(m_state) + 1];
			}
		}else if(m_num_playouts < m_children.size()){
			// run playout on unprocessed node
			result_counter = m_children[m_num_playouts]->update();
		}else{
			// count number of playouts in this subtree
			int total_playouts = m_num_playouts;
			// select a node having best UCB1 score
			double best_score = -std::numeric_limits<double>::infinity();
			MCTSNode *best_node = nullptr;
			for(auto& child : m_children){
				const auto score = child->ucb_score(total_playouts);
				if(score > best_score){
					best_score = score;
					best_node = child.get();
				}
			}
			// run playout
			result_counter = best_node->update();
		}
		m_num_playouts += PLAYOUT_SCALE;
		m_num_wins += result_counter[m_last_color + 1];
		return result_counter;
	}

	double ucb_score(int total_playouts) const {
		if(m_num_playouts == 0){
			return std::numeric_limits<double>::infinity();
		}
		const double r = static_cast<double>(m_num_wins) / m_num_playouts;
		const double x = log(total_playouts) / m_num_playouts;
		const double y = std::min(0.25, r - r * r + sqrt(2.0 * x));
		return r + sqrt(x * y);
	}

	const Move& last_move() const {
		return m_last_move;
	}

	int num_wins() const {
		return m_num_wins;
	}

	int num_playouts() const {
		return m_num_playouts;
	}

	Move select_best_move() const {
		if(m_children.empty()){ return Move(-1, -1); }
		double best_score = -std::numeric_limits<double>::infinity();
		MCTSNode *best_node = nullptr;
		for(auto& child : m_children){
			const auto score =
				static_cast<double>(child->num_wins()) / child->num_playouts();
			if(score > best_score){
				best_score = score;
				best_node = child.get();
			}
		}
		return best_node->m_last_move;
	}

};

class MCTSSolver {

private:
	std::chrono::duration<double> m_remaining_time;

	void update_loop(MCTSNode& root){
		const auto start_time = std::chrono::steady_clock::now();
		const auto break_time = start_time + m_remaining_time * TIME_PER_TURN;
		auto last_time = start_time;
		int debug = 0;
		do {
			for(int i = 0; i < PLAYOUT_BLOCK_SIZE; ++i){ root.update(); ++debug; }
			last_time = std::chrono::steady_clock::now();
		} while(last_time < break_time);
		m_remaining_time -= last_time - start_time;
	}

public:
	MCTSSolver()
		: m_remaining_time(TIME_LIMIT)
	{ }

	std::pair<int, int> play(
		const State& root, int step, const std::vector<History>& history)
	{
		if(step == 4){
			// shortcut: first step
			return std::make_pair(0, 35);
		}else if(step == 5){
			// shortcut: second step
			int used[36] = { 0 };
			for(const auto& h : history){ used[h.p] = used[h.q] = 1; }
			const std::pair<int, int> candidates[] = {
				std::make_pair<int, int>(5, 30),
				std::make_pair<int, int>(0, 35),
				std::make_pair<int, int>(0, 5),
				std::make_pair<int, int>(0, 30),
				std::make_pair<int, int>(5, 35),
				std::make_pair<int, int>(30, 35)
			};
			for(const auto& p : candidates){
				if(used[p.first] == 0 && used[p.second] == 0){ return p; }
			}
		}
		const int color = 1 - 2 * (step & 1);
		auto node = std::make_unique<MCTSNode>(
			nullptr, root, color, Move(), false);
		node->expand();
		update_loop(*node);
		const auto best = node->select_best_move();
		return std::make_pair(best.p, best.q);
	}

	int select(
		const State& root, int p, int q, int step, const std::vector<History>& history)
	{
		const int color = 1 - 2 * (step & 1);
		auto node = std::make_unique<MCTSNode>(
			nullptr, root, color, Move(p, q), true);
		node->expand();
		update_loop(*node);
		const auto best = node->select_best_move();
		return best.p;
	}

};

}
