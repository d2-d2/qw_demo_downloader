#!/usr/bin/env python

import socket, sys, re, os, select, argparse, pexpect, platform, time

# how many second should we wait for UDP connection
waitseconds = 2
# how many seconds we should wait for demo to download
DLTIMEOUT = 30

if re.search("cygwin", str(platform.uname()).lower()):
    if re.search("x86_64", str(platform.uname()).lower()):
        THINCLIENT='win/64/thin_client.exe'
    else:
        THINCLIENT='win/32/thin_client.exe'
    os.chmod('lib/'+THINCLIENT, 0755)
else:
    THINCLIENT='lin/32/thin_client'
    os.chmod('lib/'+THINCLIENT, 0755)

if len(sys.argv) < 2:
    print '[-] wrong number of arguments, see -h for help'
    sys.exit()

def f_parse():
    global parser, options
    epilog='''
  Examples:

  %(prog)s --search .*d2 --qwservers 94.23.252.225:27500        -- search for string (this could be POSIX compatible regex too) from 94.23.252.225 server
  %(prog)s --search .*d2 --qwservers file:lib/server.txt        -- search for string (this could be POSIX compatible regex too) serverlist taken from file
  %(prog)s --download 212,213 --qwservers 94.23.252.225:27500   -- download demo 212,213 from 94.23.252.225 server
    '''
    parser = argparse.ArgumentParser(
                    description='%(prog)s by d2@tdhack.com is designed to perform simple search/download operations on quakeworld servers for demo files',
                    usage='%(prog)s [options]',
                    epilog=epilog,
                    formatter_class=lambda prog: argparse.RawDescriptionHelpFormatter(prog,max_help_position=80, width=100),
                    )
    parser.add_argument('-v', '--version', action='version', version='%(prog)s v1.0 by d2@tdhack.com')
    parser.add_argument('-s', '--search',
                    dest='search',
                    metavar='regex',
                    required=False,
                    help='search for demos, looking for string, this could be simple ".*d2" or ".*(d2|grc).*')
    parser.add_argument('-d', '--download',
                    dest='download',
                    metavar='a,b,c',
                    required=False,
                    help='download demo number(s)')
    parser.add_argument('-q', '--qwservers',
                    dest='qwservers',
                    metavar='IP:PORT',
                    required=True,
                    help='file with addresses of quakeworld servers (can be downloaded from: http://www.quakeservers.net/lists/servers/servers.txt) or created from scratch in format: ip:port description -- see lib/servers.txt')
    options = parser.parse_args()
    return options

def f_verifyip(iplist):
    ''' this code is checking for valid IP:PORT pair, returns 1 in case of failure '''
    badip=0
    for host in iplist.split(','):
        if host.count('.') == 3:
            for ip in host.split('.'):
                if re.search(':', ip):
                    port=ip.split(':')[1]
                    ip=ip.split(':')[0]
                    if int(port) > 65535 or int(port) < 0:
                        print '\t[-] error: %s is not valid port for ip %s (out of range)!' % (port,host.split(':')[0])
                        badip=1
                if re.search('[A-Za-z]', ip):
                    print '\t[-] error: %s is not an IP (letters)!' % host
                    badip=1
                if int(ip) > 255 or int(ip) < 0:
                    print '\t[-] error: %s is not an IP (out of range)!' % host
                    badip=1
        else:
            print '\t[-] error: %s is not an IP!' % host
            badip=1
    return badip

f_parse()

if options.search:
    if 'file:' in options.qwservers:
        if not os.path.isfile(options.qwservers.split(':')[1]):
            print "[!] %s file does exist, please download them from: http://www.quakeservers.net/lists/servers/servers.txt or create one\n\nFormat:\n\t94.23.252.225:27500             quake.tdhack.com - duel" % qwlist
            sys.exit(1)
        f=open(options.qwservers.split(':')[1])
        fdata = f.readlines()
        f.close()
        with open(options.qwservers.split(':')[1]) as f:
            for i, l in enumerate(f):
                pass
        flines=i+1
    else:
        fdata=options.qwservers.split(',')
        flines=len(fdata)
    flinescur=1
    nlist=[]
    for hostport in fdata:
        if 'file:' in options.qwservers:
            hostport=hostport.split()
            qwhost=hostport[0].split(':')[0]
            qwport=int(hostport[0].split(':')[1])
            hostport.pop(0)
            qwserverdesc=' '.join(hostport)
        else:
            qwhost=hostport.split(':')[0]
            qwport=int(hostport.split(':')[1])
            qwserverdesc='N/A'

        if f_verifyip(qwhost+':'+str(qwport)) == 1:
            print '\t[-] remove it from list or cli'
            sys.exit(1)
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        except socket.error, msg :
            print '[-] Failed to create socket. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
            sys.exit()
        print('\r[%s/%s] searching "%s" ... please wait (result will appear on screen)') % (flinescur, flines, options.search),
        sys.stdout.flush()
        flinescur=flinescur+1
        server_address = (qwhost, qwport)
        prefix = chr(255)*4
        datapack=''
        try:
            udp_packet=prefix+'dlistr'
            sent = sock.sendto(bytes(udp_packet), server_address)
            while True:
                ready = select.select([sock], [], [], waitseconds)
                if ready[0]:
                    data, server = sock.recvfrom(4096)
                    if 'space available' in data:
                        datapack+=re.sub(prefix+'n', "", data)
                        break
                    else:
                        datapack+=re.sub(prefix+'n', "", data)
                else:
                    print '\n\t[-] uable to get demos from %s:%s server' % (qwhost,int(qwport))
                    sock.close()
                    break
        except socket.error, msg:
            print '[-] Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
        finally:
            sock.close()

        datapack=repr(datapack).split('\\n')
        for datapack_data in datapack:
            if re.match(options.search, datapack_data):
                append_data=[qwhost,qwport,datapack_data.split(':')[0].strip(),datapack_data.split(':')[1].strip()]
                nlist.append(append_data)
    sys.stdout.flush()
    print "\n"
    print '%15s %5s %11s %s' % ('host', 'port', 'demo_number', 'demo_file(size)')
    for item in xrange(len(nlist)):
        print '%15s %5s %11s %s' %(nlist[item][0], nlist[item][1], nlist[item][2], nlist[item][3])

if options.download:
    for demonum in options.download.split(','):
        if not demonum.isdigit():
            print '[-] demo number must be numeric, "%s" given' % options.download
            sys.exit(1)
        qwhost=options.qwservers.split(':')[0]
        qwport=options.qwservers.split(':')[1]
        if f_verifyip(qwhost+':'+qwport) == 1:
            print '\t[-] remove it from list or cli'
            sys.exit(1)
        crc=0
        while crc == 0:
            print '[*] downloading demo number: %s, from %s:%s, using %s' % (demonum, qwhost, qwport, THINCLIENT)
            print '\t[+] spawning connection'
            child = pexpect.spawn('lib/'+THINCLIENT+' +set cl_maxfps 30 +connect '+qwhost+':'+qwport+' +name demo_downloader +spectator 1', timeout=300)
            if os.path.isfile('/tmp/mylog'):
                os.remove('/tmp/mylog')
            child.logfile = open('/tmp/mylog', 'w')
            print '\t[+] waiting for "Connected" string'
            time.sleep(5)
            child.expect('Connected.', timeout=5)
            print '\t[+] sending "cmd dl %s" command' % (demonum)
            child.sendline('cmd dl '+demonum+'\n')
            time.sleep(5)
            child.expect(['File .*', pexpect.EOF, pexpect.TIMEOUT])
            with open('/tmp/mylog', 'r') as myfile:
                content = myfile.read()
            demofile = re.search(r'File .*.mvd', content, re.DOTALL).group().split()
            print '\t[+] number %s is demofile: %s' % (demonum, demofile[1])
            print '\t[+] waiting for download thread to finish... this will take a while'
            child.expect(['### download completed: ', pexpect.EOF, pexpect.TIMEOUT])
            print '\t[+] checking if %s file is OK' % demofile[1]
            s = file('qw/'+demofile[1], 'r').read()
            if s.find('EndOfDemo') >=0:
                print '\t[+] demo download OK'
                os.remove('/tmp/mylog')
                crc=1
            else:
                print '\t[-] failed downloading demo, try increasing timeout +30seconds'
