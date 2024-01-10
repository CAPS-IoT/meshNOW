import os
import subprocess
import sys
from os import path

import pkg_resources

DOCS_PATH = path.dirname(path.realpath(__file__))
PROJECT_PATH = path.realpath(path.join(DOCS_PATH, ".."))
BUILD_DIR = path.join(DOCS_PATH, "_build")
SOURCE_PATH = path.join(DOCS_PATH, "source")
DOXYFILE = path.join(DOCS_PATH, "Doxyfile")

BUILD_TYPES = ["html", "latex"]


def check_python_requirements(requirements_path):
    try:
        with open(requirements_path, "r") as f:
            not_satisfied = []

            for line in f:
                line = line.strip()
                try:
                    pkg_resources.require(line)
                except Exception:
                    not_satisfied.append(line)

            if len(not_satisfied) > 0:
                print(
                    "The following Python requirements from the current directory's requirements.txt are not satisfied:"
                )
                for requirement in not_satisfied:
                    print(requirement)
                sys.exit(1)

    except FileNotFoundError:
        pass


def run_doxygen():
    print("Calling Doxygen to generate XML")

    environ = os.environ.copy()
    # expose the project path to the doxyfile
    environ["PROJECT_PATH"] = PROJECT_PATH

    if not path.isfile(DOXYFILE):
        print("Doxyfile not found at path {}".format(DOXYFILE))
        sys.exit(1)

    args = ["doxygen", DOXYFILE]

    print("Running '{}'".format(" ".join(args)))

    try:
        p = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=environ,
            cwd=BUILD_DIR,
        )
        for c in iter(lambda: p.stdout.read(1), b""):
            sys.stdout.write(c.decode(sys.stdout.encoding))
        p.wait()
        sys.stdout.flush()
    except KeyboardInterrupt:
        p.kill()
        p.wait()
        raise


def sphinx_build(build_type):
    print("Building {} documentation in build_dir {}".format(build_type, BUILD_DIR))

    environ = os.environ.copy()
    environ["BUILDDIR"] = BUILD_DIR

    args = [
        sys.executable,
        "-u",
        "-m",
        "sphinx.cmd.build",
        "-b",
        build_type,
        "-d",
        os.path.join(BUILD_DIR, "doctrees"),
        "-c",
        DOCS_PATH,
        # "-D",
        # "config_dir={}".format(os.path.abspath(os.path.dirname(__file__))),
        # "-D",
        # "doxyfile_dir={}".format(os.path.abspath(build_info["doxyfile_dir"])),
        # "-D",
        # "project_path={}".format(os.path.abspath(build_info["project_path"])),
        SOURCE_PATH,
        os.path.join(BUILD_DIR, build_type),
    ]

    print("Running '{}'".format(" ".join(args)))

    ret = 0
    try:
        p = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=environ,
            cwd=BUILD_DIR,
        )
        for c in iter(lambda: p.stdout.read(1), b""):
            sys.stdout.write(c.decode(sys.stdout.encoding))
        ret = p.wait()
        sys.stdout.flush()
    except KeyboardInterrupt:
        p.kill()
        p.wait()
        raise

    return ret


def build_docs():
    for build_type in BUILD_TYPES:
        ret = sphinx_build(build_type)

        if ret != 0:
            print("Error building {} docs".format(build_type))
            sys.exit(ret)
    pass


def main():
    # First, check if the user has installed all requirements.txt
    check_python_requirements(path.join(DOCS_PATH, "requirements.txt"))

    # create build dir if it doesn't exist
    if not path.exists(BUILD_DIR):
        os.makedirs(BUILD_DIR)

    run_doxygen()

    build_docs()


if __name__ == "__main__":
    main()
