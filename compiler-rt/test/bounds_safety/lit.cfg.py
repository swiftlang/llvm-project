# -*- Python -*-

import os
import shlex

import lit.formats


config.name = "BoundsSafety :: " + config.name_suffix

config.suffixes = [".c"]

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(
    config.compiler_rt_obj_root, "test", "bounds-safety",
    config.name_suffix
)

# Build the %clang_bsafe substitution.
clang_bsafe_flags = [
    config.clang,
    "-fbounds-safety",
    "-g",
    "-O2" if config.optimized else "-O0",
    config.target_cflags,
]
if config.trap_kind == "unique-traps":
    clang_bsafe_flags.append("-fbounds-safety-unique-traps")
elif config.trap_kind == "merged-traps":
    clang_bsafe_flags.append("-fno-bounds-safety-unique-traps")
    assert config.optimized
elif config.trap_kind == "soft-traps":
    clang_bsafe_flags.append("-fbounds-safety-soft-traps=call-minimal")

# Add utils/ to include path for soft_trap_runtime_impl.h.
utils_dir = os.path.join(os.path.dirname(__file__), "utils")
clang_bsafe_flags.append("-I" + utils_dir)

# Expose configuration properties as preprocessor macros for test cases.
clang_bsafe_flags.append("-DTEST_OPTIMIZED={}".format(int(config.optimized)))
clang_bsafe_flags.append("-DTEST_UNIQUE_TRAPS={}".format(
    int(config.trap_kind != "merged-traps")))
clang_bsafe_flags.append("-DTEST_USE_DEBUGGER={}".format(int(config.use_debugger)))
clang_bsafe_flags.append("-DTEST_SOFT_TRAP={}".format(
    int(config.trap_kind == "soft-traps")))

config.substitutions.append(
    ("%clang_bsafe", " " + " ".join(clang_bsafe_flags) + " ")
)

# Build %expect-trap and %expect-no-trap substitutions.
python_exec = shlex.quote(config.python_executable)
scripts_dir = os.path.join(os.path.dirname(__file__), "scripts")
expect_trap_script = os.path.join(scripts_dir, "expect_trap.py")
expect_no_trap_script = os.path.join(scripts_dir, "expect_no_trap.py")

debugger_flags = ""
if config.use_debugger:
    debugger_flags = " --use-debugger --lldb-python-path {}".format(
        shlex.quote(config.lldb_python_path)
    )

merged_trap_flag = " --merged-traps" if config.trap_kind == "merged-traps" else ""
soft_trap_flag = " --soft-traps" if config.trap_kind == "soft-traps" else ""

config.substitutions.append(
    ("%expect-trap", "{} {}{}{}{}".format(
        python_exec, expect_trap_script, debugger_flags, merged_trap_flag,
        soft_trap_flag))
)
config.substitutions.append(
    (
        "%expect-no-trap",
        "{} {}{}{}".format(python_exec, expect_no_trap_script, debugger_flags,
                           soft_trap_flag),
    )
)

# Add features for REQUIRES/UNSUPPORTED lines.
if config.use_debugger:
    config.available_features.add("run-under-debugger")
else:
    config.available_features.add("run-directly")
if config.optimized:
    config.available_features.add("optimized")
else:
    config.available_features.add("unoptimized")
config.available_features.add(config.trap_kind)
