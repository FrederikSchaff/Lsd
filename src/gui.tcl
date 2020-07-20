#*************************************************************
#
#	LSD 7.3 - December 2020
#	written by Marco Valente, Universita' dell'Aquila
#	and by Marcelo Pereira, University of Campinas
#
#	Copyright Marco Valente and Marcelo Pereira
#	LSD is distributed under the GNU General Public License
#
#*************************************************************

#*************************************************************
# GUI.TCL
# LSD Graphical User Interface code and definitions.
#
# The module sets all the required variables, data structures
# and code required to create and operate the LSD GUI over
# Tk/ttk.
# To ensure smooth operation of LSD GUI, all Tk/ttk new 
# windows should be created using the procedures defined
# in WINDOW.TCL.
#*************************************************************

    set bRlf flat
    set ovBrlf groove 
	

package require Tk 8.6

#************************************************
# INITIAL CONFIGURATION
# The code below is run once when LMM/LSD starts
#************************************************

# load extra code
lappend auto_path "$RootLsd/$LsdSrc/themes"
source "$RootLsd/$LsdSrc/defaults.tcl" ;	# load LSD defaults
source "$RootLsd/$LsdSrc/window.tcl" ;		# load LSD gui management
source "$RootLsd/$LsdSrc/theme.tcl" ;		# load LSD gui theming
source "$RootLsd/$LsdSrc/dblclick.tcl" ;	# enhancements to double-click in text widgets

# optional development tools
set conWnd		true ;	# enable console window to be opened with CTRL+ALT+J
set logWndFn	false ;	# enable window functions operation logging
set testWnd		false ;	# enable coordinates test window

# detect current platform
if [ string equal $tcl_platform(platform) unix ] {
	if [ string equal $tcl_platform(os) Darwin ] {
		set CurPlatform mac
	} else {
		set CurPlatform linux
	}
} else {
	if { [ string equal $tcl_platform(platform) windows ] && [ string equal $tcl_platform(machine) amd64 ] } {
		set CurPlatform windows
	} else {
		set CurPlatform unsupported
	}
}

# register static special, OS-dependent configurations
if [ string equal $CurPlatform mac ] {
	set DefaultSysTerm $sysTermMac
	set systemTerm $sysTermMac
	set gnuplotTerm $gnuplotTermMac
	set DefaultExe $exeMac
	set DefaultMakeExe $makeMac
	set DefaultWish $wishMac
	set DefaultDbgExe $dbgMac
	set DefaultHtmlBrowser $browserMac
	set DefaultFont $fontMac
	set DefaultFontSize $fontSizeMac
	set deltaSize $deltaSizeMac
	set bvstepM $bvstepMwin
	set daCwid $daCwidMac
	set corrX $corrXmac
	set corrY $corrYmac

	if { [ string equal [ info patchlevel ] 8.6.9 ] } {
		set butWid $butMacTk869
		set bsizeM $bsizeMwin
		set bhstepM $bhstepMwin
		set borderMadj 0
	} else {
		set butWid $butMac
		set bsizeM $bsizeMlin
		set bhstepM $bhstepMmac
		set borderMadj $borderMmac
	}
	
} elseif [ string equal $CurPlatform linux ] {
	set DefaultSysTerm $sysTermLinux
	set systemTerm $sysTermLinux
	set gnuplotTerm $gnuplotTermLinux
	set DefaultExe $exeLinux
	set DefaultMakeExe $makeLinux
	set DefaultWish $wishLinux
	set DefaultDbgExe $dbgLinux
	set DefaultHtmlBrowser $browserLinux
	set DefaultFont $fontLinux
	set DefaultFontSize $fontSizeLinux
	set deltaSize $deltaSizeLinux
	set daCwid $daCwidLinux
	set corrX $corrXlinux
	set corrY $corrYlinux
	set butWid $butLinux
	set bsizeM $bsizeMlin
	set bhstepM $bhstepMlin
	set bvstepM $bvstepMlin
	set borderMadj 0
	
} elseif [ string equal $CurPlatform windows ] {
	set DefaultSysTerm $sysTermWindows
	set systemTerm $sysTermWindows
	set gnuplotTerm $gnuplotTermWindows
	set DefaultExe $exeWindows
	set DefaultWish $wishWindows
	set DefaultDbgExe $dbgWindows
	set DefaultHtmlBrowser $browserWindows
	set DefaultFont $fontWindows
	set DefaultFontSize $fontSizeWindows
	set deltaSize $deltaSizeWindows
	set daCwid $daCwidWindows
	set corrX $corrXwindows
	set corrY $corrYwindows
	set butWid $butWindows
	set bsizeM $bsizeMwin
	set bhstepM $bhstepMwin
	set bvstepM $bvstepMwin
	set borderMadj 0
	
	# Cygwin or MSYS2?
	if { [ catch { exec where cygwin1.dll } ] || [ catch { exec where cygintl-8.dll } ] } {
		set DefaultMakeExe $makeWinMingw
	} else {
		set DefaultMakeExe $makeWinCygwin
	}
}

# detect and update OS-dependent current/default theme configurations
updateTheme

# define dictionaries of themes available
foreach theme [ array names themeTable ] {
	set themePlatform [ lindex $themeTable($theme) 0 ]
	if { [ string equal $themePlatform $CurPlatform ] || [ string equal $themePlatform all ] } {
		set themeName [ lindex $themeTable($theme) 2 ]
		lappend themeNames $themeName
		dict set themeToName $theme $themeName
		dict set nameToTheme $themeName $theme
	}
}
set themeNames [ lsort $themeNames ]

# try to set tk theme (ttk), falling back to the set default
if { [ array names themeTable -exact $lsdTheme ] == "" || \
	 [ catch { package require [ lindex $themeTable($lsdTheme) 1 ] } ] ||
	 [ catch { ttk::style theme use $lsdTheme } ] } {
	if { [ array names themeTable -exact $DefaultTheme ] == "" || \
		 [ catch { package require [ lindex $themeTable($DefaultTheme) 1 ] } ] || \
		 [ catch { ttk::style theme use $DefaultTheme } ] } {
		set lsdTheme [ ttk::style theme use ]
	} else {
		set lsdTheme $DefaultTheme
	}
}

# define dark mode based on theme except on mac system-managed native theme
if { ! [ string equal $lsdTheme aqua ] } {
	set darkTheme [ lindex $themeTable($lsdTheme) 3 ]
} else {
	set darkTheme [ isDarkTheme ]
}

# get basic data from set theme
set colorsTheme(bg) [ ttk::style lookup . -background  ]
set colorsTheme(fg) [ ttk::style lookup . -foreground  ]
set colorsTheme(sbg) [ ttk::style lookup . -selectbackground ]
set colorsTheme(sfg) [ ttk::style lookup . -selectforeground ]
set colorsTheme(ebg) [ ttk::style lookup . -fieldbackground ]
set colorsTheme(efg) [ ttk::style lookup . -insertcolor  ]
set colorsTheme(dbg) [ ttk::style lookup TScale -troughcolor disabled ]
set colorsTheme(dfg) [ ttk::style lookup . -foreground disabled ]
set colorsTheme(isbg) [ ttk::style lookup . -lightcolor ]
set colorsTheme(hc) [ ttk::style lookup . -focuscolor ]	

# adjust between dark and light desktop modes
if { $darkTheme } {
	set colorsTheme(comm) $commcolorD
	set colorsTheme(str) $strcolorD
	set colorsTheme(prep) $prepcolorD
	set colorsTheme(type) $typecolorD
	set colorsTheme(kwrd) $kwrdcolorD
	set colorsTheme(vlsd) $vlsdcolorD
	set colorsTheme(mlsd) $mlsdcolorD
	set colorsTheme(var) $varcolorD
	set colorsTheme(lvar) $lvarcolorD
	set colorsTheme(fun) $funcolorD
	set colorsTheme(lfun) $lfuncolorD
	set colorsTheme(obj) $objcolorD
	set colorsTheme(grp) $grpcolorD
	set colorsTheme(mod) $modcolorD
} else {
	set colorsTheme(comm) $commcolorL
	set colorsTheme(str) $strcolorL
	set colorsTheme(prep) $prepcolorL
	set colorsTheme(type) $typecolorL
	set colorsTheme(kwrd) $kwrdcolorL
	set colorsTheme(vlsd) $vlsdcolorL
	set colorsTheme(mlsd) $mlsdcolorL
	set colorsTheme(var) $varcolorL
	set colorsTheme(lvar) $lvarcolorL
	set colorsTheme(fun) $funcolorL
	set colorsTheme(lfun) $lfuncolorL
	set colorsTheme(obj) $objcolorL
	set colorsTheme(grp) $grpcolorL
	set colorsTheme(mod) $modcolorL
}

# Variable 'alignMode' configure special, per module (LMM, LSD), settings
unset -nocomplain defaultPos defaultFocus
if [ info exists alignMode ] {
	if [ string equal -nocase $alignMode "LMM" ] {
		set defaultPos centerW
		set defaultFocus .f.t.t
	} else {
		set defaultPos centerW
	}
}

# lists to hold the windows parents stacks and exceptions to the parent mgmt.
set parWndLst [ list ]
set grabLst [ list ]
set noParLst [ list .log .str .plt .lat .tst ]

# list of windows with predefined sizes & positions
set wndLst [ list .lmm .lsd .log .str .da .deb .lat ]
set wndMenuHeight 0

# text line default canvas height & minimum horizontal border width
set lheightP [ expr int( [ font actual $fontP -size ] * [ tk scaling ] ) + $vtmarginP ]
set hbordsizeP	$hmbordsizeP

# current position of structure window
set posXstr 0
set posYstr 0

# load icon images
image create photo lsdImg -file "$RootLsd/$LsdSrc/icons/lsd.png"
image create photo lmmImg -file "$RootLsd/$LsdSrc/icons/lmm.png"
image create photo newImg -file "$RootLsd/$LsdSrc/icons/new.png"
image create photo openImg -file "$RootLsd/$LsdSrc/icons/open.png"
image create photo saveImg -file "$RootLsd/$LsdSrc/icons/save.png"
image create photo undoImg -file "$RootLsd/$LsdSrc/icons/undo.png"
image create photo redoImg -file "$RootLsd/$LsdSrc/icons/redo.png"
image create photo cutImg -file "$RootLsd/$LsdSrc/icons/cut.png"
image create photo deleteImg -file "$RootLsd/$LsdSrc/icons/delete.png"
image create photo copyImg -file "$RootLsd/$LsdSrc/icons/copy.png"
image create photo pasteImg -file "$RootLsd/$LsdSrc/icons/paste.png"
image create photo editImg -file "$RootLsd/$LsdSrc/icons/edit.png"
image create photo findImg -file "$RootLsd/$LsdSrc/icons/find.png"
image create photo replaceImg -file "$RootLsd/$LsdSrc/icons/replace.png"
image create photo indentImg -file "$RootLsd/$LsdSrc/icons/indent.png"
image create photo deindentImg -file "$RootLsd/$LsdSrc/icons/deindent.png"
image create photo wrapImg -file "$RootLsd/$LsdSrc/icons/wrap.png"
image create photo compileImg -file "$RootLsd/$LsdSrc/icons/compile.png"
image create photo comprunImg -file "$RootLsd/$LsdSrc/icons/comprun.png"
image create photo gdbImg -file "$RootLsd/$LsdSrc/icons/gdb.png"
image create photo infoImg -file "$RootLsd/$LsdSrc/icons/info.png"
image create photo descrImg -file "$RootLsd/$LsdSrc/icons/descr.png"
image create photo equationImg -file "$RootLsd/$LsdSrc/icons/equation.png"
image create photo extraImg -file "$RootLsd/$LsdSrc/icons/extra.png"
image create photo setImg -file "$RootLsd/$LsdSrc/icons/set.png"
image create photo hideImg -file "$RootLsd/$LsdSrc/icons/hide.png"
image create photo helpImg -file "$RootLsd/$LsdSrc/icons/help.png"
image create photo reloadImg -file "$RootLsd/$LsdSrc/icons/reload.png"
image create photo structImg -file "$RootLsd/$LsdSrc/icons/struct.png"
image create photo initImg -file "$RootLsd/$LsdSrc/icons/init.png"
image create photo numberImg -file "$RootLsd/$LsdSrc/icons/number.png"
image create photo runImg -file "$RootLsd/$LsdSrc/icons/run.png"
image create photo dataImg -file "$RootLsd/$LsdSrc/icons/data.png"
image create photo resultImg -file "$RootLsd/$LsdSrc/icons/result.png"
image create photo errorDlgImg -file "$RootLsd/$LsdSrc/icons/error.png"
image create photo infoDlgImg -file "$RootLsd/$LsdSrc/icons/information.png"
image create photo questDlgImg -file "$RootLsd/$LsdSrc/icons/question.png"
image create photo warnDlgImg -file "$RootLsd/$LsdSrc/icons/warning.png"

# load and set console configuration
if $conWnd {
	set msg "File(s) missing or corrupted"
	set det "Tcl/Tk console file 'tkcon.tcl' is missing or corrupted.\nPlease check your installation and reinstall LSD if the problem persists.\n\nLSD is continuing without console support."
	if [ file exists "$RootLsd/$LsdSrc/tkcon.tcl" ] {
		if { [ catch { source "$RootLsd/$LsdSrc/tkcon.tcl" } ] == 0 } {
			set tkcon::PRIV(showOnStartup) 0
			set tkcon::PRIV(root) .console
			set tkcon::PRIV(protocol) { tkcon hide }
			set tkcon::OPT(exec) ""
		} else {
			set conWnd false
			ttk::messageBox -type ok -icon warning -title Warning -message $msg -detail $det
		}
	} else {
		set conWnd false
			ttk::messageBox -type ok -icon warning -title Warning -message $msg -detail $det
	}
}

# open test window if enabled
if $testWnd {
	newtop .tst "LSD Coordinates Test Window" { destroytop .tst } ""

	frame .tst.xy
	label .tst.xy.l1 -anchor e -text "X:"
	label .tst.xy.v1 -anchor w -fg red
	label .tst.xy.l2 -anchor e -text "   Y:"
	label .tst.xy.v2 -anchor w -fg red
	pack .tst.xy.l1 .tst.xy.v1 .tst.xy.l2 .tst.xy.v2 -side left -padx 2 -pady 2

	frame .tst.r
	label .tst.r.l1 -anchor e -text "rootx:"
	label .tst.r.v1 -anchor w -fg red
	label .tst.r.l2 -anchor e -text "   rooty:"
	label .tst.r.v2 -anchor w -fg red
	pack .tst.r.l1 .tst.r.v1 .tst.r.l2 .tst.r.v2 -side left -padx 2 -pady 2

	frame .tst.v
	label .tst.v.l1 -anchor e -text "vrootx:"
	label .tst.v.v1 -anchor w -fg red
	label .tst.v.l2 -anchor e -text "   vrooty:"
	label .tst.v.v2 -anchor w -fg red
	pack .tst.v.l1 .tst.v.v1 .tst.v.l2 .tst.v.v2 -side left -padx 2 -pady 2

	frame .tst.s
	label .tst.s.l1 -anchor e -text "screenwidth:"
	label .tst.s.v1 -anchor w -fg red
	label .tst.s.l2 -anchor e -text "   screenheight:"
	label .tst.s.v2 -anchor w -fg red
	pack .tst.s.l1 .tst.s.v1 .tst.s.l2 .tst.s.v2 -side left -padx 2 -pady 2

	frame .tst.t
	label .tst.t.l1 -anchor e -text "vrootwidth:"
	label .tst.t.v1 -anchor w -fg red
	label .tst.t.l2 -anchor e -text "   vrootheight:"
	label .tst.t.v2 -anchor w -fg red
	pack .tst.t.l1 .tst.t.v1 .tst.t.l2 .tst.t.v2 -side left -padx 2 -pady 2

	frame .tst.m
	label .tst.m.l1 -anchor e -text "maxwidth:"
	label .tst.m.v1 -anchor w -fg red
	label .tst.m.l2 -anchor e -text "   maxheight:"
	label .tst.m.v2 -anchor w -fg red
	pack .tst.m.l1 .tst.m.v1 .tst.m.l2 .tst.m.v2 -side left -padx 2 -pady 2

	pack .tst.xy .tst.r .tst.v .tst.s .tst.t .tst.m

	bind .tst <Motion> {
		.tst.xy.v1 configure -text %X
		.tst.xy.v2 configure -text %Y
		.tst.r.v1 configure -text [ winfo rootx .tst ]
		.tst.r.v2 configure -text [ winfo rooty .tst ]
		.tst.v.v1 configure -text [ winfo vrootx .tst ]
		.tst.v.v2 configure -text [ winfo vrooty .tst ]
		.tst.s.v1 configure -text [ winfo screenwidth .tst ]
		.tst.s.v2 configure -text [ winfo screenheight .tst ]
		.tst.t.v1 configure -text [ winfo vrootwidth .tst ]
		.tst.t.v2 configure -text [ winfo vrootheight .tst ]
		.tst.m.v1 configure -text [ lindex [ wm maxsize .tst ] 0 ]
		.tst.m.v2 configure -text [ lindex [ wm maxsize .tst ] 1 ]
	}

	showtop .tst current yes yes no
}


#		ttk::messageBox -message "lsdTheme:$lsdTheme x [ ttk::style theme use ]\n\nThemes:\n[ ttk::style theme names ]\n\nthemeList:\n[ array names themeTable ]\n\npackages:\n[ package names ]\n\nColors:\n[ array get colorsTheme ]\n\nStyles:\n[ ttk::style element names ]"


