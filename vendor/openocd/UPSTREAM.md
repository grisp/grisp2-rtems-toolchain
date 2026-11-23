# Jim Tcl source archive for OpenOCD

The adjacent archive is a source snapshot from the Jim Tcl repository:

- Repository: <https://github.com/msteveb/jimtcl>
- Commit: `a77ef1a6218fad4c928ddbdc03c1aedc41007e70`
- OpenOCD gitlink: `external/openocd-code:jimtcl` at the same commit
- Archive SHA-256: `fd111331a08ef7bad4eaa6e20f8c122977b5a26aeeac29873e0b19bb719048f3`

The toolchain unpacks this archive into its ignored OpenOCD staging directory
and runs OpenOCD's bootstrap with `nosubmodule`, so building OpenOCD does not
fetch nested Git repositories. The OpenOCD build disables J-Link and does not
need its other nested submodules.

The archive contains Jim Tcl's upstream `LICENSE` file.
