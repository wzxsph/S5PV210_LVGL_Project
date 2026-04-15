import os
import runpy
import sys


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    target = os.path.join(script_dir, "template-framebuffer-gui", "tftp_server.py")

    if not os.path.exists(target):
        print(f"[ERROR] Target script not found: {target}")
        return 1

    sys.argv[0] = target
    runpy.run_path(target, run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
