@echo off
for %%X in (g++.exe) do (set FOUND=%%~$PATH:X)
if not defined FOUND goto error
echo Removing LSD intgrated gcc/g++ support.
echo Press CTRL+C to interrupt now or
pause
rd /q /s gnu\include\backward
rd /q /s gnu\include\bits
rd /q /s gnu\include\debug
rd /q /s gnu\include\ext
rd /q /s gnu\include\mingw32
rd /q /s gnu\include\next_include
rd /q /s gnu\include\sys
del gnu\include\accctrl.h
del gnu\include\aclapi.h
del gnu\include\algorithm
del gnu\include\ansidecl.h
del gnu\include\assert.h
del gnu\include\basetsd.h
del gnu\include\basetyps.h
del gnu\include\bfd.h
del gnu\include\bfdlink.h
del gnu\include\bitset
del gnu\include\cassert
del gnu\include\cctype
del gnu\include\cderr.h
del gnu\include\cerrno
del gnu\include\cfloat
del gnu\include\cguid.h
del gnu\include\ciso646
del gnu\include\climits
del gnu\include\clocale
del gnu\include\cmath
del gnu\include\comcat.h
del gnu\include\commctrl.h
del gnu\include\commdlg.h
del gnu\include\complex
del gnu\include\conio.h
del gnu\include\cpl.h
del gnu\include\cplext.h
del gnu\include\csetjmp
del gnu\include\csignal
del gnu\include\cstdarg
del gnu\include\cstddef
del gnu\include\cstdio
del gnu\include\cstdlib
del gnu\include\cstring
del gnu\include\ctime
del gnu\include\ctype.h
del gnu\include\custcntl.h
del gnu\include\cwchar
del gnu\include\cwctype
del gnu\include\cxxabi.h
del gnu\include\dbt.h
del gnu\include\dde.h
del gnu\include\ddeml.h
del gnu\include\deque
del gnu\include\dir.h
del gnu\include\direct.h
del gnu\include\dirent.h
del gnu\include\dis-asm.h
del gnu\include\dlgs.h
del gnu\include\docobj.h
del gnu\include\dos.h
del gnu\include\emmintrin.h
del gnu\include\envz.h
del gnu\include\errno.h
del gnu\include\exception
del gnu\include\exception_defines.h
del gnu\include\excpt.h
del gnu\include\exdisp.h
del gnu\include\exdispid.h
del gnu\include\fcntl.h
del gnu\include\fenv.h
del gnu\include\ffi.h
del gnu\include\float.h
del gnu\include\fstream
del gnu\include\functional
del gnu\include\g2c.h
del gnu\include\getopt.h
del gnu\include\gmon.h
del gnu\include\httpext.h
del gnu\include\idispids.h
del gnu\include\imagehlp.h
del gnu\include\imm.h
del gnu\include\index.html
del gnu\include\initguid.h
del gnu\include\intshcut.h
del gnu\include\inttypes.h
del gnu\include\io.h
del gnu\include\iomanip
del gnu\include\ios
del gnu\include\iosfwd
del gnu\include\iostream
del gnu\include\ipexport.h
del gnu\include\iphlpapi.h
del gnu\include\ipifcons.h
del gnu\include\iprtrmib.h
del gnu\include\iptypes.h
del gnu\include\isguids.h
del gnu\include\iso646.h
del gnu\include\istream
del gnu\include\iterator
del gnu\include\jni.h
del gnu\include\jvmpi.h
del gnu\include\largeint.h
del gnu\include\limits
del gnu\include\limits.h
del gnu\include\list
del gnu\include\lm.h
del gnu\include\lmaccess.h
del gnu\include\lmalert.h
del gnu\include\lmapibuf.h
del gnu\include\lmat.h
del gnu\include\lmaudit.h
del gnu\include\lmbrowsr.h
del gnu\include\lmchdev.h
del gnu\include\lmconfig.h
del gnu\include\lmcons.h
del gnu\include\lmerr.h
del gnu\include\lmerrlog.h
del gnu\include\lmmsg.h
del gnu\include\lmremutl.h
del gnu\include\lmrepl.h
del gnu\include\lmserver.h
del gnu\include\lmshare.h
del gnu\include\lmsname.h
del gnu\include\lmstats.h
del gnu\include\lmsvc.h
del gnu\include\lmuse.h
del gnu\include\lmuseflg.h
del gnu\include\lmwksta.h
del gnu\include\locale
del gnu\include\locale.h
del gnu\include\lzexpand.h
del gnu\include\malloc.h
del gnu\include\map
del gnu\include\mapi.h
del gnu\include\math.h
del gnu\include\mbctype.h
del gnu\include\mbstring.h
del gnu\include\mciavi.h
del gnu\include\mcx.h
del gnu\include\mem.h
del gnu\include\memory
del gnu\include\memory.h
del gnu\include\mgmtapi.h
del gnu\include\mmintrin.h
del gnu\include\mmsystem.h
del gnu\include\mshtml.h
del gnu\include\mswsock.h
del gnu\include\nb30.h
del gnu\include\nddeapi.h
del gnu\include\new
del gnu\include\nspapi.h
del gnu\include\ntdef.h
del gnu\include\ntdll.h
del gnu\include\ntldap.h
del gnu\include\ntsecapi.h
del gnu\include\ntsecpkg.h
del gnu\include\numeric
del gnu\include\oaidl.h
del gnu\include\objbase.h
del gnu\include\objfwd.h
del gnu\include\objidl.h
del gnu\include\ocidl.h
del gnu\include\odbcinst.h
del gnu\include\ole.h
del gnu\include\ole2.h
del gnu\include\ole2ver.h
del gnu\include\oleacc.h
del gnu\include\oleauto.h
del gnu\include\olectl.h
del gnu\include\olectlid.h
del gnu\include\oledlg.h
del gnu\include\oleidl.h
del gnu\include\ostream
del gnu\include\pbt.h
del gnu\include\pmmintrin.h
del gnu\include\poppack.h
del gnu\include\powrprof.h
del gnu\include\process.h
del gnu\include\profil.h
del gnu\include\profile.h
del gnu\include\prsht.h
del gnu\include\psapi.h
del gnu\include\pshpack1.h
del gnu\include\pshpack2.h
del gnu\include\pshpack4.h
del gnu\include\pshpack8.h
del gnu\include\queue
del gnu\include\rapi.h
del gnu\include\ras.h
del gnu\include\rasdlg.h
del gnu\include\raserror.h
del gnu\include\rassapi.h
del gnu\include\README
del gnu\include\regstr.h
del gnu\include\richedit.h
del gnu\include\richole.h
del gnu\include\rpc.h
del gnu\include\rpcdce.h
del gnu\include\rpcdce2.h
del gnu\include\rpcdcep.h
del gnu\include\rpcndr.h
del gnu\include\rpcnsi.h
del gnu\include\rpcnsip.h
del gnu\include\rpcnterr.h
del gnu\include\rpcproxy.h
del gnu\include\schannel.h
del gnu\include\schnlsp.h
del gnu\include\scrnsave.h
del gnu\include\search.h
del gnu\include\secext.h
del gnu\include\security.h
del gnu\include\servprov.h
del gnu\include\set
del gnu\include\setjmp.h
del gnu\include\setupapi.h
del gnu\include\share.h
del gnu\include\shellapi.h
del gnu\include\shlguid.h
del gnu\include\shlobj.h
del gnu\include\shlwapi.h
del gnu\include\signal.h
del gnu\include\snmp.h
del gnu\include\sql.h
del gnu\include\sqlext.h
del gnu\include\sqltypes.h
del gnu\include\sqlucode.h
del gnu\include\sspi.h
del gnu\include\sstream
del gnu\include\stack
del gnu\include\stdarg.h
del gnu\include\stdbool.h
del gnu\include\stddef.h
del gnu\include\stdexcept
del gnu\include\stdint.h
del gnu\include\stdio.h
del gnu\include\stdlib.h
del gnu\include\streambuf
del gnu\include\string
del gnu\include\string.h
del gnu\include\strings.h
del gnu\include\subauth.h
del gnu\include\svcguid.h
del gnu\include\symcat.h
del gnu\include\syslimits.h
del gnu\include\tchar.h
del gnu\include\time.h
del gnu\include\tlhelp32.h
del gnu\include\typeinfo
del gnu\include\unistd.h
del gnu\include\unknwn.h
del gnu\include\unwind.h
del gnu\include\userenv.h
del gnu\include\utility
del gnu\include\utime.h
del gnu\include\valarray
del gnu\include\values.h
del gnu\include\varargs.h
del gnu\include\vector
del gnu\include\vfw.h
del gnu\include\w32api.h
del gnu\include\wchar.h
del gnu\include\wctype.h
del gnu\include\winable.h
del gnu\include\winbase.h
del gnu\include\winber.h
del gnu\include\wincon.h
del gnu\include\wincrypt.h
del gnu\include\windef.h
del gnu\include\windows.h
del gnu\include\windowsx.h
del gnu\include\winerror.h
del gnu\include\wingdi.h
del gnu\include\wininet.h
del gnu\include\winioctl.h
del gnu\include\winldap.h
del gnu\include\winnetwk.h
del gnu\include\winnls.h
del gnu\include\winnt.h
del gnu\include\winperf.h
del gnu\include\winreg.h
del gnu\include\winresrc.h
del gnu\include\winsnmp.h
del gnu\include\winsock.h
del gnu\include\winsock2.h
del gnu\include\winspool.h
del gnu\include\winsvc.h
del gnu\include\winuser.h
del gnu\include\winver.h
del gnu\include\ws2spi.h
del gnu\include\ws2tcpip.h
del gnu\include\wsahelp.h
del gnu\include\wsipx.h
del gnu\include\wsnetbs.h
del gnu\include\wtypes.h
del gnu\include\xmmintrin.h
del gnu\include\zconf.h
del gnu\include\zlib.h
del gnu\include\zmouse.h
del gnu\include\_mingw.h
del gnu\lib\crt2.o
del gnu\lib\crtbegin.o
del gnu\lib\crtend.o
del gnu\lib\gcrt2.o
del gnu\lib\index.html
del gnu\lib\libadvapi32.a
del gnu\lib\libc.a
del gnu\lib\libcomdlg32.a
del gnu\lib\libgcc.a
del gnu\lib\libgcov.a
del gnu\lib\libgdi32.a
del gnu\lib\libgmon.a
del gnu\lib\libkernel32.a
del gnu\lib\libm.a
del gnu\lib\libmingw32.a
del gnu\lib\libmingwex.a
del gnu\lib\libmoldname.a
del gnu\lib\libmsvcrt.a
del gnu\lib\libshell32.a
del gnu\lib\libstdc++.a
del gnu\lib\libuser32.a
del gnu\lib\libz-bcc.lib
del gnu\lib\libz.a
del gnu\lib\libz.dll.a
del gnu\lib\libz.lib
del gnu\bin\as.exe
del gnu\bin\cc1plus.exe
del gnu\bin\crt2.o
del gnu\bin\crtbegin.o
del gnu\bin\crtend.o
del gnu\bin\cygz.dll
del gnu\bin\g++.exe
del gnu\bin\gcc.exe
del gnu\bin\gdb.exe
del gnu\bin\ld.exe
del gnu\bin\make.exe
del gnu\bin\msys-1.0.dll
echo Integrated gcc support removed. Reinstall LSD if required.
exit /b 0
:error
echo System gcc/g++ not found. 
echo Check that cygwin/MINGW are installed with gcc/g++ support.
echo Exiting with no changes...
exit /b 1
