"""Very basic tests, only of functions available sans server"""
#from __future__ import print_function

import hfo

def test_basic():
    hfo_env = hfo.HFOEnvironment()

    for action in range(hfo.NUM_HFO_ACTIONS):
        assert len(hfo_env.actionToString(action))

    for state in range(hfo.NUM_GAME_STATUS_STATES):
        assert len(hfo_env.statusToString(state))

if __name__ == '__main__':
    test_basic()
