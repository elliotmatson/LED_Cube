Import("env")

import json
import os
import shutil
import subprocess


def save_defconfig_action(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    cmd = ["cmake", "--build", build_dir, "--target", "save-defconfig"]

    print("Running ESP-IDF save-defconfig target...")
    print("Command:", " ".join(cmd))

    result = subprocess.run(cmd, cwd=project_dir)
    if result.returncode != 0:
        raise Exception("ESP-IDF save-defconfig failed")


# PlatformIO's ESP-IDF integration links the firmware itself (via SCons) rather
# than through the ESP-IDF CMake project's own "app" target, so the map file
# the CMake "size"/"size-components"/"size-files" targets depend on
# (<project>.map) is never produced and those targets fail with a ninja "no
# known rule to make it" error. Run esp_idf_size directly instead, pointing it
# at the map file PlatformIO actually generates ($BUILD_DIR/$PROGNAME.map).
# Use PlatformIO's own core Python env ($PYTHONEXE) rather than
# $ESPIDF_PYTHONEXE - the per-framework venv doesn't have esp_idf_size
# installed, but PlatformIO's core env (which backs its built-in "size"
# target) does.
# esp_idf_size learns the chip target from project_description.json, but it only
# trusts that file when the map file's stem matches the app_elf it names - a
# guard for multi-executable projects. PlatformIO names the map after $PROGNAME
# ("firmware") while the IDF project description names the ELF after the CMake
# project ("hub"), so the two never match, the description is discarded, and the
# tool fails with "cannot determine chip target". Hand it a correctly-named copy
# instead. Derived from the project description rather than hardcoded, so this
# works unchanged in any of the Claraxio repos.
def map_file_for_size(build_dir, progname):
    pio_map = os.path.join(build_dir, progname + ".map")
    desc_fn = os.path.join(build_dir, "project_description.json")
    if not os.path.isfile(desc_fn):
        return pio_map
    try:
        with open(desc_fn) as f:
            app_elf = json.load(f).get("app_elf", "")
    except (OSError, ValueError):
        return pio_map
    stem = os.path.splitext(os.path.basename(app_elf))[0]
    if not stem or stem == progname:
        return pio_map
    alias = os.path.join(build_dir, stem + ".map")
    shutil.copyfile(pio_map, alias)
    # The ELF needs the same treatment for the same reason: the project
    # description points at <stem>.elf, and without it the per-archive and
    # per-file tables come back empty rather than failing outright.
    pio_elf = os.path.join(build_dir, progname + ".elf")
    elf_alias = os.path.join(build_dir, stem + ".elf")
    if os.path.isfile(pio_elf):
        shutil.copyfile(pio_elf, elf_alias)
    return alias


def run_idf_size(env, mode_flag=None):
    build_dir = env.subst("$BUILD_DIR")
    progname = env.subst("$PROGNAME")
    map_file = os.path.join(build_dir, progname + ".map")

    if not os.path.isfile(map_file):
        raise Exception(
            f"Map file not found: {map_file}. Build the project first (pio run) "
            "so a map file exists to analyze."
        )
    map_file = map_file_for_size(build_dir, progname)

    cmd = [env.subst("$PYTHONEXE"), "-m", "esp_idf_size"]
    if mode_flag:
        cmd.append(mode_flag)
    cmd.append(map_file)

    print("Command:", " ".join(cmd))

    result = subprocess.run(cmd)
    if result.returncode != 0:
        raise Exception("esp_idf_size failed")


def size_action(source, target, env):
    run_idf_size(env)


def size_components_action(source, target, env):
    run_idf_size(env, "--archives")


def size_files_action(source, target, env):
    run_idf_size(env, "--files")


env.AddCustomTarget(
    name="save-defconfig",
    dependencies=None,
    actions=[save_defconfig_action],
    title="ESP-IDF Save Defconfig",
    description="Generate sdkconfig.defaults from the current sdkconfig",
)

env.AddCustomTarget(
    name="idf-size",
    dependencies=None,
    actions=[size_action],
    title="ESP-IDF Size",
    description="Display the size of the firmware",
)

env.AddCustomTarget(
    name="size-components",
    dependencies=None,
    actions=[size_components_action],
    title="ESP-IDF Size Components",
    description="Display the size of each component in the firmware",
)

env.AddCustomTarget(
    name="size-files",
    dependencies=None,
    actions=[size_files_action],
    title="ESP-IDF Size Files",
    description="Display the size of each file in the firmware",
)
