# LittleSpeaker SD-Card contents

This is a quick how-to for the SD-Card contents of LittleSpeaker

## root files

We have some files that reside on the root directory (Top level, under
windows this is directly on the drive in no folder).

- `wifi.txt` WiFi settings, this file contains 2 lines:
   1. WiFi name, attention this is case sensitive
   2. WiFi password in plaintext
- `webradio.txt` URLs to webradio streams, start a line with `#` to
  include comments. See *webradio* below for more info

If the connection to the WiFi does not work check the following:

1. WiFi has to be on the 2.4GHz band, 5GHz does not work
2. WiFi has to support WPA2
3. If nothing else is the problem, check that you're using "Unix line endings",
   this is usually only a problem on Windows machines.

## System directory

The `system` directory should contain the announcer voice files to be used for
the menu system, I will describe which one is which in the following.

The default announcer voice has been generated with a text to speech computer
voice, but if you want you can exchange them for another voice.

Format of the files should be a *stereo* MP3 file with a reasonable bitrate and
length usable for the task. Remove all ID3 tag data to make the menu more
responsive.

- Numbers 1 to 99 (e.g. `1.mp3`): Will be used for
  - Album numbers if there is no announcer (see below at *album directories*)
  - Track numbers in albums
  - Webradio station numbers
- `album.mp3` used before an album number
- `bluetooth_on.mp3` used when entering the bluetooth menu and turning bluetooth on
- `bluetooth_pair.mp3` played when a device pairs to LittleSpeaker in bluetooth mode
- `bluetooth.mp3` menu title for the bluetooth menu
- `connection_failed.mp3` played when a connection to a webradio station fails or is
  disconnected by the server.
- `hello.mp3` will be played on bootup after SD-Card index has been created
- `sd.mp3` menu title of the SD-Card menu
- `stopped.mp3` played when an album has finished, the user disconnects a webradio
  station by pressing stop or when a device disconnects from bluetooth
- `track.mp3` played when in an album before the track number
- `webradio.mp3` menu title of webradio menu

## webradio

### Requirements

There are some things you'll have to consider for the streams that can
be played by the firmware of LittleSpeaker:

1. The stream has to be an IceCast or ShoutCast stream.
2. You have to link to a stream, not the stream's playlist. Usually webradio
   stations will link a playlist file (either `m3u`, `m3u8` or `pls`) which
   contains multiple streaming URLs to provide multiple servers.
3. The stream has to be MP3, AAC and HeAAC (or AAC+) do not work currently.
4. Keep the bitrate at a reasonable value, 320 kBit is cool and all, but you
   will probably not hear a difference to a 192 kBit (or even a 128 kBit) stream.
5. Try to keep to http and not https as it is faster to process on device
6. Maximum line/URL length is 255 characters
7. Maximum number of stations is 25

### Announcer

If you want LittleSpeaker to tell you a webradio name instead of the channel
number in the menu you can add an audio file to the `webradio` directory. The
file should be a *stereo* MP3 file with a reasonable bitrate and length.
Name it the channel number in two digits (leading zero). The file will be played
back if you select the channel.

### Verification

To verify a Stream URL fetch it with curl like this:
```
curl -v http://ice2.somafm.com/groovesalad-128-mp3
```

   You will get an output like this:
```
*   Trying 173.239.76.149:80...
* Connected to ice2.somafm.com (173.239.76.149) port 80
> GET /groovesalad-128-mp3 HTTP/1.1
> Host: ice2.somafm.com
> User-Agent: curl/8.4.0
> Accept: */*
> 
< HTTP/1.1 200 OK
< Content-Type: audio/mpeg
< Date: Fri, 12 Jan 2024 18:42:54 GMT
< icy-br:128
< icy-genre:Ambient Chill
< icy-name:Groove Salad: a nicely chilled plate of ambient beats and grooves. [SomaFM]
< icy-notice1:<BR>This stream requires <a href="http://www.winamp.com/">Winamp</a><BR>
< icy-notice2:SHOUTcast Distributed Network Audio Server/Linux v1.9.5<BR>
< icy-pub:0
< icy-url:http://somafm.com
< Server: Icecast 2.4.0-kh15
< Cache-Control: no-cache, no-store
< Expires: Mon, 26 Jul 1997 05:00:00 GMT
< Connection: Close
< Access-Control-Allow-Origin: *
< Access-Control-Allow-Headers: Origin, Accept, X-Requested-With, Content-Type, Icy-MetaData
< Access-Control-Allow-Methods: GET, OPTIONS, SOURCE, PUT, HEAD, STATS
< 
Warning: Binary output can mess up your terminal. Use "--output -" to tell 
Warning: curl to output it to your terminal anyway, or consider "--output 
Warning: <FILE>" to save to a file.
* Failure writing output to destination
* Closing connection
```

It is good if curl tells you the content is a binary stream as this means
you actually got a stream URL and not the playlist that is linked. If you get
back text, it's probably a list of URLs. Sometimes you will get back a redirect
which means the service jumps to another server, you will see that if a `Location`
header is in the server response. Try that URL instead.

To verify you got an IceCast stream look for `icy-`-Headers which tell the device
Stream metadata.

You will get back a `Content-Type`-Header usually. This should be `audio/mpeg` for
streams that are supported. If you get `audio/aac` or something like that you
got an incompatible stream.

## Album directories

All directories that are not named `system` or `webradio` are assumed to be
music albums that can be played back. The Firmware can currently only play back
MP3 files, but some MP3 files contain big chunks of ID3 Metatag data and will
crash the Firmware. So if you're selecting an album, press play and then hear
the announcer voice greeting you with a *hello* something went wrong. Check
if you can remove the metadata from the files to fix that. Also make sure to
put *stereo* MP3 files on the SD-Card, mono files may play back at double speed.

If you want LittleSpeaker to announce your albums put a `album.mp3` in each
directory that should have a title. If that file is missing you will get an
announcer that tells you something like `Album thirtyfive` when selecting the
album on the menu.

If for some reason the album will play back in a wrong order you can put an
`album.m3u` or `album.m3u8` into the directory. The firmware will then use the
order specified in that playlist. Make sure the playlist only lists the filenames
without any directory traversal.
