#!/usr/bin/env python
from panda import Panda


p = Panda()

def main():
  p.set_safety_mode(Panda.SAFETY_SILENT)
  p.send_heartbeat()

main()
