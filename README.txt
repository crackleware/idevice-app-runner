*** Run app on iDevice using com.apple.debugserver ***

Requirements:

    * libimobiledevice - http://www.libimobiledevice.org/
    * iDevice(s) (maybe in developer mode)

Build:

    $ gcc -g -pthread idevice-app-runner.c -o idevice-app-runner /usr/lib/libimobiledevice.so

Usage:

    $ idevice-app-runner -r /private/var/mobile/Applications/........-....-....-....-............/...

I cooked up something mostly by tracing APIs and syscalls used in
Xcode and fruitscrap.

To get app path, for example:

    $ APPNAME=something APPPATH=`ideviceinstaller -l -o xml | egrep -A1 '<key>Path</key>|<key>CFBundleName</key>' | tr -d $'\n' | sed 's/--/\n/g' | egrep -A1 'CFBundleName.*'$APPNAME | tail -1 | tr '<>' '  ' | awk '{print $5}'`

References:
    https://github.com/ghughes/fruitstrap - very helpful

Contact:
    predrg@gmail.com

DTRUSS COMMANDS:

With tweak in fruitscrap.c:

#define GDB_SHELL "sudo dtruss /Developer/Platforms/iPhoneOS.platform/Developer/usr/libexec/gdb/gdb-arm-apple-darwin --arch armv7 -q -x " PREP_CMDS_PATH " 2> dtruss2.log"

For example:

$ egrep 'write\(0x5,|read\(0x5,' dtruss2.log

GDB COMMANDS:

$ gdb .../Xcode nnnnn

set print elements 10000

b mobdevlog
command
silent
printf "mobdevlog: %s\n", $rsi
cont
end

b USBMuxDebugLog
command
silent
printf "USBMuxDebugLog: %s %s\n", $rsi, $rdi
cont
end

b send
4
command
printf "fd=%ld, size=%ld\n", $rdi, $rdx
x/s $rsi
bt 3
cont
end

b SSL_write
command
p/d $rdx
x/s $rsi
cont
end

#b write
b *0x0000000100fd3268
command
printf "fd=%ld, size=%ld\n", $rdi, $rdx
x/s $rsi
bt 3
cont
end

#b recvfrom
b *0x10100494c
command
printf "fd=%ld, size=%ld\n", $rdi, $rdx
set variable $buf = $rsi
cont
end

#b <recvfrom+17>
b *0x10100494c+17
command
x/s $buf
cont
end

#b read
b *0x100fcb45c
command
printf "fd=%ld, size=%ld\n", $rdi, $rdx
set variable $buf2 = $rsi
cont
end

#b read+17
b *0x100fcb45c+17
command
x/s $buf2
cont
end


#b BIO_write
b *0x106aefaf8
command
printf "fd=%ld, size=%ld\n", $rdi, $rdx
x/s $rsi
x/100c $rsi
bt 3
cont
end

b DTDKStartSecureDebugServerService

