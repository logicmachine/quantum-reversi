#pragma once
#include <vector>
#include <random>
#include "state.hpp"

class RandomSolver {

private:
	std::default_random_engine m_engine;

public:
	RandomSolver(){ }

	std::pair<int, int> play(const State& root, int step){
		std::vector<int> places;
		for(int i = 0; i < BOARD_SIZE; ++i){
			if(root.classic[0](i) || root.classic[1](i)){ continue; }
			places.push_back(i);
		}
		if(places.size() == 1){
			return std::make_pair(places[0], places[0]);
		}
		std::shuffle(places.begin(), places.end(), m_engine);
		return std::make_pair(places[0], places[1]);
	}

	int select(const State& root, int p, int q, int step){
		if(m_engine() % 2 == 0){
			return p;
		}else{
			return q;
		}
	}

};
