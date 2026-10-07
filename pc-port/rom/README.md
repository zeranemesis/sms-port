# rom/

Put your copy of **Super Mario Sunshine (North America, GMSE01, Rev 0)** here: one `.iso`, `.gcm` or Dolphin `.ciso` file, any file name.

- `./run.sh` plays it when you give no path.
- `./build.sh` also packs it into a standalone executable (`sms-standalone`, or `SMS.app` on macOS) that runs without it.

Git ignores everything in this folder except this file, so the image is never committed.
The port reads the image in place; it never extracts or changes it.
