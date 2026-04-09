"""
Pre-build script: generate thin wrapper headers in include/ so that
LovyanGFX's Bus_RGB.hpp can find all its dependencies from our source files.

Bus_RGB.hpp guards itself with __has_include(<esp_lcd_panel_rgb.h>) and also
needs <esp_private/gdma.h> and <hal/dma_types.h>.  We create wrapper headers
that redirect to the real SDK files via absolute paths.

IMPORTANT: We do NOT modify CPPPATH — adding SDK directories to CPPPATH
pollutes the include order for framework libraries (SPI, Wire) and causes
'assert not declared' errors in their internal headers.
"""
Import("env")
import os


def _write_wrapper(wrapper_path, real_path):
    """Write a wrapper header that includes the real header via absolute path.
    Returns 1 if created/exists, 0 on failure."""
    content = (
        f"// Auto-generated wrapper -- do not edit\n"
        f'#include "{real_path}"\n'
    )
    if os.path.isfile(wrapper_path):
        with open(wrapper_path, "r") as fh:
            if fh.read() == content:
                return 1
    with open(wrapper_path, "w") as fh:
        fh.write(content)
    parent = os.path.basename(os.path.dirname(wrapper_path))
    fname = os.path.basename(wrapper_path)
    rel_name = f"{parent}/{fname}" if parent != "include" else fname
    print(f"  + {rel_name}")
    return 1


def _find_header_in_subdir(sdk_root, subdir, header_name):
    """Find a header like 'esp_private/gdma.h' in the SDK include tree.
    Looks for directories named `subdir` that contain `header_name`.
    Returns the absolute path or None."""
    target = "/" + subdir
    for root, dirs, files in os.walk(sdk_root):
        normalized = root.replace("\\", "/")
        if normalized.endswith(target) or os.path.basename(root) == subdir:
            if header_name in files:
                return os.path.join(root, header_name).replace("\\", "/")
    return None


# ── Main logic ────────────────────────────────────────────
print("\n[ISS-TRACKER] Generating SDK wrapper headers...")

try:
    libs_dir = env.PioPlatform().get_package_dir(
        "framework-arduinoespressif32-libs"
    )
    esp32s3_inc = os.path.join(libs_dir, "esp32s3", "include")
    project_inc = os.path.join(env["PROJECT_DIR"], "include")

    if not os.path.isdir(esp32s3_inc):
        print(f"[ISS-TRACKER] WARNING: {esp32s3_inc} does not exist")
    else:
        os.makedirs(project_inc, exist_ok=True)
        created = 0

        # 1) Flat wrappers for all esp_lcd*.h headers
        for root, dirs, files in os.walk(esp32s3_inc):
            for fname in files:
                if fname.startswith("esp_lcd") and fname.endswith(".h"):
                    real_path = os.path.join(root, fname).replace("\\", "/")
                    wrapper = os.path.join(project_inc, fname)
                    created += _write_wrapper(wrapper, real_path)

        # 2) Subdirectory wrappers for headers that Bus_RGB.hpp includes
        #    with path prefixes: <esp_private/...> and <hal/...>
        subdir_headers = [
            ("esp_private", "gdma.h"),
            ("esp_private", "gdma_types.h"),
            ("hal", "dma_types.h"),
            ("hal", "gdma_types.h"),
        ]

        for subdir, hname in subdir_headers:
            real_path = _find_header_in_subdir(esp32s3_inc, subdir, hname)
            if real_path:
                wrapper_dir = os.path.join(project_inc, subdir)
                os.makedirs(wrapper_dir, exist_ok=True)
                wrapper = os.path.join(wrapper_dir, hname)
                created += _write_wrapper(wrapper, real_path)
            else:
                print(f"  ? {subdir}/{hname} not found in SDK (may not be needed)")

        print(f"[ISS-TRACKER] {created} wrapper header(s) ready in include/\n")

except Exception as e:
    import traceback
    print(f"[ISS-TRACKER] ERROR: {e}")
    traceback.print_exc()
    print()
