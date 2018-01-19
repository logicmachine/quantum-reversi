#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""In-house Contest 2017: Quantum Reversi (C) 2017 Fixstars Corp.
"""

import sys
import os
import time
import subprocess
import threading
import shlex
import random
import json

VERSION  = "0.21"
REVISION = "a"
VER_DATE = "20171209"

VISIBLE_BOARD = True
TIME_LIMIT = 1000.0

BLACK_DISC = "o"
WHITE_DISC = "x"
QUANTUM_DISC = "="
EMPTY_DISC = "_"
DISPLAY_DISCS = {x: x for x in (BLACK_DISC, WHITE_DISC, QUANTUM_DISC, EMPTY_DISC)}
#DISPLAY_DISCS = {BLACK_DISC: "\u25cf", WHITE_DISC: "\u25cb", QUANTUM_DISC: "\u269b", EMPTY_DISC: "\u2423"}

def read_response(player, player_time):
    start_time = time.time()
    class ExecThread(threading.Thread):
        def __init__(self, player):
            self.player = player
            self.response = None
            threading.Thread.__init__(self)
        def run(self):
            self.response = self.player.stdout.readline().strip()
        def get_response(self):
            return self.response
    t = ExecThread(player)
    t.setDaemon(True)
    t.start()
    if t.isAlive(): t.join(player_time)
    player_time -= time.time() - start_time
    return t.get_response(), player_time

def check_TLE(players, name, player_time):
    if player_time <= 0.0:
        print("Error: time limit exceeded: %s" % (name,), file=sys.stderr)
        return True
    return False

def quit_game(players, player_times):
    for idx in range(2):
        try:
            players[idx].stdin.write(("%s\n" % json.dumps({"action": "quit"})).encode("utf-8"))
            players[idx].stdin.flush()
            response, player_times[idx] = read_response(players[idx], player_times[idx])
        except:
            pass

class QuantumReversi:
    """Quantum Reversi class
    """
    def __init__(self, players):
        self.players = players
        self.player_times = [TIME_LIMIT for i in range(2)]
        self.turn = 0
        self.player = 0
        self.w = 6
        self.h = 6
        self.size = self.w * self.h
        self.board = [EMPTY_DISC for x in range(self.w * self.h)] # [y*w+x]
        self.moves = []
        self.solvers = []
        self.names = []
        self.entanglement = []
        for name, solver in self.players:
            self.solvers.append(subprocess.Popen(solver, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE))
            self.names.append(name)
        self.winner = None
        self.msg = None

        # initial positions
        x = self.w // 2 - 1
        y = self.h // 2 - 1
        self.moves.append([(self.xy2pos(x+1, y  ), self.xy2pos(x+1, y  )), 0])
        self.moves.append([(self.xy2pos(x  , y  ), self.xy2pos(x  , y  )), 0])
        self.moves.append([(self.xy2pos(x  , y+1), self.xy2pos(x  , y+1)), 0])
        self.moves.append([(self.xy2pos(x+1, y+1), self.xy2pos(x+1, y+1)), 0])
        self.board[self.xy2pos(x+1, y  )] = BLACK_DISC
        self.board[self.xy2pos(x  , y  )] = WHITE_DISC
        self.board[self.xy2pos(x  , y+1)] = BLACK_DISC
        self.board[self.xy2pos(x+1, y+1)] = WHITE_DISC

    def initialize(self):
        for idx in range(2):
            try:
                data = json.dumps({"action": "init", "index": idx, "names": self.names, "size": (self.w, self.h), "board": self.board, "moves": self.moves, "black": BLACK_DISC, "white": WHITE_DISC, "quantum": QUANTUM_DISC, "empty": EMPTY_DISC})
                self.solvers[idx].stdin.write(("%s\n" % data).encode("utf-8"))
                self.solvers[idx].stdin.flush()
                response, self.player_times[idx] = read_response(self.solvers[idx], self.player_times[idx])
                if check_TLE(self.solvers, self.names[idx], self.player_times[idx]):
                    self.winner = idx^1
                    self.msg = "%s's program got time limit exceeded" % self.names[idx]
                    tle_player = (idx, self.names[idx])
                    quit_game(self.solvers, self.player_times)
                    return False
            except Exception as e:
                self.winner = idx^1
                self.msg = "%s's program was stopped in the init command: %s" % (self.names[idx], str(e))
                quit_game(self.solvers, self.player_times)
                return False
        return True

    def play(self):
        """play a game
        """
        idx = 0
        while self.board.count(BLACK_DISC) + self.board.count(WHITE_DISC) < self.w * self.h:
            try:
                data = json.dumps({"action": "play", "board": self.board, "moves": self.moves})
                self.solvers[idx].stdin.write(("%s\n" % data).encode("utf-8"))
                self.solvers[idx].stdin.flush()
                response, self.player_times[idx] = read_response(self.solvers[idx], self.player_times[idx])
                if check_TLE(self.solvers, self.names[idx], self.player_times[idx]):
                    self.winner = idx^1
                    self.msg = "%s's program got time limit exceeded" % self.names[idx]
                    tle_player = (idx, self.names[idx])
                    quit_game(self.solvers, self.player_times)
                    return
                data = json.loads(response.decode("utf-8"))
                pos1, pos2 = map(int, data["positions"])
                cyclic = self.move(pos1, pos2)
                if cyclic == False:
                    self.winner = idx^1
                    self.msg = "{}'s program moved invalid position: {}".format(self.names[idx], (pos1, pos2))
                    tle_player = (idx, self.names[idx])
                    quit_game(self.solvers, self.player_times)
                    return
                if cyclic:
                    self.entangle(cyclic)  # update entanglement
                idx ^= 1
                if self.entanglement:
                    _, qvalues = self.entanglement[0]
                    data = json.dumps({"action": "select", "entanglement": qvalues, "board": self.board, "moves": self.moves})
                    self.solvers[idx].stdin.write(("%s\n" % data).encode("utf-8"))
                    self.solvers[idx].stdin.flush()
                    response, self.player_times[idx] = read_response(self.solvers[idx], self.player_times[idx])
                    if check_TLE(self.solvers, self.names[idx], self.player_times[idx]):
                        self.winner = idx^1
                        self.msg = "%s's program got time limit exceeded" % self.names[idx]
                        tle_player = (idx, self.names[idx])
                        quit_game(self.solvers, self.player_times)
                        return
                    data = json.loads(response.decode("utf-8"))
                    self.select(qvalues.index(int(data["select"])))

            except Exception as e:
                self.winner = idx^1
                self.msg = "%s's program was stopped in the play command: %s" % (self.names[idx], str(e))
                quit_game(self.solvers, self.player_times)
                return

            if VISIBLE_BOARD:
                self.show_board()
                print()

            self.entanglement = []
        quit_game(self.solvers, self.player_times)

    def pos2xy(self, idx):
        """convert position to xy (2-tuple)
        """
        return idx % self.w, idx // self.w

    def xy2pos(self, x, y):
        """convert xy (2-tuple) to position
        """
        return x + y * self.w

    def move(self, pos1, pos2):
        """set superposition on the board
        """
        qs = sorted([pos if type(pos) == int else self.xy2pos(pos[0], pos[1]) for pos in (pos1, pos2)])
        if not self.validate(qs):
            print("Error: invalid positions", file=sys.stderr)
            return False
        for pos in qs:
            self.board[pos] = QUANTUM_DISC
        if len(qs) == 2:
            self.moves.append([tuple(qs), -1])
        self.player ^= 1
        self.turn += 1
        return self.evaluate(tuple(qs))

    def validate(self, qs):
        """validate positions of discs on the board
        """
        if len(qs) != 2:
            return False
        if qs[0] == qs[1]:
            if self.board.count(BLACK_DISC) + self.board.count(WHITE_DISC) != self.w * self.h - 1:
                return False
        if self.board[qs[0]] in (BLACK_DISC, WHITE_DISC) or self.board[qs[1]] in (BLACK_DISC, WHITE_DISC):
            return False
        return True

    def dfs(self, G, node, goal, visited):
        """DFS for cyclic entanglements
        """
        visited.append(node)
        for next in G[node][:]:
            G[node].remove(next)
            G[next].remove(node)
            if next == goal:
                return visited
            p = self.dfs(G, next, goal, visited)
            if p: return p
            G[node].append(next)
            G[next].append(node)
        visited.pop()
        return None

    def flip(self, disc, opponent, x, dx, y, dy):
        """flip discs
        """
        has_flips = False
        flip_discs = []
        while True:
            x += dx
            y += dy
            if x < 0 or x >= self.w or y < 0 or y >= self.h:
                break
            p = self.xy2pos(x, y)
            if opponent == self.board[p]:
                flip_discs.append(p)
            elif disc == self.board[p]:
                has_flips = True
                break
            else:
                break
        if has_flips:
            for p in flip_discs:
                self.board[p] = disc

    def check_flips(self, pos, idx):
        """check discs to flip
        """
        idx &= 1
        disc = (BLACK_DISC, WHITE_DISC)[idx]
        opponent = (BLACK_DISC, WHITE_DISC)[idx^1]
        x, y = self.pos2xy(pos)
        self.flip(disc, opponent, x, 1, y, 0)
        self.flip(disc, opponent, x, -1, y, 0)
        self.flip(disc, opponent, x, 0, y, 1)
        self.flip(disc, opponent, x, 0, y, -1)
        self.flip(disc, opponent, x, 1, y, 1)
        self.flip(disc, opponent, x, -1, y, 1)
        self.flip(disc, opponent, x, 1, y, -1)
        self.flip(disc, opponent, x, -1, y, -1)

    def select(self, pos):
        """select one from two discs for cyclic entanglement
        """
        if not self.entanglement:
            return False
        idx, qvalues = self.entanglement[0]
        self.moves[idx][1] = pos  # 0 or 1
        check = [qvalues[self.moves[idx][1]]]
        self.board[self.moves[idx][0][self.moves[idx][1]]] = (BLACK_DISC, WHITE_DISC)[idx&1]
        self.check_flips(self.moves[idx][0][self.moves[idx][1]], idx)

        has_entanglement = True
        while has_entanglement:
            has_entanglement = False
            for idx, qvalues in self.entanglement:
                if self.moves[idx][1] < 0:
                    xs = list(set(self.moves[idx][0]) - set(check))
                    if len(xs) == 1:
                        self.moves[idx][1] = self.moves[idx][0].index(xs[0])
                        check.append(xs[0])
                        has_entanglement = True
        for idx, qvalues in self.entanglement[1:]:
            self.board[self.moves[idx][0][self.moves[idx][1]]] = (BLACK_DISC, WHITE_DISC)[idx&1]
            self.check_flips(self.moves[idx][0][self.moves[idx][1]], idx)
        return True

    def entangle(self, cyclic):
        """check cyclic entanglements and collapse to classical values
        """
        paths = []
        entireties = cyclic[:]  # classical values
        for i in range(len(entireties) - 1):
            paths.append(tuple(sorted([entireties[i], entireties[i+1]])))
        paths.append(tuple(sorted([entireties[0], entireties[-1]])))

        has_entirety = True
        while has_entirety:
            has_entirety = False
            for c in entireties[:]:
                for e, state in self.moves:
                    if c in e and e not in paths:
                        paths.append(e)
                        entireties.append((set(e) - {c}).pop())
                        has_entirety = True
        
        self.entanglement = []
        for i, (e, state) in enumerate(self.moves):
            if e in paths:
                self.entanglement.append((i, e))
        self.entanglement.sort(reverse=True)

    def evaluate(self, pos):
        """check and evaluate discs on the board
        """
        G = [[] for x in range(self.w * self.h)]
        for qvalues, state in self.moves:
            G[qvalues[0]].append(qvalues[1])
            G[qvalues[1]].append(qvalues[0])
        cyclic = self.dfs(G, min(pos), min(pos), [])
        return cyclic

    def show_board(self):
        """show the board
        """
        n = len(self.moves)
        print("Step %03d: %s [%s]  move: %d, %d" % ((n, self.names[~n&1], (DISPLAY_DISCS[BLACK_DISC], DISPLAY_DISCS[WHITE_DISC])[~n&1]) + self.moves[-1][0]))
        for pos, cell in enumerate(self.board):
            if pos % self.w == 0: print(";", end="")
            print("%s" % DISPLAY_DISCS[cell], end="")
            if (pos + 1) % self.w  == 0: print()
        if self.entanglement:
            print("Cyclic entanglement:")
            for e in self.entanglement:
                print(" {}".format(e))
        print("State  %s: %d, %s: %d" % (DISPLAY_DISCS[BLACK_DISC], self.board.count(BLACK_DISC), DISPLAY_DISCS[WHITE_DISC], self.board.count(WHITE_DISC)))

    def show_moves(self):
        """show moves
        """
        for i, (qvalues, idx) in enumerate(self.moves):
            print(i, qvalues, idx)

    def show_result(self):
        """show result
        """
        if self.winner in (0, 1):
            print(self.msg)
            print("### No score")
            print("### Winner: {}".format(self.names[self.winner]))
        else:
            score0 = self.board.count(BLACK_DISC)
            score1 = self.board.count(WHITE_DISC)
            self.winner = 0 if score0 > score1 else (1 if score0 < score1 else -1)
            self.msg = "Winner is {}".format(self.names[self.winner]) if self.winner in (0, 1) else "Draw game"
            print("### Score: %s: %d, %s: %d" % (self.names[0], score0, self.names[1], score1))
            if self.winner in (0, 1):
                print("### Winner: {}".format(self.names[self.winner]))
            else:
                print("### Draw game")

def main(args):
    if len(args) < 5:
        print("Usage: %s name1 command1 name2 command2" % os.path.basename(args[0]), file=sys.stderr)
        print("  name1    : first player's name", file=sys.stderr)
        print("  command1 : first player's command", file=sys.stderr)
        print("  name2    : second player's name", file=sys.stderr)
        print("  command2 : second player's command", file=sys.stderr)
        sys.exit(1)

    players = [(name, solver) for name, solver in zip(args[1:4:2], args[2:5:2])]
    qr = QuantumReversi(players)
    if qr.initialize():
        qr.play()
    qr.show_moves()
    qr.show_result()

if __name__ == "__main__":
    main(sys.argv)
