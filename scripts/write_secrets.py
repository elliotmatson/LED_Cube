#!/usr/bin/env python3
"""Writes lib/cube/secrets.h for a CI build from the environment.

The release workflow passes MQTT_URL, MQTT_USER and MQTT_PASSWORD from
repository secrets (pio-actions' build-env). Without MQTT_URL nothing is
written, and the build simply does not report (see lib/cube/telemetry.h).
Values are escaped as C string literals, so any password works.
"""

import json
import os
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "lib" / "cube" / "secrets.h"


def c_string(value: str) -> str:
    # JSON string escaping is valid C for these: \" \\ \n and \uXXXX.
    return json.dumps(value)


def main() -> None:
    url = os.environ.get("MQTT_URL", "").strip()
    if not url:
        print("write_secrets: no MQTT_URL; this build will not report")
        return
    lines = ["// Written by scripts/write_secrets.py in CI. Do not commit.", "#pragma once"]
    for key in ("MQTT_URL", "MQTT_USER", "MQTT_PASSWORD"):
        value = os.environ.get(key, "")
        if value:
            lines.append(f"#define {key} {c_string(value)}")
    HEADER.write_text("\n".join(lines) + "\n")
    print(f"write_secrets: wrote {HEADER.name} ({', '.join(k for k in ('MQTT_URL', 'MQTT_USER', 'MQTT_PASSWORD') if os.environ.get(k))})")


if __name__ == "__main__":
    main()
