HFO
===============

[Half Field Offense in Robocup 2D Soccer](http://www.cs.utexas.edu/~AustinVilla/sim/halffieldoffense/) is a subtask in RoboCup simulated soccer, modeling a situation in which the offense of one team has to get past the defense of the opposition in order to shoot goals. 

## Dependencies
 - [rcssserver-15.2.2](http://sourceforge.net/projects/sserver/files/rcssserver/15.2.2/)
 - [librcsc-4.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51941/librcsc-4.1.0.tar.gz/)
 - [soccerwindow2-5.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51942/soccerwindow2-5.1.0.tar.gz/) (Optional)

## Install
1. Edit the `LIBRCSC_INCLUDE`/`LIBRCSC_LINK` variables in `CMakeLists.txt` to point to your librcsc include/lib directories. 
2. `cmake .`
3. `make`

## (Optional) Patch rcssserver
By default if your agent takes longer then two seconds to select an action it will be disconnected from the server. To enable longer wait times, apply the following patch and rebuild your rcssserver:

`patch your_path/rcssserver-15.2.2/src/stadium.cpp < stadium.patch`

## Run
```bash
./bin/start.py
```
