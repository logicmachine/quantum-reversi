// akemi's Quantum Reversi Solver (C) 2017 Fixstars Corp.
// g++ akemi.cpp -std=c++14 -o akemi -O3 -Wall -Wno-unused-but-set-variable

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>

#include "src/json.hpp"

constexpr const char* version  = "0.06";
constexpr const char* revision = "a";
constexpr const char* ver_date = "20171201";

std::pair<int, int> pos2xy(int idx, int w)
{
	return std::make_pair(idx % w, idx / w);
}

int xy2pos(int x, int y, int w)
{
	return x + y * w;
}

void remove_newline(std::string& s)
{
	std::string target("\n");
	std::string::size_type pos = s.find(target);
	while (pos != std::string::npos) {
		s.replace(pos, target.size(), "");
		pos = s.find(target, pos);
	}
}

void solver(std::vector<int>& cells, int w, int h)
{
	std::sort(cells.begin(), cells.end(), [w, h](int a, int b) {
			auto xy1 = pos2xy(a, w);
			auto xy2 = pos2xy(b, w);
			auto f = [](int a, int b){ int x = a - b; return x < 0 ? ~x : x; };
			int p1 = f(xy1.first, w / 2) + f(xy1.second, h / 2);
			int p2 = f(xy2.first, w / 2) + f(xy2.second, h / 2);
			return p1 > p2;
		});
}

int main(int argc, char* argv[])
{
	std::string name, opp_name;
	std::string s;
	std::vector<std::string> data;
	int w = 0;
	int h = 0;

	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());
	std::uniform_int_distribution<> coin(0, 1);

	std::string white_disc = "o";
	std::string black_disc = "x";
	std::string quantum_disc = "=";
	std::string empty_disc = " ";

	for (;;) {
		getline(std::cin, s);
		nlohmann::json obj = nlohmann::json::parse(s);
		auto action = obj["action"];
			
		if (action == "play") {
			auto board(obj["board"]);
			std::vector<int> cells;
			for (int i = 0; i < static_cast<int>(board.size()); i++) {
				std::string c = board[i];
				if (c != white_disc && c != black_disc) cells.push_back(i);
			}
			
			solver(cells, w, h);

			nlohmann::json out;
			std::vector<int> positions;
			for (int i = 0; i < 2; i++)
				positions.push_back(cells[cells.size() > 1 ? i : 0]);
			out["positions"] = positions;
			std::string json(out.dump());
			remove_newline(json);
			std::cout << json << std::endl << std::flush;

		} else if (action == "select") {
			auto entanglement(obj["entanglement"]);
			int select = entanglement[coin(engine)];
			nlohmann::json out;
			out["select"] = select;
			std::string json(out.dump());
			remove_newline(json);
			std::cout << json << std::endl << std::flush;

		} else if (action == "init") {
			int index(obj["index"]);
			auto names(obj["names"]);
			auto name(names[index]);
			w = obj["size"][0];
			h = obj["size"][1];
			white_disc = obj["white"];
			black_disc = obj["black"];
			quantum_disc = obj["quantum"];
			empty_disc = obj["empty"];
			std::cout << std::endl << std::flush;

		} else if (action == "quit") {
			std::cout << std::endl << std::flush;
			break;
		}
	}
	
	return 0;
}
