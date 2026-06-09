"""Find whether CLO's REST/async-callback API is reachable from Python.

Run INSIDE CLO as a FILE (Main Menu -> Edit -> Python Script -> Run Python
Script, pick this file). Running it as a file avoids the interactive console's
line-by-line indentation errors.

No side effects. It only inspects modules and prints what it finds. The goal is
one fact: is `CallbackRestRequest` (the async, non-blocking REST callback) — or
any `CallREST*` function — importable from Python, and from which module? That
decides whether the "long-poll via CLO callback" real-time path is possible.
"""

import sys

NEEDLES = ("REST", "CALLBACK")          # case-insensitive substrings to match
KNOWN = ["utility_api", "import_api", "export_api", "fabric_api",
         "pattern_api", "ApiTypes"]
CANDIDATES = ["api", "clo_api", "rest_api", "RestApi", "rest", "marvelous",
              "Marvelous", "CloApi", "cloapi", "qt_api", "widget_api"]


def _matches(obj):
    return sorted(
        name for name in dir(obj)
        if any(n in name.upper() for n in NEEDLES)
    )


print("=== clo-mcp diagnose REST ===")

# 1) Known API modules.
print("--- known *_api modules ---")
for name in KNOWN:
    try:
        mod = __import__(name)
    except Exception as exc:  # noqa: BLE001
        print("ERR  %-14s %s" % (name, exc))
        continue
    hits = _matches(mod)
    print(("HIT  " if hits else "none ") + "%-14s %s" % (name, hits))

# 2) Candidate module names we haven't confirmed exist.
print("--- candidate module names ---")
for name in CANDIDATES:
    try:
        mod = __import__(name)
    except Exception:  # noqa: BLE001 - most won't exist; that's fine
        print("MISS " + name)
        continue
    hits = _matches(mod)
    print(("HIT  " if hits else "OK   ") + "%-14s %s" % (name, hits))

# 3) Brute scan of every already-loaded module for the target functions.
print("--- scan of all loaded modules (sys.modules) ---")
found_any = False
for modname, mod in list(sys.modules.items()):
    if mod is None:
        continue
    try:
        hits = _matches(mod)
    except Exception:  # noqa: BLE001 - some modules raise on dir()
        continue
    if hits:
        found_any = True
        print("HIT  %-22s %s" % (modname, hits))
if not found_any:
    print("(no REST/Callback names found in any loaded module)")

print("=== done ===")
