#!/usr/bin/env python3
from enum import Enum
from selfdrive.controls.lib.latcontrol_torque import set_torque_tune


class LongTunes(Enum):
  PEDAL = 0
  TSS2 = 1
  TSS = 2

class LatTunes(Enum):
  TORQUE = 0


###### LONG ######
def set_long_tune(tune, name):
  # Improved longitudinal tune
  if name == LongTunes.TSS2 or name == LongTunes.PEDAL:
    tune.deadzoneBP = [0., 8.05]
    tune.deadzoneV = [.0, .14]
    tune.kpBP = [0., 5., 20.]
    tune.kpV = [1.3, 1.0, 0.7]
    tune.kiBP = [0., 5., 12., 20., 27.]
    tune.kiV = [.35, .23, .20, .17, .1]
  # Default longitudinal tune
  elif name == LongTunes.TSS:
    tune.kpBP = [0., 5., 35.]
    tune.kiBP = [0., 35.]
    tune.kpV = [1.8, 1.5, 1.0]
    tune.kiV = [0.54, 0.36]
  else:
    raise NotImplementedError('This longitudinal tune does not exist')


###### LAT ######
def set_lat_tune(tune, name, MAX_LAT_ACCEL=2.5, FRICTION=0.01, steering_angle_deadzone_deg=0.0, use_steering_angle=True):
  if name == LatTunes.TORQUE:
    set_torque_tune(tune, MAX_LAT_ACCEL, FRICTION, steering_angle_deadzone_deg)
  else:
    raise NotImplementedError('This lateral tune does not exist')
