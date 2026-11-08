#!/usr/bin/env python3
"""Configure and build the multi-protocol gateway with CMake."""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent
BUILD_DIR = PROJECT_ROOT / ".build"
BUILD_TYPES = {"debug": "Debug", "release": "Release"}


def run(command):
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, check=True)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Configure and build the project with CMake."
    )
    parser.add_argument(
        "-t",
        "--type",
        dest="build_type",
        type=str.lower,
        choices=sorted(BUILD_TYPES),
        default="debug",
        metavar="TYPE",
        help="build type: debug or release (default: debug)",
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=None,
        metavar="N",
        help="number of parallel build jobs",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="remove the build directory before configuring",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    if args.jobs is not None and args.jobs < 1:
        print("error: --jobs must be at least 1", file=sys.stderr)
        return 2

    if args.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)

    try:
        run(
            [
                "cmake",
                "-S",
                str(PROJECT_ROOT),
                "-B",
                str(BUILD_DIR),
                "-DCMAKE_BUILD_TYPE=" + BUILD_TYPES[args.build_type],
                "-DBUILD_TESTING=ON",
            ]
        )

        build_command = ["cmake", "--build", str(BUILD_DIR)]
        if args.jobs is not None:
            build_command += ["--parallel", str(args.jobs)]
        run(build_command)
    except FileNotFoundError:
        print("error: cmake was not found on PATH", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode or 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
