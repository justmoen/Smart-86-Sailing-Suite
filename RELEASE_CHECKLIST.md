# Release checklist

Use this for each firmware release.

## 1. Update the build version

Edit [CMakeLists.txt](CMakeLists.txt) and bump the project version.

Example:

```cmake
project(sailing_suite VERSION 0.7.0)
```

This is the version value used at compile time via:

```cmake
add_compile_definitions(FIRMWARE_VERSION=\"${PROJECT_VERSION}\")
```

## 2. Build the firmware

From the project root, with the ESP-IDF environment loaded:

```bash
idf.py build
```

Verify the build log shows the expected app version:

```text
-- App "sailing_suite" version: 0.7.0
```

## 3. Tag the release in git

Create and push a tag matching the release version:

```bash
git tag v0.7.0
git push origin v0.7.0
```

The GitHub release tag should use the same version, typically with a leading `v`.

## 4. Publish the GitHub release

Create a GitHub Release for tag `v0.7.0`.

- Attach the firmware binary asset if desired
- Include release notes
- Keep the release tag name in sync with the version number

## 5. OTA expectations

The firmware compares the running version against the GitHub release tag.

- The OTA code strips the leading `v` when comparing versions
- The app's runtime version is generated from the CMake version value
- The GitHub tag is the release identifier used for update discovery

## 6. Important note

Do not maintain three independent version values unless you intend to override the compile-time version.

The intended pattern is:

- CMake project version: `0.7.0`
- Git tag: `v0.7.0`
- Fallback string in code: only a default if the compile-time definition is absent

## 7. Recommended release pattern

Use this release sequence for each new version:

```bash
# 1. update CMakeLists.txt
# 2. build firmware
# 3. tag the repo
# 4. push the tag
# 5. publish the GitHub release
# 6. test OTA update path
```
