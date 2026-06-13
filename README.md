# Shipyard
My custom package manager! **(WIP)**

### Basic usage
```
Commands:
   help:               (usage: ship help)                        Show help screen
   dock/install        (usage: ship dock <path> <exec>)          Install a pre-built package (folder or tarball)
   build               (usage: ship build <path> [cfg-args])     Build & install via Makefile/CMake (folder or tarball)
   jettison/uninstall: (usage: ship jettison <package>)          Uninstall a package
   service/update      (usage: ship service [package])           Update the package database/a package
   manifest/list       (usage: ship manifest)                    Displays a list of all installed packages
   inspect                (usage: ship inspect <package>)              Show details for an installed package
   port                (usage: ship port <url> [command])        Use the package list from another port
```

To add packages see [Creating Ports](https://github.com/Codiak540/Shipyard/wiki#creating-ports)
