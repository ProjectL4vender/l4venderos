SystemC 3.0 BL0SS0M MOD.0 - ITWLM Network Support
==========================================

This supplemental ISO contains the current E1000 network stack source and a
package manifest for engineering/debugging. It is a data ISO, not bootable.

In the running SystemC 3.0 Terminal, type:
    itwlmcli install network auto

In this release the E1000 driver is statically linked into systemc-i386.krn;
the command reinitializes the driver and requests DHCP. SystemC does not yet
have a general ISO9660/ATAPI package installer or dynamic kernel-module loader,
so this package ISO is not dynamically read by the running OS.

Supported virtual NIC: Intel E1000. HTTPS/TLS is not implemented by the browser.
