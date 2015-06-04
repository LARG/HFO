HFO
===============

![3 on 3 HFO](https://github.com/mhauskn/HFO/blob/master/img/hfo3on3.png)

[Half Field Offense in Robocup 2D Soccer](http://www.cs.utexas.edu/~AustinVilla/sim/halffieldoffense/) is a subtask in RoboCup simulated soccer, modeling a situation in which the offense of one team has to get past the defense of the opposition in order to shoot goals. 

## Dependencies
 - [rcssserver-15.2.2](http://sourceforge.net/projects/sserver/files/rcssserver/15.2.2/)
  - If `configure` fails to detect boost libraries, set the configure flag `--with-boost-libdir=your_boost_path`
  - If `make` encounters [lex errors](http://sourceforge.net/p/sserver/discussion/76439/thread/5b13cac6/), ensure you have [bison 2.7.1](http://www.gnu.org/software/bison/) installed.
  - Only run single-threaded make. Multi-threaded make (eg `make -j4`) fails.
 - [librcsc-4.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51941/librcsc-4.1.0.tar.gz/)
 - [soccerwindow2-5.1.0](http://en.sourceforge.jp/projects/rctools/downloads/51942/soccerwindow2-5.1.0.tar.gz/) (Optional)
  - If -laudio is not found during make, it can be safely removed from the link command.

## Install
1. Edit the `LIBRCSC_INCLUDE`/`LIBRCSC_LINK` variables in `CMakeLists.txt` to point to your librcsc include/lib directories. 
2. `cmake .`
3. `make`

## Patch rcssserver
By default if your agent takes longer then two seconds to select an action it will be disconnected from the server. To enable longer wait times, apply the following patch and rebuild your rcssserver:

`patch your_path/rcssserver-15.2.2/src/stadium.cpp < stadium.patch`

## Example Agents
Example C++ and Python agents are provided in the `example` directory.

## Run
To start the HFO server run: `./bin/start.py` then start an example agent: `./example/hfo_example_agent` or start them both at the same time: 

`(./bin/start.py &) && ./example/hfo_example_agent`

## Documentation
The state and action spaces provided by the HFO domain are documented in the [manual](doc/manual.pdf).