from os import listdir
from os.path import join, isfile, isdir

Import("env")

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-arduino-avr")
PROJECT_DIR = env["PROJECT_DIR"]
patch_dir = join(PROJECT_DIR, "scripts", "patches")
patchflag_path = join(FRAMEWORK_DIR, ".patching-done")

assert isdir(patch_dir)

# patch file only if we didn't do it before
if isfile(patchflag_path):
    env.Execute("patch -p0 -R -d %s -i %s" % (FRAMEWORK_DIR, patchflag_path))
    env.Execute("rm %s" % (patchflag_path))

for filename in listdir(patch_dir):
    patch_file = join(patch_dir, filename)
    env.Execute("patch -p0 -d %s -i %s" % (FRAMEWORK_DIR, patch_file))
    env.Execute("cat %s >> %s" % (patch_file, patchflag_path))
