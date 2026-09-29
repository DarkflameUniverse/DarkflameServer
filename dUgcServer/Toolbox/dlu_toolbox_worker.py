# The UGC server's LU Toolbox worker (docs/UgcServer.md, "LU Toolbox in Blender"). Runs inside a headless Blender
# started by the UGC server and stays up, making one model after another:
#
#   blender -b --factory-startup -t <threads> --python dlu_toolbox_worker.py -- \
#       --standalone <LU-Toolbox-Standalone> --brickdb <folder> --res <client res> --device cpu
#
# Each model goes through LU Toolbox itself, as LU-Toolbox-Standalone's lu_batch_driver.py runs it (its functions are
# used, not copied): the LU Toolbox importer, Process Model, Bake Lighting and the niftools .nif export for LEGO
# Universe. Nothing of LU Toolbox is changed or re-implemented here; this file only keeps Blender up between models.
#
# The protocol: one JSON object per line. Requests come on stdin; replies go to the original stdout, which only this
# script writes to (everything Blender and the add-ons print is moved to stderr). Every message has "dlutb": 1.
#   -> {"dlutb":1,"cmd":"make","id":7,"input":"/x/7.lxfml","output":"/x/7.nif","lods":[0,2]}
#   <- {"dlutb":1,"type":"done","id":7,"ok":true,"ms":{"reset":..,"import":..,"process":..,"bake":..,"export":..}}
#   <- {"dlutb":1,"type":"done","id":7,"ok":false,"error":"..."}
#   -> {"dlutb":1,"cmd":"ping","id":8}      <- {"dlutb":1,"type":"pong","id":8}
#   -> {"dlutb":1,"cmd":"quit"}             (the worker exits; it also exits when stdin closes)
# When it is ready for work it sends {"dlutb":1,"type":"ready","blender":..,"toolbox":..,"niftools":..,"device":..},
# or {"dlutb":1,"type":"failed","error":..} and exits when it can't be.

import argparse
import json
import os
import sys
import time
import traceback
import zipfile

PROTOCOL = 1

# The replies' channel is the original stdout; stdout itself now goes to stderr, so nothing else can write into it
_reply = os.fdopen(os.dup(1), "w", encoding="utf-8", newline="\n")
os.dup2(2, 1)


def send(message):
    message = dict(message)
    message["dlutb"] = PROTOCOL
    _reply.write(json.dumps(message, separators=(",", ":")) + "\n")
    _reply.flush()


def log(*parts):
    print("[dlu-toolbox]", *parts, file=sys.stderr, flush=True)


def script_args():
    argv = sys.argv
    return argv[argv.index("--") + 1:] if "--" in argv else []


def prepare_brickdb(brickdb, res):
    """
    LU Toolbox reads the brick data from a folder with the brick database unpacked (Assemblies/, Primitives/,
    Materials.xml) and the client's brickprimitives/. Given the client's res folder it unpacks brickdb.zip there
    itself, so a folder of its own is made instead: brickdb.zip unpacked into it and brickprimitives/ linked file by
    file (LU Toolbox lists the folder without following linked folders).
    """
    os.makedirs(brickdb, exist_ok=True)
    if not os.path.isdir(os.path.join(brickdb, "Assemblies")):
        with zipfile.ZipFile(os.path.join(res, "brickdb.zip")) as archive:
            archive.extractall(brickdb)
        log("unpacked brickdb.zip into", brickdb)
    source = os.path.join(res, "brickprimitives")
    target = os.path.join(brickdb, "brickprimitives")
    if os.path.isdir(source) and not os.path.isdir(target):
        linked = 0
        for root, _, files in os.walk(source):
            folder = os.path.join(target, os.path.relpath(root, source))
            os.makedirs(folder, exist_ok=True)
            for name in files:
                src, dst = os.path.join(root, name), os.path.join(folder, name)
                if os.path.lexists(dst):
                    continue
                try:
                    os.symlink(src, dst)
                except OSError:
                    import shutil
                    shutil.copyfile(src, dst)
                linked += 1
        log("linked", linked, "brickprimitives files into", target)


def set_device(driver, device):
    import bpy
    if device == "hip":
        # lu_batch_driver's device choice knows CPU, CUDA and OptiX; AMD GPUs through HIP the same way
        try:
            bpy.ops.preferences.addon_enable(module="cycles")
        except Exception:
            pass
        cp = bpy.context.preferences.addons["cycles"].preferences
        try:
            cp.compute_device_type = "HIP"
            driver._refresh_cycles_devices(cp)
            found = False
            for d in cp.devices:
                d.use = d.type in ("HIP", "CPU")
                found = found or d.type == "HIP"
            if found:
                bpy.context.scene.cycles.device = "GPU"
                return "hip"
        except Exception as ex:
            log("HIP unavailable:", ex)
        bpy.context.scene.cycles.device = "CPU"
        return "cpu"
    if device == "auto":
        return driver.set_cycles_device_auto()
    return driver.set_cycles_device_forced(device)


def addon_version(name):
    import addon_utils
    for module in addon_utils.modules():
        if module.__name__ == name:
            return ".".join(str(part) for part in module.bl_info.get("version", ()))
    return ""


class Worker:
    def __init__(self, args):
        self.args = args
        sys.path.insert(0, args.standalone)
        import lu_batch_driver  # LU-Toolbox-Standalone's driver: its steps, run as it runs them
        self.driver = lu_batch_driver
        self.device = "cpu"

    def setup(self):
        """What lu_batch_driver's main does before importing, for a scene just loaded"""
        import bpy
        for module in ("lu_toolbox", "io_scene_niftools"):
            try:
                bpy.ops.preferences.addon_enable(module=module)
            except Exception as ex:
                log("enabling", module, "failed:", ex)
        if "lu_toolbox" not in bpy.context.preferences.addons:
            raise RuntimeError("the lu_toolbox add-on can't be enabled (is it in the Blender scripts folder's addons?)")
        if "io_scene_niftools" not in bpy.context.preferences.addons:
            raise RuntimeError("the io_scene_niftools add-on can't be enabled (is it in the Blender scripts folder's addons?)")
        bpy.context.preferences.addons["lu_toolbox"].preferences.brickdbpath = self.args.brickdb
        self.driver._apply_headless_patches()
        self.device = set_device(self.driver, self.args.device)
        self.driver.set_lu_gpu_flags(use_gpu=(self.device != "cpu"))

    def reset(self):
        """Every model starts from a fresh scene, as a Blender started for it would"""
        import bpy
        # niftools keeps the last export operator to report through, which is gone once it has finished; a second
        # export (or enabling the add-on again) would report through it and fail. Its default reporter instead.
        try:
            from io_scene_niftools.utils import logging as nif_logging
            nif_logging.NifLog.op = nif_logging._MockOperator()
        except Exception:
            pass
        bpy.ops.wm.read_factory_settings(use_empty=False)
        self.setup()

    def make(self, request):
        ms = {}
        step = time.perf_counter()

        def lap(name):
            nonlocal step
            now = time.perf_counter()
            ms[name] = round((now - step) * 1000)
            step = now

        self.reset()
        lap("reset")
        lods = request.get("lods") or []
        lod_kwargs = {f"importLOD{n}": n in lods for n in range(4)} if lods else None
        self.driver.try_import_lxf(os.path.abspath(request["input"]), op_override=None, lod_kwargs=lod_kwargs)
        lap("import")
        self.driver.call_op("lutb.process_model", "Process Model")
        lap("process")
        self.driver.ensure_vertex_colors_exist()
        self.driver.call_op("lutb.bake_lighting", "Bake Lighting")
        lap("bake")
        output = os.path.abspath(request["output"])
        os.makedirs(os.path.dirname(output), exist_ok=True)
        self.driver.set_niftools_game_to_lu()
        self.driver.export_nif(output)
        lap("export")
        if not os.path.isfile(output):
            raise RuntimeError("the .nif export wrote nothing")
        return ms


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--standalone", required=True)
    parser.add_argument("--brickdb", required=True)
    parser.add_argument("--res", default="")
    parser.add_argument("--device", default="cpu", choices=["auto", "cpu", "cuda", "optix", "hip"])
    args = parser.parse_args(script_args())
    try:
        import bpy
        if args.res:
            prepare_brickdb(args.brickdb, args.res)
        worker = Worker(args)
        worker.setup()
        send({"type": "ready", "blender": bpy.app.version_string, "toolbox": addon_version("lu_toolbox"),
              "niftools": addon_version("io_scene_niftools"), "device": worker.device})
    except Exception as ex:
        traceback.print_exc()
        send({"type": "failed", "error": f"{type(ex).__name__}: {ex}"})
        return 2

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except ValueError:
            log("not JSON:", line[:200])
            continue
        if not isinstance(request, dict) or request.get("dlutb") != PROTOCOL:
            log("unknown message:", line[:200])
            continue
        cmd = request.get("cmd")
        if cmd == "quit":
            break
        if cmd == "ping":
            send({"type": "pong", "id": request.get("id")})
            continue
        if cmd != "make":
            send({"type": "done", "id": request.get("id"), "ok": False, "error": f"unknown command {cmd}"})
            continue
        log("making", request.get("input"))
        try:
            ms = worker.make(request)
            send({"type": "done", "id": request.get("id"), "ok": True, "ms": ms})
        except BaseException as ex:  # SystemExit too: the driver's helpers exit on failures
            traceback.print_exc()
            send({"type": "done", "id": request.get("id"), "ok": False, "error": f"{type(ex).__name__}: {ex}"})
    return 0


if __name__ == "__main__":
    code = main()
    _reply.close()
    # Blender would go on to its own shutdown, which is slow with add-ons loaded and not needed
    os._exit(code)
