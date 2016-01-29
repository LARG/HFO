#!/bin/bash

BINARY_DIR=`dirname $0`/teams/base #"$( cd "$( dirname "$0" )" && pwd )"
CONFIG_DIR=$BINARY_DIR/config

player="${BINARY_DIR}/agent"
coach="${BINARY_DIR}/sample_coach"
teamname="HELIOS_base"
host="localhost"
port=6000
coach_port=""
debug_server_host=""
debug_server_port=""

player_conf="${CONFIG_DIR}/player.conf"
config_dir="${CONFIG_DIR}/formations-dt"

coach_conf="${CONFIG_DIR}/coach.conf"
team_graphic="--use_team_graphic off"

number=11
usecoach="true"

unum=0

sleepprog=sleep
goaliesleep=1
sleeptime=1

debugopt=""
coachdebug=""
opts=""

offline_logging=""
offline_mode=""
fullstateopt=""

record_stats_file=""

use_gdb=""
run_debug_version=""

usage()
{
  (echo "Usage: $0 [options]"
   echo "Available options:"
   echo "      --help                   prints this"
   echo "  -h, --host HOST              specifies server host (default: localhost)"
   echo "  -p, --port PORT              specifies server port (default: 6000)"
   echo "  -P  --coach-port PORT        specifies server port for online coach (default: 6002)"
   echo "  -t, --teamname TEAMNAME      specifies team name"
   echo "  -n, --number NUMBER          specifies the number of players"
   echo "  -u, --unum UNUM              specifies the uniform number of players"
   echo "  -C, --without-coach          specifies not to run the coach"
   echo "  -f, --formation DIR          specifies the formation directory"
   echo "  --team-graphic FILE          specifies the team graphic xpm file"
   echo "  --offline-logging            writes offline client log (default: off)"
   echo "  --offline-client-mode        starts as an offline client (default: off)"
   echo "  --debug                      writes debug log (default: off)"
   echo "  --debug-server-connect       connects to the debug server (default: off)"
   echo "  --debug-server-host HOST     specifies debug server host (default: localhost)"
   echo "  --debug-server-port PORT     specifies debug server port (default: 6032)"
   echo "  --debug-server-logging       writes debug server log (default: off)"
   echo "  --log-dir DIRECTORY          specifies debug log directory (default: /tmp)"
   echo "  --debug-log-ext EXTENSION    specifies debug log file extension (default: .log)"
   echo "  --fullstate FULLSTATE_TYPE   specifies fullstate model handling"
   echo "  --record                     records actions (default: off)"
   echo "                               FULLSTATE_TYPE is one of [ignore|reference|override]."
   echo "  --offensePlayers player1 ... specifies the numbers of the offense players"
   echo "  --defensePlayers player1 ... specifies the numbers of the defense players"
   echo "  --record_stats_file FILE     specifies file to write stats to"
   echo "  --learn-actions NUM          number of instances to learn actions, don't do the normal play"
   echo "  --learn-index NUM            learning index"
   echo "  --learn-path STR             learn path"
   echo "  --model-path STR             model path"
   echo "  --option-filename FILE       name of file with json options"
   echo "  --teammate STR               name of teammates"
   echo "  --numTeammates NUM           number of teammates"
   echo "  --numOpponents NUM           number of opponents"
   echo "  --playingOffense [0|1]       are we playing offense or defense"
   echo "  --serverPort   NUM           port for server to list on"
   echo "  --seed NUM                   seed for rng"
   echo "  --gdb                        runs with gdb on (default:off)"
   ) 1>&2
}

while [ $# -gt 0 ]
do
  case $1 in

    --help)
      usage
      exit 0
      ;;

    -h|--host)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      host="${2}"
      shift 1
      ;;

    -p|--port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      port="${2}"
      shift 1
      ;;

    -P|--coach-port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      coach_port="${2}"
      shift 1
      ;;

    -t|--teamname)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      teamname="${2}"
      shift 1
      ;;

    -n|--number)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      number="${2}"
      shift 1
      ;;

    -u|--unum)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      unum="${2}"
      shift 1
      ;;

    -C|--without-coach)
      usecoach="false"
      ;;

    -f|--formation)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      config_dir="${2}"
      shift 1
      ;;

    --team-graphic)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      team_graphic="--use_team_graphic on --team_graphic_file ${2}"
      shift 1
      ;;

    --offline-logging)
      offline_logging="--offline_logging"
      ;;

    --offline-client-mode)
      offline_mode="on"
      ;;

    --record)
      record="--record"
      ;;

    --record_stats_file)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      record_stats_file="--record_stats_file ${2}"
      shift 1
      ;;

    --learn-actions)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --learn-actions ${2}"
      shift 1
      ;;

    --learn-index)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --learn-index ${2}"
      shift 1
      ;;

    --learn-path)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --learn-path ${2}"
      shift 1
      ;;

    --model-path)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --model-path ${2}"
      shift 1
      ;;

    --option-filename)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --option-filename ${2}"
      shift 1
      ;;

    --teammate)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --teammate ${2}"
      shift 1
      ;;

    --numTeammates)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --numTeammates ${2}"
      shift 1
      ;;

    --numOpponents)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --numOpponents ${2}"
      shift 1
      ;;

    --playingOffense)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --playingOffense ${2}"
      shift 1
      ;;

    --serverPort)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --serverPort ${2}"
      shift 1
      ;;

    --seed)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --seed ${2}"
      shift 1
      ;;

    --trainer)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --trainer ${2}"
      shift 1
      ;;

    --save-path)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      opts="${opts} --save-path ${2}"
      shift 1
      ;;

    --gdb)
      use_gdb="true"
      ;;

    --run-debug-version)
      run_debug_version="true"
      ;;

    --debug)
      debugopt="${debugopt} --debug"
      coachdebug="${coachdebug} --debug"
      ;;

    --debug-server-connect)
      debugopt="${debugopt} --debug_server_connect"
      ;;

    --debug-server-host)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debug_server_host="${2}"
      shift 1
      ;;

    --debug-server-port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debug_server_port="${2}"
      shift 1
      ;;

    --debug-server-logging)
      debugopt="${debugopt} --debug_server_logging"
      ;;

    --log-dir)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debugopt="${debugopt} --log_dir ${2}"
      LOG_DIR=${2}
      shift 1
      ;;

    --debug-log-ext)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debugopt="${debugopt} --debug_log_ext ${2}"
      shift 1
      ;;

    --fullstate)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      fullstate_type="${2}"
      shift 1

      case "${fullstate_type}" in
	ignore)
		fullstateopt="--use_fullstate false --debug_fullstate false"
		;;

	reference)
		fullstateopt="--use_fullstate false --debug_fullstate true"
		;;

	override)
		fullstateopt="--use_fullstate true --debug_fullstate true"
		;;

	*)
		usage
		exit 1
		;;
      esac
      ;;

    *)
      echo 1>&2
      echo "invalid option \"${1}\"." 1>&2
      echo 1>&2
      usage
      exit 1
      ;;
  esac

  shift 1
done

if  [ X"${offline_logging}" != X'' ]; then
  if  [ X"${offline_mode}" != X'' ]; then
    echo "'--offline-logging' and '--offline-mode' cannot be used simultaneously."
    exit 1
  fi
fi

if [ X"${coach_port}" = X'' ]; then
  coach_port=`expr ${port} + 2`
fi

if [ X"${debug_server_host}" = X'' ]; then
  debug_server_host="${host}"
fi

if [ X"${debug_server_port}" = X'' ]; then
  debug_server_port=`expr ${port} + 32`
fi

opt="--player-config ${player_conf} --config_dir ${config_dir}"
opt="${opt} -h ${host} -p ${port} -t ${teamname}"
opt="${opt} ${fullstateopt}"
opt="${opt} --debug_server_host ${debug_server_host}"
opt="${opt} --debug_server_port ${debug_server_port}"
opt="${opt} ${offline_logging}"
opt="${opt} ${debugopt}"
opt="${opt} ${record}"

ping -c 1 $host

i=1
while [ $i -le ${number} ] ; do
  offline_number=""
  if  [ X"${offline_mode}" != X'' ]; then
    offline_number="--offline_client_number ${i}"
  fi
  tempsleeptime=${sleeptime}
  goalie=""
  if [ $i -eq 1 ]; then
    goalie="-g ${record_stats_file}"
    tempsleeptime=${goaliesleep}
  fi

  if [ $unum -eq 0 ] || [ $unum -eq $i ]; then
    cmd="${player} ${opt} ${opts} ${offline_number} ${goalie} --reconnect $i"
    if [ X"${use_gdb}" = X'' ]; then
      ${cmd} &
      echo "PID: $!" > ${LOG_DIR}/start$$
    else
      gdb -ex run --args  ${cmd}
    fi
    $sleepprog ${tempsleeptime}
  fi

  i=`expr $i + 1`
done


if [ "${usecoach}" = "true" ]; then
  coachopt="--coach-config ${coach_conf}"
  coachopt="${coachopt} -h ${host} -p ${coach_port} -t ${teamname}"
  coachopt="${coachopt} ${team_graphic}"
  coachopt="${coachopt} --debug_server_host ${debug_server_host}"
  coachopt="${coachopt} --debug_server_port ${debug_server_port}"
  coachopt="${coachopt} ${offline_logging}"
  coachopt="${coachopt} ${debugopt}"


  offline_number=""
  if  [ X"${offline_mode}" != X'' ]; then
    offline_mode="--offline_client_mode"
  fi
  if [ $unum -eq 0 ] || [ $unum -eq 12 ]; then
    $coach ${coachopt} ${offline_mode} &
  fi
fi
