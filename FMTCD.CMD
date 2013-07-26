@echo off
rem
rem If FMT is run through this cmd file, the current directory at the
rem command line will be set to the current directory of FMT when it
rem ended.  In this way, FMT can be used as an enhanced CD command.
rem You can safely rename this .cmd file to whatever you like.  I'd
rem suggest CCD.cmd for this one.
rem
rem NOTE!!!!!!
rem You should replace the fully qualified paths to the occurrences
rem of FMT.EXE and $FMTCD.CMD below with those used on your system
rem (FMT writes $FMTCD.CMD into the same directory that holds FMT.EXE).
rem
rem
rem Start FMT with /$ argument, and any others from command line.
rem Note:  You could add /R here to start in Tree mode...
rem
f:\fmt\FMT.EXE /$ %1 %2 %3 %4 %5 %6 %7 %8 %9
rem
rem
rem Call the .cmd file built by FMT when it exited.
rem
call f:\fmt\$FMTCD.CMD
rem
rem
rem Clean up -- delete the .cmd file built by FMT when it exited.
rem
del f:\fmt\$FMTCD.CMD 1>NUL 2>NUL
rem
rem
rem That's all, folks.
rem
