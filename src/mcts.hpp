#pragma once
#include <vector>
#include <random>
#include <limits>
#include <memory>
#include <cmath>
#include "state.hpp"

static std::random_device g_random_device;

class MCTSSolver {

private:
	static constexpr int NUM_PLAYOUTS = 100000;
	static constexpr int EXPAND_THRESHOLD = 200;

	struct Node {
		Node *parent;
		std::vector<std::unique_ptr<Node>> children;

		State state;
		int step;

		int num_wins;
		int num_playouts;

		Node()
			: parent(nullptr)
			, children()
			, state()
			, step(0)
			, num_wins(0)
			, num_playouts(0)
		{ }

		double ucb_score(int total_playouts) const {
			if(num_playouts == 0){ return std::numeric_limits<double>::infinity(); }
			const double mean = static_cast<double>(num_wins) / num_playouts;
			const double bias = sqrt(2 * log(total_playouts) / num_playouts);
			return mean + bias;
		}
	};

	std::default_random_engine m_engine;
	int m_self_color;

	std::vector<int> enumerate_places(const State& state) const {
		std::vector<int> places;
		places.reserve(BOARD_SIZE);
		for(int i = 0; i < BOARD_SIZE; ++i){
			if(state.classic[0](i)){ continue; }
			if(state.classic[1](i)){ continue; }
			places.push_back(i);
		}
		return places;
	}

	std::vector<std::pair<int, int>> enumerate_hands(const State& state) const {
		const auto places = enumerate_places(state);
		if(places.size() == 1){
			return { std::make_pair(places[0], places[0]) };
		}else{
			std::vector<std::pair<int, int>> hands;
			hands.reserve(places.size() * (places.size() - 1) / 2);
			for(const auto p : places){
				for(const auto q : places){
					if(p < q){ hands.emplace_back(p, q); }
				}
			}
			return hands;
		}
	}

	int playout(State state, int step){
		for(; step < BOARD_SIZE; ++step){
			const auto places = enumerate_places(state);
			std::uniform_int_distribution<int> dist(0, places.size() - 1);
			int p = places[0], q = places[0];
			if(places.size() > 1){
				p = places[dist(m_engine)];
				do {
					q = places[dist(m_engine)];
				} while(p == q);
			}
			if(state.test_entanglement(p, q)){
				if(m_engine() % 2 == 0){
					state.select_entanglement(p, step);
				}else{
					state.select_entanglement(q, step);
				}
			}else{
				state.put_quantum(p, q, step);
			}
		}
		const int black = state.classic[0].count();
		const int white = state.classic[1].count();
		if(black > white){
			return 0;
		}else if(black < white){
			return 1;
		}else{
			return -1;
		}
	}

	void expand(Node *node) const {
		assert(node->children.empty());
		const auto hands = enumerate_hands(node->state);
		for(const auto pq : hands){
			const int p = pq.first, q = pq.second;
			if(node->state.test_entanglement(p, q)){
				auto child = std::make_unique<Node>();
				child->parent = node;
				child->state = node->state;
				child->step = node->step + 1;
				{	// select p
					auto child_p = std::make_unique<Node>();
					child_p->parent = child.get();
					child_p->state = node->state;
					child_p->state.select_entanglement(p, node->step);
					child_p->step = node->step + 1;
					child->children.push_back(std::move(child_p));
				}
				if(p != q){ // select q
					auto child_q = std::make_unique<Node>();
					child_q->parent = child.get();
					child_q->state = node->state;
					child_q->state.select_entanglement(q, node->step);
					child_q->step = node->step + 1;
					child->children.push_back(std::move(child_q));
				}
				node->children.push_back(std::move(child));
			}else{
				auto child = std::make_unique<Node>();
				child->parent = node;
				child->state = node->state;
				child->state.put_quantum(p, q, node->step);
				child->step = node->step + 1;
				node->children.push_back(std::move(child));
			}
		}
	}

	int update(Node *node){
		int result = -1;
		if(node->children.empty()){
			if(node->num_playouts >= EXPAND_THRESHOLD){ expand(node); }
		}
		if(node->children.empty()){
			assert(node->parent);
			const int color = (node->parent->step & 1);
			result = playout(node->state, node->step);
			if(result >= 0){ result = (result == color ? 1 : 0); }
		}else{
			int total_playouts = 0;
			for(auto& child : node->children){
				total_playouts += child->num_playouts;
			}
			double best_score = -std::numeric_limits<double>::infinity();
			Node *best = nullptr;
			for(auto& child : node->children){
				const auto score = child->ucb_score(total_playouts);
				if(score > best_score){
					best_score = score;
					best = child.get();
				}
			}
			result = update(best);
			if(node->parent && node->step != node->parent->step){
				if(result >= 0){ result = 1 - result; }
			}
		}
		if(node->parent){
			node->num_wins += (result == 1 ? 1 : 0);
			node->num_playouts += 1;
		}
		return result;
	}

public:
	MCTSSolver()
		: m_engine(g_random_device())
	{ }

	std::pair<int, int> play(const State& root, int step){
		const auto hands = enumerate_hands(root);
		auto node = std::make_unique<Node>();
		node->state = root;
		node->step = step;
		expand(node.get());
		for(int iter = 0; iter < NUM_PLAYOUTS; ++iter){
			update(node.get());
		}
		double best_score = -std::numeric_limits<double>::infinity();
		std::pair<int, int> best_hand;
		for(size_t i = 0; i < hands.size(); ++i){
			const auto& child = node->children[i];
			if(child->num_playouts == 0){ continue; }
			const double score =
				static_cast<double>(child->num_wins) / child->num_playouts;
			if(score > best_score){
				best_score = score;
				best_hand = hands[i];
			}
		}
		return best_hand;
	}

	int select(const State& root, int p, int q, int step){
		if(p == q){ return p; }
		assert(root.test_entanglement(p, q));
		auto node = std::make_unique<Node>();
		node->state = root;
		node->step = step + 1;
		{	// select p
			auto child_p = std::make_unique<Node>();
			child_p->parent = node.get();
			child_p->state = root;
			child_p->state.select_entanglement(p, step);
			child_p->step = step + 1;
			node->children.push_back(std::move(child_p));
		}
		{	// select q
			auto child_q = std::make_unique<Node>();
			child_q->parent = node.get();
			child_q->state = root;
			child_q->state.select_entanglement(q, step);
			child_q->step = step + 1;
			node->children.push_back(std::move(child_q));
		}
		for(int iter = 0; iter < NUM_PLAYOUTS; ++iter){
			update(node.get());
		}
		const auto p_node = node->children[0].get();
		const auto q_node = node->children[1].get();
		const double p_score =
			static_cast<double>(p_node->num_wins) / p_node->num_playouts;
		const double q_score =
			static_cast<double>(q_node->num_wins) / q_node->num_playouts;
		if(p_score > q_score){
			return p;
		}else{
			return q;
		}
	}

};

