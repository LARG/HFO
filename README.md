HFO
===============

[Half Field Offense in Robocup 2D Soccer](http://www.cs.utexas.edu/~AustinVilla/sim/halffieldoffense/) is a subtask in RoboCup simulated soccer, modeling a situation in which the offense of one team has to get past the defense of the opposition in order to shoot goals. 

## Dependencies
 - [rcssserver-15.2.2](http://sourceforge.net/projects/sserver/files/rcssserver/15.2.2/)
 - [librcsc-4.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51941/librcsc-4.1.0.tar.gz/)
 - [soccerwindow2-5.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51942/soccerwindow2-5.1.0.tar.gz/) Optional

## Install
1. Edit the `LIBRCSC_INCLUDE` and `LIBRCSC_LINK` variables in CMakeLists.txt to point to your librcsc include and library directories. 
2. `cmake .`
3. `make`

## Run
Execute `bin/start.py`. 
