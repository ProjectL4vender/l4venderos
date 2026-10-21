import os
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class ReleaseFeatureTests(unittest.TestCase):
    def test_i386_desktop_features_present(self):
        with open(os.path.join(ROOT, "source", "kernel", "kernel.c"), "r") as stream:
            source = stream.read()
        for feature in (
            "draw_mouse_cursor", "draw_clock", "window_dx", "dragging_window",
            "PAINT STUDIO", "NOTEPAD", "handle_mouse", "net_init()", "net_poll()",
            "draw_context_menu", "framebuffer_backbuffer", "SYSTEMCSETUP-CLI --RESOLUTION",
            "ITWLMCLI INSTALL NETWORK AUTO", "SYSTEMC VERSION 3.0",
            "BL0SS0M MOD.0", "draw_calendar", "draw_help_center", "draw_task_manager",
            "COPYRIGHT (C) 2023-2026", "rtc_read_day_month_year", "try_login",
        ):
            self.assertIn(feature, source)

    def test_resolution_is_build_time_configurable(self):
        with open(os.path.join(ROOT, "build.sh"), "r") as stream:
            builder = stream.read()
        with open(os.path.join(ROOT, "source", "arch", "boot.S"), "r") as stream:
            boot = stream.read()
        self.assertIn("SYSTEMC_RESOLUTION", builder)
        self.assertIn("SC_BOOT_WIDTH", builder)
        self.assertIn("SC_BOOT_HEIGHT", builder)
        self.assertIn(".long SC_BOOT_WIDTH", boot)
        self.assertIn(".long SC_BOOT_HEIGHT", boot)
        with open(os.path.join(ROOT, "source", "arch", "vbe.S"), "r") as stream:
            vbe = stream.read()
        self.assertIn("vbe_rm_pitch_check", vbe)
        self.assertIn("vbe_pm16", vbe)

    def test_new_desktop_apps_and_session_password(self):
        with open(os.path.join(ROOT, "source", "kernel", "kernel.c"), "r") as stream:
            source = stream.read()
        for app in ("CALENDAR", "HELP", "TASKMGR", "draw_calendar", "draw_help_center", "draw_task_manager"):
            self.assertIn(app, source)
        self.assertIn("if(str_eq(login_password,setup_password))", source)
        self.assertIn("COPYRIGHT (C) 2023-2026 L4VENDER PROJECT", source)

    def test_network_iso_builder_is_separate_and_data_only(self):
        path = os.path.join(ROOT, "build-network-iso.sh")
        self.assertTrue(os.path.isfile(path))
        with open(path, "r") as stream:
            builder = stream.read()
        self.assertIn("SystemC-3.0-network-support.iso", builder)
        self.assertIn("verify_iso9660.py", builder)
        self.assertIn("/ITWLM/manifest.ini", builder)

    def test_iso_builder_requests_grub_framebuffer_payload(self):
        with open(os.path.join(ROOT, "build-iso.sh"), "r") as stream:
            source = stream.read()
        self.assertIn("set gfxpayload=keep", source)
        self.assertIn("-report_el_torito plain", source)
        self.assertIn('cmp "$KERNEL" "$CHECKDIR/kernel.krn"', source)
        self.assertIn('tools/verify_iso9660.py', source)
        self.assertIn('xorriso -indev "$TMPISO" -pvd_info', source)
        self.assertIn('GRUB_VERSION=', source)
        self.assertNotIn('file "$TMPISO" | grep', source)

    def test_release_is_i386_only(self):
        with open(os.path.join(ROOT, "VERSION"), "r") as stream:
            version = stream.read().lower()
        self.assertIn("i386 only", version)
        self.assertIn("3.0", version)
        self.assertIn("bl0ss0m mod.0", version.lower())
        self.assertNotIn("x64", version)


if __name__ == "__main__":
    unittest.main()
