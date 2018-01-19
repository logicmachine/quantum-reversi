#include <iostream>
#include <utility>
#include <random>
#include <cassert>
#include "state.hpp"
#include "mcts.hpp"
#include "json.hpp"

static std::random_device g_random_device;

static State parse_state(const nlohmann::json& obj){
	State state;
	const auto& board = obj["board"];
	for(int i = 0; i < 36; ++i){
		const std::string c = board[i];
		if(c == "o"){
			state.force_put_classic(i, 1);
		}else if(c == "x"){
			state.force_put_classic(i, -1);
		}
	}
	const auto& moves = obj["moves"];
	int num_moves = moves.size();
	if(obj["action"] == "select"){ --num_moves; }
	for(int step = 4; step < num_moves; ++step){
		const auto& move = moves[step];
		const auto& b = state.classic_board();
		const auto p = static_cast<int>(move[0][0]);
		const auto q = static_cast<int>(move[0][1]);
		const auto type = static_cast<int>(move[1]);
		if(b.get(p) || b.get(q)){ continue; }
		const int color = 1 - 2 * (step & 1);
		if(type < 0){ state.put(p, q, color); }
	}
	return state;
}

static std::pair<int, int> parse_entanglement(const nlohmann::json& obj){
	const auto& entanglement = obj["entanglement"];
	return std::make_pair(
		static_cast<int>(entanglement[0]),
		static_cast<int>(entanglement[1]));
}

static std::vector<History> parse_history(const nlohmann::json& obj){
	const auto& moves = obj["moves"];
	int num_moves = moves.size();
	std::vector<History> history;
	for(int i = 0; i < num_moves; ++i){
		const auto& move = moves[i];
		int p = static_cast<int>(move[0][0]);
		int q = static_cast<int>(move[0][1]);
		int select = static_cast<int>(move[1]);
		if(p > q){
			std::swap(p, q);
			if(select >= 0){ select = 1 - select; }
		}
		history.emplace_back(p, q, select);
	}
	return history;
}

int main(){
	std::ios_base::sync_with_stdio(false);
	set_seed(g_random_device());

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
	mcts::MCTSSolver solver;
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
			const auto history = parse_history(obj);
			const auto ret = solver.play(root, step, history);
			std::cout << "{\"positions\":[" << ret.first << "," << ret.second << "]}" << std::endl;
			step += 2;
		}else if(action == "select"){
			const auto root = parse_state(obj);
			const auto history = parse_history(obj);
			const auto entanglement = parse_entanglement(obj);
			const auto ret = solver.select(
				root, entanglement.first, entanglement.second, step - 1, history);
			std::cout << "{\"select\":" << ret << "}" << std::endl;
		}
	}
	return 0;
}
