from hfo_py.hfo import *
import os

def get_hfo_path():
  package_directory = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(package_directory, 'bin/HFO')

def get_viewer_path():
  package_directory = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(package_directory, 'bin/soccerwindow2')

def get_config_path():
  package_directory = os.path.dirname(os.path.abspath(__file__))
  return os.path.join(package_directory, 'bin/teams/base/config/formations-dt')
