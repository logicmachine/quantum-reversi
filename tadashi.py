#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""tadashi's Quantum Reversi Solver (C) 2017 Fixstars Corp.
"""

import sys
import os
import random
import json
#import numpy as np

VERSION  = "0.08"
REVISION = "a"
VER_DATE = "20171201"

def solver(cells):
    return random.sample(cells, 2) if len(cells) != 1 else [cells[0], cells[0]]

def main(args):
    index = None
    names = None
    name = None
    w, h = 0, 0
    white_disc = "o"
    black_disc = "x"
    quantum_disc = "="
    empty_disc = " "
    while True:
        data = json.loads(sys.stdin.readline().strip())
        action = data["action"]
        if action == "play":
            board = data["board"]
            cells = []
            for i, s in enumerate(board):
                if s not in (white_disc, black_disc):
                    cells.append(i)
            positions = solver(cells)
            sys.stdout.write("%s\n" % json.dumps({"positions": positions}))
            sys.stdout.flush()
        elif action == "select":
            qvalues = data["entanglement"]
            select = random.choice(qvalues)
            sys.stdout.write("%s\n" % json.dumps({"select": select}))
            sys.stdout.flush()
        elif action == "init":
            index = data["index"]
            names = data["names"]
            name = names[index]
            w, h = data["size"]
            white_disc = data["white"]
            black_disc = data["black"]
            quantum_disc = data["quantum"]
            empty_disc = data["empty"]
            sys.stdout.write("\n")
            sys.stdout.flush()
        elif action == "quit":
            sys.stdout.flush()
            break

if __name__ == "__main__":
    main(sys.argv)
