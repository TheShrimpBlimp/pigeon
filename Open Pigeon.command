#!/bin/zsh
cd -- "${0:A:h}" || exit 1
if [[ -x .venv/bin/python ]]; then
  exec .venv/bin/python tools/pigeon_gui.py
fi
exec python3 tools/pigeon_gui.py
