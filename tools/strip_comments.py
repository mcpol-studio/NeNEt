#!/usr/bin/env python3
"""Strip // and /* */ comments from C/C++/GLSL source under src/ and assets/shaders/.
Also drops standalone '#'-comment lines from CMakeLists.txt files (not preprocessor directives).
String/char literals are preserved (state machine).
Run from repo root: python tools/strip_comments.py
"""
import pathlib

C_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
GLSL_EXTS = {".vert", ".frag", ".comp", ".tesc", ".tese", ".geom"}
ROOTS = ["src", "assets/shaders"]


def strip_c_comments(src: str) -> str:
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = src.find("\n", i)
            if j == -1:
                break
            i = j
            continue
        if c == "/" and nxt == "*":
            j = src.find("*/", i + 2)
            if j == -1:
                break
            i = j + 2
            continue
        if c == '"':
            out.append(c)
            i += 1
            while i < n:
                cc = src[i]
                out.append(cc)
                if cc == "\\" and i + 1 < n:
                    out.append(src[i + 1])
                    i += 2
                    continue
                if cc == '"':
                    i += 1
                    break
                i += 1
            continue
        if c == "'":
            out.append(c)
            i += 1
            while i < n:
                cc = src[i]
                out.append(cc)
                if cc == "\\" and i + 1 < n:
                    out.append(src[i + 1])
                    i += 2
                    continue
                if cc == "'":
                    i += 1
                    break
                i += 1
            continue
        out.append(c)
        i += 1
    res = "".join(out)
    return collapse_blanks(res)


def strip_cmake_comments(src: str) -> str:
    lines = []
    for ln in src.splitlines():
        s = ln.lstrip()
        if s.startswith("#"):
            continue
        lines.append(ln.rstrip())
    return collapse_blanks("\n".join(lines))


def collapse_blanks(s: str) -> str:
    out = []
    blank = 0
    for ln in s.split("\n"):
        ln = ln.rstrip()
        if ln == "":
            blank += 1
            if blank > 1:
                continue
        else:
            blank = 0
        out.append(ln)
    return "\n".join(out).rstrip() + "\n"


def main():
    repo = pathlib.Path(__file__).resolve().parents[1]
    n_changed = 0
    for root in ROOTS:
        base = repo / root
        if not base.exists():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            ext = p.suffix
            if ext in C_EXTS or ext in GLSL_EXTS:
                src = p.read_text(encoding="utf-8", errors="replace")
                new = strip_c_comments(src)
                if new != src:
                    p.write_text(new, encoding="utf-8", newline="\n")
                    print("stripped", p.relative_to(repo))
                    n_changed += 1
    cmake = repo / "CMakeLists.txt"
    if cmake.exists():
        src = cmake.read_text(encoding="utf-8", errors="replace")
        new = strip_cmake_comments(src)
        if new != src:
            cmake.write_text(new, encoding="utf-8", newline="\n")
            print("stripped CMakeLists.txt")
            n_changed += 1
    print(f"done. {n_changed} files changed.")


if __name__ == "__main__":
    main()
