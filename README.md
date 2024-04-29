# Configurable Binary Patcher

This C++ program is a simple binary patcher, configurable through `.ini`-esque patch files.

How to use it:

    PS C:\> patch
    Usage: patch <file to be patched> <patch configuration file>
How does the actual patch-file look like:

    [config]
    backup = true
    pre_crc32 = CAFEBABE
    post_crc32 = C0DEBA5E
    
    [data]
    0x0000 = DE AD BE EF
    0x1234 = AC DC
`[config]` parameters are optional, `backup` will create a backup of your file and is set to true by default, `pre_crc32` will check the CRC32 of your file before the patch and `post_crc32` will verify the expected CRC32 of your file after the patch. Each CRC32 check can be skipped by just leaving it out.
The `[data]` section contains lines in the form of `<offset> = <data>` and simply signals which data should be written from the given offset.
Currently there is no way to automatically generate these patch-files since this is only meant for small hand-made patches.

See the `patches` directory for example files.