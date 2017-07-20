"""A few tests using a server (and --fullstate so that know will 'see' uniform numbers)"""
from __future__ import print_function

import os
import subprocess
import sys
import time

import hfo

hfo_env = hfo.HFOEnvironment()

def try_step(): # if a game ends within ~5 frames, something is wrong...
    status = hfo_env.step()
    
    assert (status ==
            hfo.IN_GAME), ("Status is {!s} ({!r}), not IN_GAME".
                           format(hfo_env.statusToString(status),status))
    
    return hfo_env.getState()

def test_with_server():
    test_dir   = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
    binary_dir = os.path.normpath(test_dir + "/../bin")
    conf_dir = os.path.join(binary_dir, 'teams/base/config/formations-dt')
    bin_HFO = os.path.join(binary_dir, "HFO")
    
    popen_list = [sys.executable, "-x", bin_HFO,
                  "--offense-agents=1", "--defense-npcs=2",
                  "--offense-npcs=2", "--trials=1", "--headless",
                  "--fullstate"]
    
    HFO_process = subprocess.Popen(popen_list)
    
    time.sleep(0.2)
    
    assert (HFO_process.poll() is
            None), "Failed to start HFO with command '{}'".format(" ".join(popen_list))
    
    time.sleep(3)
    
    try:
        hfo_env.connectToServer(config_dir=conf_dir) # using defaults otherwise

        min_state_size = 58+(9*4)
        state_size = hfo_env.getStateSize()
        assert (state_size >=
                min_state_size), "State size is {!s}, not {!s}+".format(state_size,min_state_size)

        print("State size is {!s}".format(state_size))

        my_unum = hfo_env.getUnum()
        assert ((my_unum > 0) and (my_unum <= 11)), "Wrong self uniform number ({!r})".format(my_unum)

        print("My unum is {!s}".format(my_unum))

        num_teammates = hfo_env.getNumTeammates()
        assert (num_teammates == 2), "Wrong num teammates ({!r})".format(num_teammates)

        num_opponents = hfo_env.getNumOpponents()
        assert (num_opponents == 2), "Wrong num opponents ({!r})".format(num_opponents)

        had_ok_unum = False
        had_ok_unum_set_my_side = set()
        had_ok_unum_set_their_side = set();

        hfo_env.act(hfo.NOOP)

        state = try_step()

        for x in range(0,5):
            if int(state[12]) == 1: # can kick the ball
                hfo_env.act(hfo.DRIBBLE)
            else:
                hfo_env.act(hfo.MOVE)
            
            state = try_step()
                
            for n in range((state_size-4), state_size):
                their_unum = state[n]
                if ((their_unum > 0) and (their_unum <= 0.11)):
                    print("{!s}: OK uniform number ({!r}) for {!s}".format(x,their_unum,n))
                    had_ok_unum = True
                    if n > (state_size-3):
                        had_ok_unum_set_their_side.add(their_unum)
                    else:
                        had_ok_unum_set_my_side.add(their_unum)
                elif x > 3:
                    print("{!s}: Wrong other uniform number ({!r}) for {!s}".format(x,their_unum,n))

            if (len(had_ok_unum_set_my_side) > 1) and (len(had_ok_unum_set_their_side) > 1):
                break
                    
        assert had_ok_unum, "Never saw OK other uniform number"

        try:
            hfo_env.act(hfo.MOVE_TO)
        except AssertionError:
            pass
        else:
            raise AssertionError("Should have got AssertionError")

        HFO_process.terminate()
        hfo_env.act(hfo.QUIT)
        time.sleep(1.2)

        status = hfo_env.step()

        assert (status ==
                hfo.SERVER_DOWN), ("Status is {!s} ({!r}), not SERVER_DOWN".
                                   format(hfo_env.statusToString(status), status))

    finally:
        if HFO_process.poll() is None:
            HFO_process.terminate()
        os.system("killall -9 rcssserver")

if __name__ == '__main__':
    test_with_server()
