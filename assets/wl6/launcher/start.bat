@ECHO OFF

:loop
  cd c:\launcher
  cycles max
  call launch.exe

  IF ERRORLEVEL 0 SET LAUNCH=0
  IF ERRORLEVEL 1 SET LAUNCH=1
  IF ERRORLEVEL 2 SET LAUNCH=2

  if %LAUNCH% == 0 GOTO done
  if %LAUNCH% == 1 GOTO launch1
  if %LAUNCH% == 2 GOTO launch2

:launch1
  cd ..
  cycles fixed 15000
  wolf3d.exe
  GOTO loop

:launch2
  cd ..\m1
  cycles fixed 15000
  spear.exe
  GOTO loop

:done
  exit
