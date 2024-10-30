This Linux program shows the JPEG preview image embedded within an .ARW file\
Specifically written for .ARW files made by the [Sony a6000](https://en.wikipedia.org/wiki/Sony_%CE%B16000), it might not work on others

## Controls
`Escape` or `q` to exit

## Building
```console
sudo apt install libsfml-dev
./compile.sh
./arw-preview example.ARW
```

## Sony .ARW file format resources
A Sony a6000 .ARW file is a TIFF file, so if you need to manually read one, start here:\
https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf ([archive](https://web.archive.org/web/20240926225851/https://www.itu.int/itudoc/itu-t/com16/tiff-fx/docs/tiff6.pdf))

Despite being under the "Sony Ericsson Tags" section, Sony a6000 .ARW files contain the following:

https://exiftool.org/TagNames/Sony.html#Ericsson

Tag ID `0x0201` (JPEG preview image start offset in bytes)\
Tag ID `0x0202` (JPEG preview image length in bytes)

(Tag ID is a TIFF term, the first 2 bytes of an IFD entry.)

which are 32-bit unsigned integers (4-byte "LONG" type in TIFF spec terms).\
Since the byte count of its type does not exceed 4, it means their value is stored directly in the "Value Offset" of its IFD entry, instead of pointing to a byte offset in the file.

To see the value of these tags within an .ARW file, use exiftool:
```console
$ exiftool -s -hex -PreviewImageStart -PreviewImageLength example.ARW
0x0201 PreviewImageStart               : 136354
0x0202 PreviewImageLength              : 67657
```

If you look close, you'll find these Tag IDs show up twice in the file:
```console
$ exiftool -s -hex example.ARW | grep -E "(0x0201|0x0202)"
0x0201 PreviewImageStart               : 136354
0x0202 PreviewImageLength              : 67657
0x0201 ThumbnailOffset                 : 38676
0x0202 ThumbnailLength                 : 1260
```
Once as preview image, second as thumbnail. This program just reads the first IFD in the file, which contains the PreviewImageStart and PreviewImageLength. (presumably the highest resolution JPEG preview stored in the file)

Description of multiple Sony RAW file formats:\
https://github.com/lclevy/sony_raw

## Too long, didn't read
- Loads the .ARW file into memory (mmap)
- Finds the value of the `PreviewImageStart` and `PreviewImageLength` tags (from the first IFD)
- Loads JPEG image from memory at the above values
- Closes the .ARW file (cleanup() function calls close(), munmap())
- Opens a window to show the JPEG preview image (anti-aliasing, vsync/framerate limit enabled)
