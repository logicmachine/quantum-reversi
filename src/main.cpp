#include <iostream>
#include <utility>
#include <cassert>
#include "state.hpp"
#include "mcts.hpp"
#include "json.hpp"

static State parse_state(const nlohmann::json& obj){
	State state;
	const auto& board = obj["board"];
	for(int i = 0; i < BOARD_SIZE; ++i){
		const std::string c = board[i];
		if(c == "o"){
			state.classic[0].set(i);
		}else if(c == "x"){
			state.classic[1].set(i);
		}
	}
	const auto& moves = obj["moves"];
	int num_moves = moves.size();
	if(obj["action"] == "select"){ --num_moves; }
	for(int step = 4; step < num_moves; ++step){
		const auto& move = moves[step];
		const auto p = static_cast<int>(move[0][0]);
		const auto q = static_cast<int>(move[0][1]);
		const auto type = static_cast<int>(move[1]);
		if(state.classic[0](p) || state.classic[1](p)){ continue; }
		if(state.classic[0](q) || state.classic[1](q)){ continue; }
		if(type < 0){ state.put_quantum(p, q, step); }
	}
	return state;
}

static std::pair<int, int> parse_entanglement(const nlohmann::json& obj){
	const auto& entanglement = obj["entanglement"];
	return std::make_pair(
		static_cast<int>(entanglement[0]),
		static_cast<int>(entanglement[1]));
}

int main(){
	std::ios_base::sync_with_stdio(false);

	int self_color = 0;
	{	// init
		std::string line;
		std::getline(std::cin, line);
		const nlohmann::json obj = nlohmann::json::parse(line);
		assert(obj["action"] == "init");
		self_color = static_cast<int>(obj["index"]);
		std::cout << std::endl;
	}

	int step = 4 + self_color;
	MCTSSolver solver;
	while(true){
		std::string line;
		std::getline(std::cin, line);
		const nlohmann::json obj = nlohmann::json::parse(line);
		const auto& action = obj["action"];
		if(action == "quit"){
			std::cout << std::endl;
			break;
		}else if(action == "play"){
			const auto root = parse_state(obj);
			const auto ret = solver.play(root, step);
			std::cout << "{\"positions\":[" << ret.first << "," << ret.second << "]}" << std::endl;
			step += 2;
		}else if(action == "select"){
			const auto root = parse_state(obj);
			const auto entanglement = parse_entanglement(obj);
			const auto ret = solver.select(
				root, entanglement.first, entanglement.second, step - 1);
			std::cout << "{\"select\":" << ret << "}" << std::endl;
		}
	}
#if 0
	State state;
	state.put_classic(15, 0);
	state.put_classic(14, 1);
	state.put_classic(20, 0);
	state.put_classic(21, 1);
	for(int step = 4; step < BOARD_SIZE; ++step){
		std::cout << state;
		std::cout << "xo"[step & 1] << " >> " << std::flush;
		std::string ps, qs;
		std::cin >> ps >> qs;
		const int p = (ps[1] - '1') * BOARD_WIDTH + (ps[0] - 'a');
		const int q = (qs[1] - '1') * BOARD_WIDTH + (qs[0] - 'a');
		if(state.test_entanglement(p, q)){
			std::string select;
			do {
				std::cout << "[" << ps << ", " << qs << "] >> " << std::flush;
				std::cin >> select;
			} while(select != ps && select != qs);
			const int s = (select[1] - '1') * BOARD_WIDTH + (select[0] - 'a');
			state.select_entanglement(s, step);
		}else{
			state.put_quantum(p, q, step);
		}
		std::cout << std::endl;
	}
#endif
	return 0;
}
