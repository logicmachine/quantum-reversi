#pragma once
#include "state.hpp"
#include "random.hpp"

int playout(const State& root){
	using Edge = typename State::Edge;
	ClassicBoard board = root.classic_board();
	int step = board.count(1) + board.count(-1) + root.edges().size();
	// adjacency matrix and edge list
	std::array<uint64_t, 36> graph;
	for(int i = 0; i < 36; ++i){ graph[i] = 0; }
	std::array<Edge, 36> edges;
	int edges_head = 0, edges_tail = 0;
	for(const auto& e : root.edges()){
		const int u = e.u, v = e.v;
		edges[edges_tail++] = e;
		graph[u] |= (1ul << v);
		graph[v] |= (1ul << u);
	}
	// initialize group mapping
	std::array<int, 36> group;
	for(int i = 0; i < 36; ++i){ group[i] = i; }
	for(int i = 0; i < 36; ++i){
		if(group[i] != i){ continue; }
		std::array<int, 36> q;
		int q_head = 0, q_tail = 0;
		q[q_head++] = i;
		while(q_tail < q_head){
			const int u = q[q_tail++];
			for(uint64_t b = graph[u]; b > 0; b &= b - 1){
				const int v = __builtin_ctzll(b);
				if(v != i && group[v] != i){
					group[v] = i;
					q[q_head++] = v;
				}
			}
		}
	}
	for(; step < 36; ++step){
		const int color = 1 - 2 * (step & 1);
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
			board.put(plist[0], color);
			continue;
		}
		// select a pair of cells
		const int k0 = modulus_random(pcount), k1 = modulus_random(pcount - 1);
		const int p = plist[k0], q = plist[k1 + (k1 >= k0)];
		if(group[p] == group[q]){
			// entanglement
			const int sel = (modulus_random(2) ? p : q);
			std::array<int, 36> d;
			for(int i = 0; i < 36; ++i){ d[i] = 36; }
			std::array<int, 36> q;
			int q_head = 0, q_tail = 0;
			q[q_head++] = sel;
			d[sel] = 0;
			while(q_tail < q_head){
				const int u = q[q_tail++];
				for(uint64_t b = graph[u]; b > 0; b &= b - 1){
					const int v = __builtin_ctzll(b);
					if(d[v] == 36){
						d[v] = d[u] + 1;
						q[q_head++] = v;
					}
				}
			}
			const int before_head = edges_head;
			edges_head = edges_tail;
			board.put(sel, color);
			for(int i = edges_tail - 1; i >= before_head; --i){
				const auto& e = edges[i];
				const int u = e.u, v = e.v;
				if(d[u] < d[v]){
					board.put(v, e.color);
				}else if(d[u] > d[v]){
					board.put(u, e.color);
				}else{
					edges[--edges_head] = e;
				}
			}
		}else{
			// put quantum-stone
			edges[edges_tail++] = Edge(p, q, color);
			graph[p] |= (1ul << q);
			graph[q] |= (1ul << p);
			const int gp = group[p], gq = group[q];
			for(int i = 0; i < 36; ++i){
				if(group[i] == gq){ group[i] = gp; }
			}
		}
	}

	const int black = board.count( 1);
	const int white = board.count(-1);
	if(black > white){
		return 1;
	}else if(black < white){
		return -1;
	}else{
		return 0;
	}
}
