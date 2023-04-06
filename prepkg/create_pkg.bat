@echo off

for /f "tokens=*" %%x in (.\version) do (
	set version=%%x
	goto :read_version_end_loop
)
:read_version_end_loop

set filename=yquake2_%version%

echo Creating %filename%.pkg it may take a while depending on .\pkg\ folder size...
.\tools\pkg-win64.exe --contentid UP0001-QUAKE2_00-0000000000000000 .\pkg\ %filename%.pkg
echo Done