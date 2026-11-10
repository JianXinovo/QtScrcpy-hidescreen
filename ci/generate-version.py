import sys
import os
import subprocess

# 仓库还没有任何 tag 时使用的兜底版本号。
# 不给兜底的话 appversion 会被写成空串，project(VERSION) 取不到值，
# QtScrcpy/res/QtScrcpy.rc 里的 FILEVERSION 变成空，资源编译直接报
# "RC2127: version WORDs separated by commas expected"。
DEFAULT_VERSION = "4.2.1"


def git_output(args):
    """执行 git 命令；失败（无 tag、浅克隆、git 不存在）一律返回空串。"""
    try:
        return subprocess.check_output(args, stderr=subprocess.DEVNULL).decode("utf-8", "ignore").strip()
    except Exception:
        return ""


if __name__ == "__main__":
    commit = git_output(["git", "rev-list", "--tags", "--max-count=1"])
    tag = git_output(["git", "describe", "--tags", commit]) if commit else ""

    version = tag[1:] if tag.startswith("v") else tag
    if not version:
        version = DEFAULT_VERSION

    version_file = os.path.abspath(os.path.join(os.path.dirname(__file__), "../QtScrcpy/appversion"))
    file = open(version_file, "w")
    file.write(version)
    file.close()
    sys.exit(0)
