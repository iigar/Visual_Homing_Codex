"""Small Python 3.12 compatibility surface for ArduPilot 4.3.6's pinned Waf.

That Waf revision uses the removed ``imp.new_module`` and ``open(..., 'rU')``.
Keeping both compatibility adaptations outside the pinned ArduPilot worktree
preserves the exact firmware source on Ubuntu 24.04 / Python 3.12.
"""

import builtins
from types import ModuleType


_open = builtins.open


def _open_without_universal_newline(file, mode="r", *args, **kwargs):
    return _open(file, mode.replace("U", ""), *args, **kwargs)


builtins.open = _open_without_universal_newline


def new_module(name: str) -> ModuleType:
    return ModuleType(name)
