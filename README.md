# WTF?

Quick and dirty PoC for console client that can search/download demos from quakeworld servers. Thin Client (CLI) taken from http://jogi.netdome.biz/, slightly modified to be more verbose. Check src/ directory if you want to diff with original.

## Requirements

* python modules: socket, sys, re, os, select, argparse, pexpect, platform, time
* OS: 32/64bit Windows/Linux (precompiled binaries of thin_client are inside lib/ directory)
* MVDSV/KTX servers (this software is not compatible with FTE)

## Installation

Well thin_client is already compiled (statically) and it should work on most Linux and Windows.
If you wish to make your own compilation, please follow these steps:
```
wget http://quake.tdhack.com/tools/dd.tar.gz
tar -zxf dd.tar.gz
cd demo_downloader/src
make clean
make d2
cp thin_client ../lib/
cd ..
```

## Example outputs:
### help
```
$ python dd.py -h
usage: dd.py [options]

dd.py by d2@tdhack.com is designed to perform simple search/download operations on quakeworld servers for demo files

optional arguments:
  -h, --help                       show this help message and exit
  -v, --version                    show program's version number and exit
  -s regex, --search regex         search for demos, looking for string, this could be simple ".*d2"
                                   or ".*(d2|grc).*
  -d a,b,c, --download a,b,c       download demo number(s)
  -q IP:PORT, --qwservers IP:PORT  file with addresses of quakeworld servers (can be downloaded
                                   from: http://www.quakeservers.net/lists/servers/servers.txt) or
                                   created from scratch in format: ip:port description -- see
                                   lib/servers.txt

  Examples:

  dd.py --search .*d2 --qwservers 94.23.252.225:27500        -- search for string (this could be POSIX compatible regex too) from 94.23.252.225 server
  dd.py --search .*d2 --qwservers file:lib/server.txt        -- search for string (this could be POSIX compatible regex too) serverlist taken from file
  dd.py --download 212,213 --qwservers 94.23.252.225:27500   -- download demo 212,213 from 94.23.252.225 server
```

### searching
```
$ python dd.py --search .*mushi.*dm2.* --qwservers file:qw_servers_names.txt
[*] searching ".*mushi.*dm2.*" ... please wait (result will appear on screen)
 109.74.195.224:27500  118: duel_mushi!_vs_(m@tr!}{)[dm2]030512-0022.mvd (1042k)
 109.74.195.224:27500  119: duel_mushi!_vs_(m@tr!}{)[dm2]030512-0038.mvd (992k)
  62.20.115.126:28001  404: duel_ake_vader_vs_mushi![dm2]251013-2354.mvd (1042k)
  62.20.115.126:28002  404: duel_ake_vader_vs_mushi![dm2]251013-2354.mvd (1042k)
[...]
```

### downloading (downloads are stored in qw/ folder)
```
$ python dd.py --download 213 --qwservers 94.23.252.225:27500
[*] downloading demo number: 213, from 94.23.252.225:27500, using thin_client.exe
        [+] spawning connection
        [+] waiting for "Connected" string
        [+] sending "cmd dl 213" command
        [+] number 213 is demofile: demos/duel_d2_vs_the_dead[dm2]290414-1903.mvd
        [+] waiting for download thread to finish... this will take a while
        [+] checking if demos/duel_d2_vs_the_dead[dm2]290414-1903.mvd file is OK
        [+] demo download OK
```

### 4. Bugs

- connection error handling for thin_client is not working
- error reading real demoname
- lack of detection for UDP errors, sometimes resulting in endless loop for python script

### Ideas? Reports?

Send them to: d2@tdhack.com
