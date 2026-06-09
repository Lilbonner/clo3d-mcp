"""CLO bring-up diagnostic. No side effects — safe to run anytime.

Run it INSIDE CLO: Main Menu -> Edit -> Python Script -> Run Python Script,
then pick this file (or drag this file onto the CLO window). Running it as a
FILE avoids the interactive console's line-by-line paste problems.

It prints the embedded Python version and which CLO API modules and Qt bindings
are importable — exactly the facts the listener needs.
"""

import importlib.util
import sys

print("=== clo-mcp diagnose ===")
print("PYTHON:", sys.version)

api_modules = ["import_api", "export_api", "fabric_api", "pattern_api", "utility_api", "ApiTypes"]
qt_bindings = ["PySide6", "PySide2", "PyQt5"]

print("--- CLO API modules ---")
for name in api_modules:
    ok = importlib.util.find_spec(name) is not None
    print(("OK   " if ok else "MISS ") + name)

print("--- Qt bindings ---")
for name in qt_bindings:
    ok = importlib.util.find_spec(name) is not None
    print(("OK   " if ok else "MISS ") + name)

print("=== done ===")
