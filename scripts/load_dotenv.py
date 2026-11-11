"""Load the project's .env file into the PlatformIO build and OTA upload.

PlatformIO does not read .env files natively. Wired into [env] as
`extra_scripts = pre:scripts/load_dotenv.py`, this script:

  1. Parses ./.env (KEY=VALUE lines, # comments, optional "..." / '...'
     quoting around values). Variables already exported in the shell take
     precedence over the file.
  2. Injects the device settings as CPPDEFINES (CFG_<NAME>), which
     include/env_config.h turns into typed constants. Because this happens on
     the SCons environment, IDE-driven builds work without sourcing anything.
  3. For OTA environments (upload_protocol = espota), sets the upload target
     and password from the same values that were compiled into the firmware,
     so the two can never disagree:
       - target:   OTA_UPLOAD_HOST if set, else <DEVICE_HOSTNAME>.local
                   (an explicit upload_port / --upload-port wins)
       - password: OTA_PASSWORD, passed to espota as --auth

If a required variable is missing, the build fails with a message pointing
at .env-example.
"""

Import("env")  # noqa: F821 (provided by SCons at pre-script time)

import os
from pathlib import Path

# Settings compiled into the firmware as string literals.
STRING_SETTINGS = [
    "DEVICE_HOSTNAME",
    "WIFI_SSID",
    "WIFI_PASSWORD",
    "SIGNALK_HOST",
    "OTA_PASSWORD",
    "SHIP_TZ_NAME",
    "SHIP_TZ_POSIX",
]

# Settings compiled into the firmware as numbers.
NUMERIC_SETTINGS = [
    "SIGNALK_PORT",
]


def parse_dotenv(path):
    """Parse a minimal .env file. Returns dict of key -> string value."""
    values = {}
    if not path.exists():
        return values
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
            value = value[1:-1]
        values[key] = value
    return values


project_dir = Path(env["PROJECT_DIR"])  # noqa: F821
dotenv_path = project_dir / ".env"

for key, value in parse_dotenv(dotenv_path).items():
    os.environ.setdefault(key, value)

missing = [k for k in STRING_SETTINGS + NUMERIC_SETTINGS if not os.environ.get(k)]
if missing:
    raise Exception(
        "load_dotenv.py: missing required settings: "
        + ", ".join(missing)
        + " (checked "
        + str(dotenv_path)
        + " and the shell environment). Copy .env-example to .env and fill it in."
    )

defines = [
    ("CFG_" + k, env.StringifyMacro(os.environ[k]))  # noqa: F821
    for k in STRING_SETTINGS
]
defines += [("CFG_" + k, os.environ[k]) for k in NUMERIC_SETTINGS]
env.Append(CPPDEFINES=defines)  # noqa: F821

if str(env.get("UPLOAD_PROTOCOL", "")).strip() == "espota":  # noqa: F821
    if not env.subst("$UPLOAD_PORT"):  # noqa: F821
        host = os.environ.get("OTA_UPLOAD_HOST") or (
            os.environ["DEVICE_HOSTNAME"] + ".local"
        )
        env.Replace(UPLOAD_PORT=host)  # noqa: F821
    flags = [str(f) for f in env.get("UPLOAD_FLAGS", [])]  # noqa: F821
    if not any(f.startswith("--auth") for f in flags):
        env.Append(UPLOAD_FLAGS=["--auth=" + os.environ["OTA_PASSWORD"]])  # noqa: F821
