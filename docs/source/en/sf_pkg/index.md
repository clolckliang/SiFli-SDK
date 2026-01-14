# SiFli Package Registry

Similar to [RT-PKG](../app_note/rt-pkg.md), SiFli Package Registry is also a package manager. However, SiFli Package Registry uses [Conan](https://conan.io/) as its underlying package management tool, supporting a rich set of package management features. It facilitates the creation, upload, and download of driver packages, and supports semantic versioning.
- Click here to access the [SiFli Package Registry](https://packages.sifli.com/)
## Prerequisites

- Navigate to a valid SDK project directory
- Execute `.\export.ps1` (Windows) or `./export.sh` (Linux/macOS) to initialize the environment

## Quick Reference Command Table

| Command                                                   | Description                |
| --------------------------------------------------------- | -------------------------- |
| `sdk.py sf-pkg-login -u <username> -t <token>`            | Log in to the sf-pkg system |
| `sdk.py sf-pkg-new --name <package_name>`                 | Create a new package configuration |
| `sdk.py sf-pkg-build --version <version>`                 | Build a package            |
| `sdk.py sf-pkg-upload --name <package_name>/<version>@<username>` | Upload a package to the server |
| `sdk.py sf-pkg-remove --name <package_name>`              | Clear local cache          |
| `sdk.py sf-pkg-init`                                      | Initialize dependency configuration |
| `sdk.py sf-pkg-install`                                   | Install dependency packages |
| `conan search "<package_name>/*" -r=artifactory`          | Search for available packages |

```{toctree}
:titlesonly:

use
create
faq
```
