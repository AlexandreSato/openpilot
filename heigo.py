#!/usr/bin/env python3
import threading, subprocess
from cereal import messaging
from openpilot.common.realtime import Ratekeeper, DT_MDL
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.common.params import Params
_params = Params()


X_THRESH, T_THRESH = 30.0, 0.3   # sunnypilot: GREEN_LIGHT_X_THRESHOLD / TRIGGER_TIMER_THRESHOLD


class Alert:  # a mesma FSM do sunnypilot, parametrizada
  def __init__(self, armed, fired):
    self.armed, self.fired = armed, fired
    self.timer, self.done = 0.0, False

  def update(self, sm):
    if not self.armed(sm):                    # desarma → reseta (igual ao reset do sunnypilot)
      self.timer, self.done = 0.0, False
      return
    self.timer = self.timer + DT_MDL if self.fired(sm) else 0.0
    if self.timer > T_THRESH and not self.done:
      self.done = True
      _params.put("as_heigo", "1")


green = Alert(
  armed=lambda sm: not sm['selfdriveState'].enabled and (sm['driverMonitoringState'].visionPolicyState.isDistracted or
                   sm['driverMonitoringState'].visionPolicyState.distractedTypes.phone) and
                   sm['carState'].vEgo < 0.1 and not sm['radarState'].leadOne.status,
  fired=lambda sm: len(sm['modelV2'].position.x) == ModelConstants.IDX_N and
                   sm['modelV2'].position.x[-1] > X_THRESH
)

lead = Alert(
  armed=lambda sm: not sm['selfdriveState'].enabled and (sm['driverMonitoringState'].visionPolicyState.isDistracted or
                   sm['driverMonitoringState'].visionPolicyState.distractedTypes.phone) and sm['carState'].vEgo < 0.1 and
                   sm['radarState'].leadOne.status,
  fired=lambda sm: sm['modelV2'].position.x[-1] > X_THRESH,   # modelo quer andar = lead saiu
)


def main():
  sm = messaging.SubMaster(['selfdriveState', 'modelV2', 'radarState', 'carState', 'driverMonitoringState'], poll='modelV2')
  rk = Ratekeeper(20)
  while True:
    sm.update(0)
    for a in (green, lead):
      a.update(sm)
    rk.keep_time()

if __name__ == "__main__":
  main()
