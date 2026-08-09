#!/usr/bin/env python3.11
"""
generate_blackboard.py  —  reads blackboard_schema.toml, emits C accessors.
Usage: python3.11 generate_blackboard.py <schema.toml> <output_dir>
"""

import sys
import os
import tomllib   # stdlib from Python 3.11+

def c_safe(name: str) -> str:
    return name.upper().replace(".", "_")

def generate(schema_path: str, out_dir: str) -> None:
    with open(schema_path, "rb") as f:
        schema = tomllib.load(f)

    keys = schema.get("key", [])
    os.makedirs(out_dir, exist_ok=True)

    # ── Header ────────────────────────────────────────────────────────────────
    h_lines = [
        "/* AUTO-GENERATED — DO NOT EDIT — see tools/generate_blackboard.py */",
        "#pragma once",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
    ]

    for k in keys:
        safe = c_safe(k["name"])
        t    = k["type"]
        if t == "int64":
            h_lines.append(f"int64_t monday_bb_get_{k['name']}(void);")
            h_lines.append(f"void    monday_bb_set_{k['name']}(int64_t v);")
        elif t in ("string", "bytes"):
            h_lines.append(f"const char *monday_bb_get_{k['name']}(void);")
            h_lines.append(f"void        monday_bb_set_{k['name']}(const char *v, size_t len);")
        elif t == "float64":
            h_lines.append(f"double monday_bb_get_{k['name']}(void);")
            h_lines.append(f"void   monday_bb_set_{k['name']}(double v);")
        elif t == "bool":
            h_lines.append(f"int  monday_bb_get_{k['name']}(void);")
            h_lines.append(f"void monday_bb_set_{k['name']}(int v);")
        h_lines.append("")

    h_path = os.path.join(out_dir, "blackboard_accessors.h")
    with open(h_path, "w") as f:
        f.write("\n".join(h_lines))

    # ── Implementation ────────────────────────────────────────────────────────
    c_lines = [
        "/* AUTO-GENERATED — DO NOT EDIT — see tools/generate_blackboard.py */",
        "#include \"blackboard_accessors.h\"",
        "#include <string.h>",
        "#include <stdlib.h>",
        "",
    ]

    for k in keys:
        t    = k["type"]
        n    = k["name"]
        msz  = k["max_size_bytes"]

        if t == "int64":
            c_lines += [
                f"static int64_t g_bb_{n} = 0;",
                f"int64_t monday_bb_get_{n}(void) {{ return g_bb_{n}; }}",
                f"void    monday_bb_set_{n}(int64_t v) {{ g_bb_{n} = v; }}",
                "",
            ]
        elif t in ("string", "bytes"):
            c_lines += [
                f"static char g_bb_{n}[{msz}] = {{0}};",
                f"const char *monday_bb_get_{n}(void) {{ return g_bb_{n}; }}",
                f"void monday_bb_set_{n}(const char *v, size_t len) {{",
                f"    size_t copy = len < {msz} - 1 ? len : {msz} - 1;",
                f"    memcpy(g_bb_{n}, v, copy);",
                f"    g_bb_{n}[copy] = '\\0';",
                f"}}",
                "",
            ]
        elif t == "float64":
            c_lines += [
                f"static double g_bb_{n} = 0.0;",
                f"double monday_bb_get_{n}(void) {{ return g_bb_{n}; }}",
                f"void   monday_bb_set_{n}(double v) {{ g_bb_{n} = v; }}",
                "",
            ]
        elif t == "bool":
            c_lines += [
                f"static int g_bb_{n} = 0;",
                f"int  monday_bb_get_{n}(void) {{ return g_bb_{n}; }}",
                f"void monday_bb_set_{n}(int v) {{ g_bb_{n} = !!v; }}",
                "",
            ]

    c_path = os.path.join(out_dir, "blackboard_accessors.c")
    with open(c_path, "w") as f:
        f.write("\n".join(c_lines))

    print(f"[generate_blackboard] wrote {h_path} and {c_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: generate_blackboard.py <schema.toml> <output_dir>")
        sys.exit(1)
    generate(sys.argv[1], sys.argv[2])

