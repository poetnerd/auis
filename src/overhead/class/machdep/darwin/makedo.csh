#!/bin/csh -f
# makedo - create a dynamically loadable module (.do)
#
# Based on the Solaris (sun_sparc_51) makedo which uses dlopen().
# Creates a shared object linked against libclass.

if (! $?ANDREWDIR) setenv ANDREWDIR /usr/andrew
if ($#argv == 0) then
    echo "usage: makedo [-o outfile] [-b bindir] [-d libdir] [-e entrypoint] [-g] files..."
    echo "  -b overrides /usr/andrew/bin for finding doindex"
    echo "  -d overrides ANDREWDIR/lib for library path"
    echo "  -e overrides the default entry point (unused with dlopen)"
    echo "  -g keep debug symbols"
    exit 1
endif
set filelist
set bindir="${ANDREWDIR}/bin"
set libdir="${ANDREWDIR}/lib"
foreach file ($*)
    if ($?outcoming) then
        set outfile=$file
	unset outcoming
	continue
    endif
    if ($?bincoming) then
        set bindir=$file
	unset bincoming
	continue
    endif
    if ($?libcoming) then
	set libdir=$file
	unset libcoming
	continue
    endif
    if ($?entrypointcoming) then
        set entrypoint=$file
        unset entrypointcoming
        continue
    endif
    switch ($file)
    case -o:
	set outcoming
	breaksw
    case -b:
	set bincoming
	breaksw
    case -d:
	set libcoming
	breaksw
    case -e:
        set entrypointcoming
	breaksw
    case -g:
	set gflag
	breaksw
    default:
	if (! $?outfile) set outfile=$file
        set filelist=($filelist $file)
    endsw
end
if (! $?filelist) then
    echo "makedo: No object modules given."
    exit 1
endif

cc -dynamiclib -flat_namespace -undefined dynamic_lookup -o ${outfile:r}.do $filelist -L${libdir} -lclass
exit($status)
