#!/bin/bash

# sudo iptables -t nat -A OUTPUT -p tcp -d 192.168.2.1 --dport 445 -j REDIRECT --to-ports 1445

# perl -e "use IO::Socket::INET; IO::Socket::INET->new(PeerAddr => '192.168.2.1', PeerPort => 445, Proto => 'tcp', Timeout => 1) or die('connect');" || sudo iptables -t nat -A OUTPUT -p tcp -d 192.168.2.1 --dport 445 -j REDIRECT --to-ports 1445

todos() {
	unix2dos "$@"
}
fromdos() {
	dos2unix "$@"
}

set -ex

WINEXE=$(dirname "$0")/winexe/winexe

# test to execute, empty for all tests
TEST=moreresults2
TEST=${1:-raiserror}

# host configurations (Windows account to launch programs)
USER='FREETDS\Administrator'
PASS=password
HOST=192.168.2.1

HOST=192.168.122.76

# server configuration
SRV=sqlexpress
SRV=sql2008
SRV=docker
# freetds/mssql/ncli/ncli10
DRIVER=ncli10

export WINEPREFIX="$HOME/.local/share/wineprefixes/odbc"

# go to main distro dir
ORIGDIR=$(dirname "$0")
cd "$ORIGDIR"
ORIGDIR=$(/bin/pwd)

if false; then
	SERVER="(local)\\sqlexpress"
	SERVER="localhost\\sqlexpress"
	SERVER="localhost,1027"
else
	SERVER="10.0.2.2,1028"
	SERVER="(local)\\sql2008"
	SERVER="192.168.122.76,1032"
	SERVER="192.168.122.1,12345"
	SERVER="172.17.0.2,1433"
fi

# find test directory
if test -r "$ORIGDIR/src/odbc/.libs/libtdsodbc-0.dll"; then
	DIR="$ORIGDIR"
else
	DIR=$(find -maxdepth 1 -type d -name freetds-\* | sort | tail -1)
	if test ! -r "$DIR"; then
		echo "Win32 distribution not found" >&2
		exit 1
	fi
fi
rm -rf ftds
mkdir ftds

SERVER="\"Server\"=\"${SERVER//\\/\\\\}\"
\"ServerName\"=-"
case $DRIVER in
freetds)
	SERVER="\"ServerName\"=\"$SRV\"
\"Server\"=-"
	DRVNAME='FreeTDS'
	DRVDLL="C:\\ftds\\FreeTDS.dll"
	;;
mssql)
	DRVNAME='SQL Server'
	DRVDLL='C:\WINDOWS\system32\SQLSRV32.dll'
	;;
ncli)
	DRVNAME='SQL Native Client'
	DRVDLL='C:\WINDOWS\system32\SQLNCLI.dll'
	;;
ncli10)
	DRVNAME='ODBC Driver 18 for SQL Server'
	DRVDLL='C:\windows\system32\msodbcsql18.dll'
	;;
*)
	echo "wrong \"$DRIVER\" driver!" >&2
	exit 1
	;;
esac

cmd() {
	"$WINEXE" --reinstall -U "$USER%$PASS" "//$HOST" "$@"
}

cat > ftds/srv.reg <<EOF
REGEDIT4

[HKEY_LOCAL_MACHINE\\SOFTWARE\\ODBC\\ODBC.INI\\$SRV]
"Driver"="${DRVDLL//\\/\\\\}"
$SERVER
"Trusted_Connection"="No"
"Language"="English"
"TDS_Version"="7.0"

[HKEY_LOCAL_MACHINE\\SOFTWARE\\ODBC\\ODBC.INI\\ODBC Data Sources]
"$SRV"="$DRVNAME"
EOF
todos ftds/srv.reg
wine32 regedit ftds/srv.reg

# copy all files needed
if ! grep -q "^SRV=$SRV\$" $ORIGDIR/PWD; then
	echo "PWD file does not match server \"$SRV\"specified" >&2
	exit 1
fi
cp $ORIGDIR/PWD ftds/

exec 3> ftds/go.bat

cp "$DIR/src/odbc/.libs/libtdsodbc-0.dll" ftds/FreeTDS.dll
cp /usr/i686-w64-mingw32/sys-root/mingw/bin/{libwinpthread-1,libgcc_s_dw2-1,libstdc++-6,libssl-3,libcrypto-3,iconv,zlib1,libssp-0}.dll ftds/
(cd ftds && exec wine32 regsvr32 /s FreeTDS.dll)
cp misc/zip.exe ftds/

# prepare a script
echo "@echo off
set FREETDS=%CD%
set TDSPWDFILE=PWD
" >&3

cp "$HOME/.freetds.conf" ftds/freetds.conf
todos ftds/freetds.conf

# find which strip to call
STRIP=i586-mingw32msvc-strip
for i in i586-mingw32msvc-strip i686-w64-mingw32-strip; do
	if $i --version &> /dev/null; then
		STRIP=$i
	fi
done

rm -f test_*.txt
OUTPUT=
for F in $DIR/src/odbc/unittests/*.exe; do
	F=${F##*/}
	F=${F%%.exe}
	FN=$F

	if test "$TEST" = "" -o "$F" = "$TEST"; then
		# strip executable and compute right one
		if [ -r "$DIR/src/odbc/unittests/.libs/$F.exe" ]; then
			FN=".libs/$F"
		fi
		cp "$DIR/src/odbc/unittests/$FN.exe" "ftds/$F.exe"
		if [ -r "$DIR/src/odbc/unittests/$F.in" ]; then
			cp "$DIR/src/odbc/unittests/$F.in" ftds/ 2> /dev/null
		fi
		$STRIP "ftds/$F.exe"

		echo "echo executing test $F...
set TDSDUMP=test_${F}_log.txt
echo > test_${F}_log.txt
del test_${F}_log.txt
.\\$F.exe > test_$F.txt 2>&1
if not errorlevel 1 echo ok>> test_$F.txt
" >&3
	fi
done

OUT="$DRIVER-`date +%Y%m%d`"
echo "mkdir $OUT
move test_*.txt $OUT
copy $DRVDLL $OUT
copy C:\\WINDOWS\\WinSxS\\x86_Microsoft.VC80.CRT_1fc8b3b9a1e18e3b_8.0.50727.3053_x-ww_b80fa8ca\\MSVCR80.dll $OUT
zip -r $OUT.zip $OUT" >&3
exec 3>&-
todos ftds/go.bat

# launch!
cd ftds
wine32 cmd /c go.bat
cp $OUT.zip ..
cd ..

rm -rf $OUT
unzip $OUT.zip 
cd $OUT
mkdir ok bad
fromdos test_*.txt
for f in test_*.txt; do
	n=${f#test_}
	d=bad
	tail -n1 < $f | grep -q '^ok$' && d=ok
	mv $f $d/$n
done

echo Done

