@echo off
rem
rem Using this command file will cause FMT to act like a file viewer --
rem it will view whatever file you give it on the command line, and exit
rem the program when you exit the viewer.  You can use wildcards for the
rem filename, in which case you can use Ctrl+Page Down in the viewer to
rem toggle through the files that match the wildcard.
rem You can safely rename this .cmd file to whatever you like.  I'd suggest
rem READ.cmd for this one.
rem
rem Examples:  FMTRD read.me
rem            FMTRD F:\MYDOCS\*.TXT
rem
rem NOTE!!!!!
rem You should replace the drive and path to FMT.EXE below with what you're
rem using on your system.
rem
rem
rem Start FMT with the /V arg to put it in viewer mode:
rem
F:\FMT\FMT.EXE /V %1 %2 %3 %4 %5 %6 %7 %8 %9
rem
rem Outta here...
rem
