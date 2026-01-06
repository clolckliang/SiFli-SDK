# Creating and Uploading SF-PKG Packages

This section describes how to log in, create, build, and upload your own SF-PKG packages.

## Obtaining an Access Token

1. Open <https://packages.sifli.com/zh> and log in using your GitHub account. Your username will be your GitHub username (all lowercase).
2. After logging in, navigate to **Profile**.
3. In your profile center, apply for an access token (Token) and save it securely. This token will be used for the `sdk.py sf-pkg-login` command.

![Log in to the website](./assert/log_in_to_the_website.png)
![Navigate to Profile](./assert/enter_profile.png)
![Create Token](./assert/create_token.png)
![Obtain Token](./assert/get_token.png)

```{note}
You only need to log in once per computer. Login information will be saved locally.
```

## Log in to sf-pkg

```bash
sdk.py sf-pkg-login -u <lowercase_GitHub_username> -t <obtained_token>
```

![Login Successful](./assert/sdk-pkg-login.png)

```{warning}
The -u parameter must be your lowercase GitHub username! Otherwise, the upload will fail.
```

## Create Package Configuration (sf-pkg-new)

After preparing the driver folder, navigate to that directory in the terminal and execute:

```bash
sdk.py sf-pkg-new --name <package_name>
```

Optional parameters:

- `--version`: Package version number
- `--license`: License declaration
- `--author`: Author
- `--support-sdk-version`: Supported SiFli-SDK version

Example (with version and author information):

```bash
sdk.py sf-pkg-new --name <package_name> --version 1.0.0 --author yourname
```

Upon successful execution, a `conanfile.py` file will be generated. For detailed information, refer to the [Conan Official Documentation](https://docs.conan.io/en/latest/reference/conanfile.html).

Generally, the default content of the `conanfile.py` file is as follows:

```python
from conan import ConanFile

class Example_AddRecipe(ConanFile):
    name = "example_add"
    version = "0.1.0"

    license = "Apache-2.0"
    user = "halfsweet"
    author = "halfsweet"
    url = "<Package recipe repository url here, for issues about the package>"
    homepage = "<Package homepage here>"
    description = "<Description of hello package here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")

    support_sdk_version = "^2.4"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "*"

    python_requires = "sf-pkg-base/[^1.0]@sifli"
    python_requires_extend = "sf-pkg-base.SourceOnlyBase"


    def requirements(self):
        # add your package dependencies here, for example:
        # self.requires("fmt/8.1.1")
        pass
```

Pay close attention to the following parameters:

- `name`: Package name, required. Used as a unique identifier on `https://packages.sifli.com`.
- `version`: Package version number. It is recommended to use semantic versioning, such as `0.0.1`, `1.0.0`, etc.
- `user`: Your username (also called `namespace`) on `https://packages.sifli.com`, required.
- `license`: Open-source license. Common options include `Apache-2.0`, `MIT`, `GPL-3.0`, `BSD-3-Clause`, etc. The default is `Apache-2.0`.
- `author`: Package author/maintainer name, optional.
- `url`: Package repository URL (GitHub/GitLab, etc.), optional.
- `description`: Detailed description of the package, optional.
- `topics`: Package tags/topics for easier searching and categorization. Use tuple format, optional.
- `support_sdk_version`: Supported SiFli-SDK version, required. Format follows [Semantic Versioning range](https://semver.org/).

The combination of `user` and `name` serves as the unique identifier for the package.

The `requirements` method is used to add package dependencies:

```python
def requirements(self):
    # add your package dependencies here, for example:
    # self.requires("fmt/8.1.1")
    pass
```

This indicates that no dependencies have been added. You can add required packages in the `requirements` method. For example:

```python
def requirements(self):
    self.requires("sht30/0.0.4@caisong123")
```

`self.requires` is used to specify dependencies in the format `package_name/version@username`.

## Build Package (sf-pkg-build)

Execute the following command in the driver folder:

```bash
sdk.py sf-pkg-build --version <version_number>
```

![Build Package](./assert/sf-pkg-build.png)

> It is recommended to use semantic versioning for the version number, such as `0.0.1`, `1.0.0`, etc.

## Upload Package (sf-pkg-upload)

```bash
sdk.py sf-pkg-upload --name <package_name>/<version>@<username>
```

![Upload Package](./assert/sf-pkg-upload.png)

Command format explanation:

- `package_name`: The package name defined in `conanfile.py`
- `version`: The version number specified during build
- `username`: Your GitHub username

### Handling Upload Failures

1. Clear local cache:
   ```bash
   sdk.py sf-pkg-remove --name <package_name>
   ```
2. Rebuild:
   ```bash
   sdk.py sf-pkg-build --version <version_number>
   ```
3. Upload again:
   ```bash
   sdk.py sf-pkg-upload --name <package_name>/<version>@<username>
   ```

### Verify Upload Result

After a successful upload, you can view the uploaded package on the server website:

![Package on Server](./assert/pkg_in_website.png)
