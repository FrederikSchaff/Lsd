/*************************************************************

	LSD 7.1 - May 2018
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente
	LSD is distributed under the GNU General Public License
	
 *************************************************************/

/*****
used up to 87 options
*******/

/*****************************************************

This program is a front end for dealing with LSD models code (running, compiling, editing, 
debugging LSD model programs). See the manual for help on its use.

IMPORTANT: this is _NOT_ a LSD model, but the best I could produce of something similar to 
a programming environment for LSD model programs.


This file can be compiled with the command line:

make -f <makefile>

There are several makefiles in LSD root directory appropriate to different environments 
(Windows, Mac & Linux) and configurations (32 or 64-bit).

LMM starts in a quite weird way. If there is no parameter in the call used to start it, the only 
operation it does is to ... run a copy of itself followed by the parameter "kickstart". This trick 
is required because under Windows there are troubles launching external package from a "first 
instance" of a program.

LMM reads all the directories that are not: Manual, gnu, gnu64, LMM.app, R and src as model 
directories, where it expect to find certain files. At any given moment a model name is stored, 
together with its directory and the file shown. 

Any internal command is executed in a condition like this:

if ( choice == x)
 do_this_and_that

and returned to the main cycle. After each block the flow returns to "loop" where the main 
Tcl_DoOneEvent loop sits.

The widget of importance are:
- .f.t.t is the main text editor
- .f.m is the frame containing the upper buttons, models list and help window

*****************************************************/

#include <tk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <new>

// LSD version strings, for About... boxes and code testing
#define _LSD_MAJOR_ 7
#define _LSD_MINOR_ 1
#define _LSD_VERSION_ "7.1"
#define _LSD_DATE_ __DATE__

// general buffer limits
#define TCL_BUFF_STR 3000		// standard Tcl buffer size (>1000)
#define MAX_PATH_LENGTH 500		// maximum path length
#define MAX_LINE_SIZE 1000		// max size of a text line to read from files (>999)

// user defined signals
#define SIGMEM NSIG + 1			// out of memory signal
#define SIGSTL NSIG + 2			// standard library exception signal

// Eigen library include command
#define EIGEN "#define EIGENLIB"

using namespace std;

// auxiliary C procedures
bool compile_run( bool run, bool nw = false );
bool discard_change( void );	// ask before discarding unsaved changes
bool use_eigen( void );			// check is Eigen library is in use
char *get_fun_name( char *str, bool nw = false );
int ModManMain( int argn, char **argv );
int Tcl_discard_change( ClientData, Tcl_Interp *, int, const char *[ ] );// Tcl entry point
void check_option_files( bool sys = false );
void cmd( const char *cm, ... );
void color( int hiLev, long iniLin, long finLin );
void create_compresult_window( bool nw = false );
void handle_signals( void );
void log_tcl_error( const char *cm, const char *message );
void make_makefile( bool nw = false );
void signal_handler( int signum );

// global variables
Tcl_Interp *inter = NULL;
bool tk_ok = false;				// control for tk_ready to operate
char *exec_path = NULL;			// path of executable file
char msg[ TCL_BUFF_STR ]; 
char rootLsd[ MAX_PATH_LENGTH ];
int choice;
int shigh;						// syntax highlighting state (0, 1 or 2)
int tosave = false;				// modified file flag
int v_counter = 0; 				//counter of the v[ i ] variables inserted in the equations


/*************************************
 MAIN
 *************************************/
int main( int argn, char **argv )
{
	int res;
	
	// register all signal handlers
	handle_signals( );

	try
	{
		res = ModManMain( argn, argv );
	}
	catch ( bad_alloc&  )		// out of memory conditions
	{
		signal_handler( SIGMEM );
	}
	catch ( exception& exc )	// other known error conditions
	{
		sprintf( msg, "\nSTL exception of type: %s\n", exc.what( ) );
		signal_handler( SIGSTL );
	}
	catch ( ... )				// other unknown error conditions
	{
		abort( );				// raises a SIGABRT exception, tell user & close
	}
	
	if ( inter != NULL )
	{
		cmd( "if { ! [ catch { package present Tk } ] } { destroy . }" );
		Tcl_Finalize( );
	}
	return res;
}


/*************************************
 MODMANMAIN
 *************************************/
int ModManMain( int argn, char **argv )
{
int i, num, sourcefile, macro = 1;
char *s, str[ MAX_LINE_SIZE + 2 * MAX_PATH_LENGTH ], str1[ 2 * MAX_PATH_LENGTH ], str2[ 2 * MAX_PATH_LENGTH ];
FILE *f;

// initialize the tcl interpreter
Tcl_FindExecutable( argv[0] );
inter = Tcl_CreateInterp( );
num = Tcl_Init( inter );
if ( num != TCL_OK )
{
	sprintf( msg, "Tcl initialization directories not found, check the Tcl/Tk installation  and configuration or reinstall LSD\nTcl Error = %d : %s", num,  Tcl_GetStringResult( inter ) );
	log_tcl_error( "Create Tcl interpreter", msg );
	return 1;
}

// set variables and links in TCL interpreter
Tcl_SetVar( inter, "_LSD_VERSION_", _LSD_VERSION_, 0 );
Tcl_SetVar( inter, "_LSD_DATE_", _LSD_DATE_, 0 );
Tcl_LinkVar( inter, "choice", ( char * ) &choice, TCL_LINK_INT );

// test Tcl interpreter
cmd( "set choice 1234567890" );
Tcl_UpdateLinkedVar( inter, "choice" );
if ( choice != 1234567890 )
{
	log_tcl_error( "Test Tcl", "Tcl failed, check the Tcl/Tk installation and configuration or reinstall LSD" );
	return 2;
}
	
// initialize & test the tk application
choice = 1;
num = Tk_Init( inter );
if ( num == TCL_OK )
	cmd( "if { ! [ catch { package present Tk 8.5 } ] && [ winfo exists . ] } { set choice 0 } { set choice 1 }" );
if ( choice )
{
	sprintf( msg, "Tk failed, check the Tcl/Tk installation (version 8.5+) and configuration or reinstall LSD\nTcl Error = %d : %s", num,  Tcl_GetStringResult( inter ) );
	log_tcl_error( "Start Tk", msg );
	return 3;
}
tk_ok = true;
cmd( "tk appname lmm" );

// close console if open (usually only in Mac)
cmd( "if [ string equal $tcl_platform(os) Darwin ] { foreach i [ winfo interps ] { if { ! [ string equal [ string range $i 0 2 ] lmm ] && ! [ string equal [ string range $i 0 2 ] lsd ] } { send $i \"wm iconify .; wm withdraw .; destroy .\" } } }" );

// check installation directory for no spaces in name
cmd( "if { [ string first \" \" \"[ pwd ]\" ] >= 0  } { set choice 1 } { set choice 0 }" );
if ( choice )
{
	log_tcl_error( "Path check", "LSD directory path includes spaces, move all the LSD directory in another directory without spaces in the path" );
	cmd( "tk_messageBox -icon error -title Error -type ok -message \"Installation error\" -detail \"The LSD directory is: '[ pwd ]'\n\nIt includes spaces, which makes impossible to compile and run LSD models.\nThe LSD directory must be located where there are no spaces in the full path name.\nMove all the LSD directory in another directory. If exists, delete the 'system_options.txt' file from the \\src directory.\n\nLSD is aborting now.\"" );
	return 4;
}

// try to open text file if name is provided in the command line
if ( argn > 1 )
{
	for ( i = 0; argv[ 1 ][ i ] != '\0'; ++i )
	{
		if ( argv[ 1 ][ i ] == '\\' )
			msg[ i ] = '/';
		else
			msg[ i ] = argv[ 1 ][ i ];
	}
	msg[ i ] = '\0';
	cmd( "set filetoload \"%s\"", msg );
	cmd( "if { ! [ file pathtype \"$filetoload\" ] == \"absolute\" } { set filetoload \"[pwd]/$filetoload\" }" );
}

// prepare to use exec path to find LSD directory
cmd( "if { [ info nameofexecutable ] != \"\" } { set path [ file dirname [ info nameofexecutable ] ] } { set path \"[ pwd ]\" }" );
s = ( char * ) Tcl_GetVar( inter, "path", 0 );
if ( s != NULL && strlen( s ) > 0 )
{
	exec_path = new char[ strlen( s ) + 1 ];
	strcpy( exec_path, s );
}

// check if LSDROOT environment variable exists and use it if so
cmd( "if [ info exists env(LSDROOT) ] { set RootLsd [ file normalize $env(LSDROOT) ]; if [ file exists \"$RootLsd/src/decl.h\" ] { cd \"$RootLsd\"; set choice 0 } { set choice 1 } } { set choice 1 }" );

if ( choice )
{
	choice = 0;
	cmd( "set RootLsd [ file normalize \"%s\" ]", exec_path );
	// check if directory is ok and if executable is inside a macOS package
	cmd( "if [ file exists \"$RootLsd/src/decl.h\" ] { \
			cd \"$RootLsd\" \
		} { \
			if [ file exists \"$RootLsd/../../../src/decl.h\" ] { \
				cd \"$RootLsd/../../..\"; \
				set RootLsd \"[ pwd ]\" \
			} { \
				unset RootLsd; \
				set choice 1 \
			} \
		}" );
	if ( choice )
	{
		log_tcl_error( "Source files check", "Required LSD source file(s) missing or corrupted, check the installation of LSD and reinstall LSD if the problem persists" );
		cmd( "tk_messageBox -type ok -icon error -title Error -message \"File(s) missing or corrupted\" -detail \"Some critical LSD files or folders are missing or corrupted.\nPlease check your installation and reinstall LSD if the problem persists.\n\nLSD is aborting now.\"" );
		return 5;	
	}
	cmd( "set env(LSDROOT) $RootLsd" );
}

s =  ( char * ) Tcl_GetVar( inter, "RootLsd", 0 );
strncpy( rootLsd, s, 499 );
num = strlen( rootLsd );
for ( i = 0; i < num; ++i )
	if ( rootLsd[ i ] == '\\' )
		rootLsd[ i ] = '/';
cmd( "set RootLsd \"%s\"", rootLsd );

// set platform-specific variables
cmd( "if [ string equal $tcl_platform(platform) unix ] { set DefaultExe lsd_gnu; set DefaultMakeExe make; set DefaultWish wish; set DefaultDbgTerm xterm; set DefaultDbgExe gdb; set DefaultHtmlBrowser firefox; set DefaultFont Courier; set DefaultFontSize 12 }" );
#ifdef MAC_PKG
cmd( "if [ string equal $tcl_platform(os) Darwin ] { set DefaultExe LSD; set DefaultMakeExe make; set DefaultWish wish8.6; set DefaultDbgTerm Terminal; set DefaultDbgExe lldb; set DefaultHtmlBrowser open; set DefaultFont Monaco; set DefaultFontSize 14 }" );
#else
cmd( "if [ string equal $tcl_platform(os) Darwin ] { set DefaultExe lsd_gnu; set DefaultMakeExe make; set DefaultWish wish8.5; set DefaultDbgTerm Terminal; set DefaultDbgExe lldb; set DefaultHtmlBrowser open; set DefaultFont Monaco; set DefaultFontSize 14 }" );
#endif
cmd( "if { [ string equal $tcl_platform(platform) windows ] && [ string equal $tcl_platform(machine) intel ] } { set DefaultExe lsd_gnu; set DefaultMakeExe \"make.exe\"; set DefaultWish wish85.exe; set DefaultDbgTerm cmd; set DefaultDbgExe gdb; set DefaultHtmlBrowser open; set DefaultFont Consolas; set DefaultFontSize 11; set LsdGnu gnu }" );
cmd( "if { [ string equal $tcl_platform(platform) windows ] && [ string equal $tcl_platform(machine) amd64 ] } { set DefaultExe lsd_gnu; set DefaultWish wish86.exe; set DefaultDbgTerm cmd; set DefaultDbgExe gdb; set DefaultHtmlBrowser open; set DefaultFont Consolas; set DefaultFontSize 11; set LsdGnu gnu64; if { [ catch { exec where cygwin1.dll } ] || [ catch { exec where cygintl-8.dll } ] } { set DefaultMakeExe \"gnumake.exe\" } { set DefaultMakeExe \"make.exe\" } }" );

cmd( "set MakeExe \"$DefaultMakeExe\"" );
cmd( "set small_character [ expr $DefaultFontSize - 2 ]" );

cmd( "set choice [ file exist \"$RootLsd/lmm_options.txt\" ]" );
if ( choice )
{
	cmd( "set f [open \"$RootLsd/lmm_options.txt\" r]" );
	cmd( "gets $f DbgTerm" );
	cmd( "gets $f HtmlBrowser" );
	cmd( "gets $f fonttype" );
	cmd( "gets $f wish" );
	cmd( "gets $f LsdSrc" );
	cmd( "gets $f dim_character" );
	cmd( "gets $f tabsize" );
	cmd( "gets $f wrap" );
	cmd( "gets $f shigh" );
	cmd( "gets $f autoHide" );
	cmd( "gets $f showFileCmds" );
	cmd( "gets $f LsdNew" );
	cmd( "gets $f DbgExe" );
	cmd( "close $f" );
	// handle old options file
	cmd( "if { $DbgTerm == \"\" || $HtmlBrowser == \"\" || $fonttype == \"\" || $wish == \"\" || $LsdSrc == \"\" || $dim_character == \"\" || $tabsize == \"\" || $wrap == \"\" || $shigh == \"\" || $autoHide == \"\" || $showFileCmds == \"\" || $LsdNew == \"\" || $DbgExe == \"\" } { set choice 0 }" );
}
else
{
	cmd( "set DbgTerm \"\"" );
	cmd( "set HtmlBrowser \"\"" );
	cmd( "set fonttype \"\"" );
	cmd( "set wish \"\"" );
	cmd( "set LsdSrc \"\"" );
	cmd( "set dim_character \"\"" );
	cmd( "set tabsize \"\"" );
	cmd( "set wrap \"\"" );
	cmd( "set shigh \"\"" );
	cmd( "set autoHide \"\"" );
	cmd( "set showFileCmds \"\"" );
	cmd( "set LsdNew \"\"" );
	cmd( "set DbgExe \"\"" );
}

// handle non-existent or old options file for new options
if ( ! choice )
 {
	// set new parameters
	cmd( "if { $DbgTerm == \"\" } { set DbgTerm \"$DefaultDbgTerm\" }" );// default debugger terminal
	cmd( "if { $HtmlBrowser == \"\" } { set HtmlBrowser \"$DefaultHtmlBrowser\" }" );// default browser
	cmd( "if { $wish == \"\" } { set wish \"$DefaultWish\" }" );// default tcl interpreter
	cmd( "if { $LsdSrc == \"\" } { set LsdSrc src }" );	// source path
	cmd( "if { $fonttype == \"\" } { set fonttype $DefaultFont }" );
	cmd( "if { $dim_character == \"\" } { set dim_character $DefaultFontSize }" );
	cmd( "if { $tabsize == \"\" } { set tabsize 2 }" );	// default tab size
	cmd( "if { $wrap == \"\" } { set wrap 1 }" );		// default text wrapping mode (1=yes)
	cmd( "if { $shigh == \"\" } { set shigh 2 }" );		// default is full syntax highlighting
	cmd( "if { $autoHide == \"\" } { set autoHide 0 }" );// default is to not auto hide LMM on run
	cmd( "if { $showFileCmds == \"\" } { set showFileCmds 0 }" );// default is no text file commands in File menu
	cmd( "if { $LsdNew == \"\" } { set LsdNew Work }" );// default new model subdirectory is "Work"
	cmd( "if { $DbgExe == \"\" } { set DbgExe \"$DefaultDbgExe\" }" );// default debugger
	
	// save to config file
	cmd( "set f [open \"$RootLsd/lmm_options.txt\" w]" );
	cmd( "puts $f \"$DbgTerm\"" );
	cmd( "puts $f \"$HtmlBrowser\"" );
	cmd( "puts $f \"$fonttype\"" );
	cmd( "puts $f $wish" );  
	cmd( "puts $f $LsdSrc" );
	cmd( "puts $f $dim_character" );
	cmd( "puts $f $tabsize" );
	cmd( "puts $f $wrap" );
	cmd( "puts $f $shigh" );
	cmd( "puts $f $autoHide" );
	cmd( "puts $f $showFileCmds" );
	cmd( "puts $f $LsdNew" );
	cmd( "puts $f \"$DbgExe\"" );
	cmd( "close $f" );
 }
 
check_option_files( true );

cmd( "set choice 0" );
cmd( "set recolor \"\"" );
cmd( "set docase 0" );
cmd( "set dirsearch \"-forwards\"" );
cmd( "set lfind [ list ]");
cmd( "set lfindsize 0" );
cmd( "set endsearch end" );
cmd( "set currentpos \"\"" );
cmd( "set currentdoc \"\"" );
cmd( "set v_num 0" );
cmd( "set shigh_temp $shigh" );
cmd( "set alignMode \"LMM\"" );

// load required Tcl/Tk data, procedures and packages
cmd( "if [ file exists \"$RootLsd/$LsdSrc/showmodel.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/showmodel.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
cmd( "if [ file exists \"$RootLsd/$LsdSrc/lst_mdl.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/lst_mdl.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
cmd( "if [ file exists \"$RootLsd/$LsdSrc/defaults.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/defaults.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
cmd( "if [ file exists \"$RootLsd/$LsdSrc/window.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/window.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
cmd( "if [ file exists \"$RootLsd/$LsdSrc/ls2html.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/ls2html.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
cmd( "if [ file exists \"$RootLsd/$LsdSrc/dblclick.tcl\" ] { if { [ catch { source \"$RootLsd/$LsdSrc/dblclick.tcl\" } ] != 0 } { set choice [ expr $choice + 1 ] } } { set choice [ expr $choice + 2 ] }" );
if ( choice != 0 )
{
	log_tcl_error( "Source files check", "Required Tcl/Tk source file(s) missing or corrupted, check the installation of LSD and reinstall LSD if the problem persists" );
	cmd( "tk_messageBox -type ok -icon error -title Error -message \"File(s) missing or corrupted\" -detail \"Some critical Tcl files ($choice) are missing or corrupted.\nPlease check your installation and reinstall LSD if the problem persists.\n\nLSD is aborting now.\"" );
	return 10 + choice;
}

// create a Tcl command that calls the C discard_change function before killing LMM
Tcl_CreateCommand( inter, "discard_change", Tcl_discard_change, NULL, NULL );

Tcl_LinkVar( inter, "num", ( char * ) &num, TCL_LINK_INT );
Tcl_LinkVar( inter, "tosave", ( char * ) &tosave, TCL_LINK_BOOLEAN);
Tcl_LinkVar( inter, "macro", ( char * ) &macro, TCL_LINK_BOOLEAN);
Tcl_LinkVar( inter, "shigh", ( char * ) &shigh, TCL_LINK_INT );
cmd( "set shigh $shigh_temp" );	// restore correct value

// set main window
cmd( "wm withdraw ." );
cmd( "wm title . \"LSD Model Manager - LMM\"" );
cmd( "wm protocol . WM_DELETE_WINDOW { set choice 1 }" );
cmd( ". configure -menu .m" );		// define here to avoid redimensining the window
cmd( "bind . <Destroy> { set choice 1 }" );
cmd( "icontop . lmm" );
cmd( "sizetop .lmm" );
cmd( "setglobkeys ." );				// set global keys for main window

// main menu
cmd( "menu .m -tearoff 0" );

cmd( "set w .m.file" );
cmd( "menu $w -tearoff 0" );
cmd( ".m add cascade -label File -menu $w -underline 0" );
cmd( "$w add command -label \"New Model/Group...\" -underline 0 -command { set choice 14}" );	// entryconfig 0
cmd( "$w add command -label \"Browse Models...\" -underline 0 -command {set choice 33} -accelerator Ctrl+b" );	// entryconfig 1
cmd( "$w add command -label \"Save Model\" -underline 0 -state disabled -command { if {[string length \"$filename\"] > 0} {if { [file exist \"$dirname/$filename\"] == 1} {catch {file copy -force \"$dirname/$filename\" \"$dirname/[file rootname \"$filename\"].bak\"}}; set f [open \"$dirname/$filename\" w];puts -nonewline $f [.f.t.t get 0.0 end]; close $f; set before [.f.t.t get 0.0 end]; set choice 999} } -underline 0 -accelerator Ctrl+s" );	// entryconfig 2
cmd( "$w add command -label \"Save Model As...\" -state disabled -underline 0 -command { set choice 41} -underline 11 -accelerator Ctrl+a" );	// entryconfig 3
cmd( "$w add separator" );	// entryconfig 4
cmd( "$w add command -label \"Compare Models...\" -underline 3 -command {set choice 61} -underline 0" );
cmd( "$w add command -label \"TkDiff...\" -command {set choice 57} -underline 0" );
cmd( "$w add separator" );

cmd( "if { $showFileCmds == 1 } { $w add command -label \"New Text File\" -command { set choice 39} -underline 4 }" );
cmd( "if { $showFileCmds == 1 } { $w add command -label \"Open Text File...\" -command { set choice 15} -underline 0 -accelerator Ctrl+o }" );
cmd( "if { $showFileCmds == 1 } { $w add command -label \"Save Text File\" -command { if {[string length \"$filename\"] > 0} {if { [file exist \"$dirname/$filename\"] == 1} {catch {file copy -force \"$dirname/$filename\" \"$dirname/[file rootname \"$filename\"].bak\"}} {}; set f [open \"$dirname/$filename\" w];puts -nonewline $f [.f.t.t get 0.0 end]; close $f; set before [.f.t.t get 0.0 end]; set choice 999} {}} -underline 2 }" );
cmd( "if { $showFileCmds == 1 } { $w add command -label \"Save Text File As...\" -command {set choice 4} -underline 3 }" );
cmd( "if { $showFileCmds == 1 } { $w add separator }" );

cmd( "$w add command -label \"Options...\" -command { set choice 60 } -underline 1" );
cmd( "$w add separator" );
cmd( "$w add command -label \"Quit\" -command {set choice 1} -underline 0 -accelerator Ctrl+q" );

cmd( "set w .m.edit" );
cmd( "menu $w -tearoff 0" );
cmd( ".m add cascade -label Edit -menu $w -underline 0" );
// make menu the same as ctrl-z/y (more color friendly)
cmd( "$w add command -label Undo -command { catch { savCurIni; .f.t.t edit undo; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd } } -underline 0 -accelerator Ctrl+z" );	// entryconfig 0
cmd( "$w add command -label Redo -command { catch { savCurIni; .f.t.t edit redo; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd } } -underline 2 -accelerator Ctrl+y" );	// entryconfig 1
cmd( "$w add separator" );	// entryconfig 2
// collect information to focus recoloring
cmd( "$w add command -label Cut -command { savCurIni; tk_textCut .f.t.t; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd } -underline 1 -accelerator Ctrl+x" );	// entryconfig 3
cmd( "$w add command -label Copy -command { tk_textCopy .f.t.t } -underline 0 -accelerator Ctrl+c" );	// entryconfig 4
cmd( "$w add command -label Paste -command { savCurIni; tk_textPaste .f.t.t; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd } -underline 0 -accelerator Ctrl+v" );	// entryconfig 5
cmd( "$w add command -label Delete -command { savCurIni; if { [ string equal [ .f.t.t tag ranges sel ] \"\" ] } { .f.t.t delete insert } { .f.t.t delete sel.first sel.last }; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd } -accelerator Del" );	// entryconfig 6
cmd( "$w add separator" );
cmd( "$w add command -label \"Find...\" -command {set choice 11} -underline 0 -accelerator Ctrl+f" );
cmd( "$w add command -label \"Find Again\" -command {set choice 12} -underline 5 -accelerator F3" );
cmd( "$w add command -label \"Replace...\" -command {set choice 21} -underline 0" );
cmd( "$w add command -label \"Go to Line...\" -command {set choice 10} -underline 5 -accelerator Ctrl+l" );
cmd( "$w add separator" );
cmd( "$w add command -label \"Match \\\{ \\}\" -command {set choice 17} -accelerator Ctrl+m" );
cmd( "$w add command -label \"Match \\\( \\)\" -command {set choice 32} -accelerator Ctrl+u" );
cmd( "$w add command -label \"Insert \\\{\" -command {.f.t.t insert insert \\\{} -accelerator Ctrl+\\\(" );
cmd( "$w add command -label \"Insert \\}\" -command {.f.t.t insert insert \\}} -accelerator Ctrl+\\)" );
cmd( "$w add separator" );
cmd( "$w add command -label \"Indent\" -command {set choice 42} -accelerator Ctrl+>" );
cmd( "$w add command -label \"De-indent\" -command {set choice 43} -accelerator Ctrl+<" );
cmd( "$w add separator" );
cmd( "$w add command -label \"Larger Font\" -command {incr dim_character 1; set a [list \"$fonttype\" $dim_character]; .f.t.t conf -font \"$a\"; settab .f.t.t $tabsize \"$a\"} -accelerator Ctrl+'+'" );
cmd( "$w add command -label \"Smaller Font\" -command {incr dim_character -1; set a [list \"$fonttype\" $dim_character]; .f.t.t conf -font \"$a\"; settab .f.t.t $tabsize \"$a\"} -accelerator Ctrl+'-'" );
cmd( "$w add command -label \"Font...\" -command {set choice 59} -underline 8" );
cmd( "$w add separator" );
// add option to ajust syntax highlighting (word coloring)
cmd( "$w add cascade -label \"Syntax Highlighting\" -menu $w.color -underline 0" );
cmd( "$w add check -label \"Wrap/Unwrap\" -variable wrap -command { setwrap .f.t.t $wrap } -underline 1 -accelerator Ctrl+w " );
cmd( "$w add command -label \"Tab Size...\" -command {set choice 67} -underline 7" );
cmd( "$w add command -label \"Insert LSD Macro...\" -command {set choice 28} -underline 0 -accelerator Ctrl+i" );

cmd( "menu $w.color -tearoff 0" );
cmd( "$w.color add radio -label \" Full\" -variable shigh -value 2 -command {set choice 64} -underline 1 -accelerator \"Ctrl+;\"" );
cmd( "$w.color add radio -label \" Partial\" -variable shigh -value 1 -command {set choice 65} -underline 1 -accelerator Ctrl+," );
cmd( "$w.color add radio -label \" None\" -variable shigh -value 0 -command {set choice 66} -underline 1 -accelerator Ctrl+." );

cmd( "set w .m.model" );
cmd( "menu $w -tearoff 0" );
cmd( ".m add cascade -label Model -menu $w -underline 0" );
cmd( "$w add command -label \"Compile and Run...\" -state disabled -underline 0 -command {set choice 2} -accelerator Ctrl+r" );	// entryconfig 0
cmd( "$w add command -label \"Recompile\" -state disabled -underline 0 -command {set choice 6} -accelerator Ctrl+p" );	// entryconfig 1
cmd( "$w add command -label \"[ string toupper $DbgExe ] Debugger\" -state disabled -underline 0 -command {set choice 13} -accelerator Ctrl+g" );	// entryconfig 2
cmd( "$w add command -label \"Create 'No Window' Version\" -underline 8 -state disabled -command {set choice 62}" );	// entryconfig 3
cmd( "$w add command -label \"Model Info...\" -underline 6 -state disabled -command {set choice 44}" );	// entryconfig 4
cmd( "$w add separator" );	// entryconfig 5
cmd( "$w add command -label \"Show Description\" -underline 5 -state disabled -command {set choice 5} -accelerator Ctrl+d" );	// entryconfig 6
cmd( "$w add command -label \"Show Equations\" -state disabled -underline 5 -command {set choice 8} -accelerator Ctrl+e" );	// entryconfig 7
cmd( "$w add command -label \"Show Extra Files...\" -state disabled -underline 6 -command {set choice 70} -accelerator Ctrl+j" );	// entryconfig 8
cmd( "$w add command -label \"Show Make File\" -state disabled -underline 7 -command { set choice 3}" );	// entryconfig 9
cmd( "$w add command -label \"Show Compilation Errors\" -underline 6 -state disabled -command {set choice 7}" );	// entryconfig 10
cmd( "$w add separator" );	// entryconfig 11
cmd( "$w add command -label \"Model Options...\" -underline 4 -state disabled -command {set choice 48}" );	// entryconfig 12
cmd( "$w add command -label \"System Options...\" -underline 0 -command {set choice 47}" );	// entryconfig 13
cmd( "$w add separator" );	// entryconfig 14
cmd( "$w add check -label \"Auto-hide LMM\" -variable autoHide -underline 0" );	// entryconfig 15
cmd( "$w add cascade -label \"Equation Code Style\" -underline 1 -menu $w.macro" );	// entryconfig 16

cmd( "menu $w.macro -tearoff 0" );
cmd( "$w.macro add radio -label \"Use LSD Macros\" -variable macro -value 1 -command {.m.help entryconf 1 -label \"Help on Macros for LSD Equations\" -underline 6 -command {LsdHelp LSD_macros.html}; set choice 68}" );
cmd( "$w.macro add radio -label \"Use LSD C++ functions\" -variable macro -value 0 -command {.m.help entryconf 1 -label \"Help on C++ for LSD Equations\" -underline 8 -command {LsdHelp LSD_functions.html}; set choice 69}" );

cmd( "set w .m.help" );
cmd( "menu $w -tearoff 0" );
cmd( ".m add cascade -label Help -menu $w -underline 0" );
cmd( "$w add command -label \"Help on LMM\" -underline 0 -accelerator F1 -command { LsdHelp LMM.html }" );
if ( macro )
  cmd( "$w add command -label \"Help on Macros for LSD Equations\" -underline 8 -command { LsdHelp LSD_macros.html }" );
else
  cmd( "$w add command -label \"Help on C++ for LSD Equations\" -underline 8 -command { LsdHelp LSD_functions.html }" ); 

cmd( "$w add command -label \"LSD Documentation\" -underline 4 -command { LsdHelp LSD_documentation.html }" );
cmd( "$w add separator" );
cmd( "$w add command -label \"LMM Primer Tutorial\" -underline 4 -command { LsdHelp LMM_primer.html }" );
cmd( "$w add command -label \"Using LSD Models Tutorial\" -underline 0 -command { LsdHelp model_using.html }" );
cmd( "$w add command -label \"Writing LSD Models Tutorial\" -underline 0 -command { LsdHelp model_writing.html }" );
cmd( "$w add separator" );
cmd( "$w add command -label \"About LSD...\" -underline 0 -command { LsdAbout {%s} {%s} }", _LSD_VERSION_, _LSD_DATE_  );


// Button bar
cmd( "frame .bbar -bd 2" );

cmd( "button .bbar.open -image openImg -relief $bRlf -overrelief $ovBrlf -command { set choice 33 }" );
cmd( "button .bbar.save -image saveImg -relief $bRlf -overrelief $ovBrlf -command { .m.file invoke 2 }" );
cmd( "button .bbar.undo -image undoImg -relief $bRlf -overrelief $ovBrlf -command { .m.edit invoke 0 }" );
cmd( "button .bbar.redo -image redoImg -relief $bRlf -overrelief $ovBrlf -command { .m.edit invoke 1 }" );
cmd( "button .bbar.cut -image cutImg -relief $bRlf -overrelief $ovBrlf -command { .m.edit invoke 3 }" );
cmd( "button .bbar.copy -image copyImg -relief $bRlf -overrelief $ovBrlf -command { .m.edit invoke 4 }" );
cmd( "button .bbar.paste -image pasteImg -relief $bRlf -overrelief $ovBrlf -command { .m.edit invoke 5 }" );
cmd( "button .bbar.find -image findImg -relief $bRlf -overrelief $ovBrlf -command {set choice 11}" );
cmd( "button .bbar.replace -image replaceImg -relief $bRlf -overrelief $ovBrlf -command {set choice 21}" );
cmd( "button .bbar.indent -image indentImg -relief $bRlf -overrelief $ovBrlf -command {set choice 42}" );
cmd( "button .bbar.deindent -image deindentImg -relief $bRlf -overrelief $ovBrlf -command {set choice 43}" );
cmd( "button .bbar.wrap -image wrapImg -relief $bRlf -overrelief $ovBrlf -command { if { $wrap == 0 } { set wrap 1 } { set wrap 0 }; setwrap .f.t.t $wrap }" );
cmd( "button .bbar.compile -image compileImg -relief $bRlf -overrelief $ovBrlf -command {set choice 6}" );
cmd( "button .bbar.comprun -image comprunImg -relief $bRlf -overrelief $ovBrlf -command {set choice 2}" );
cmd( "button .bbar.gdb -image gdbImg -relief $bRlf -overrelief $ovBrlf -command {set choice 13}" );
cmd( "button .bbar.info -image infoImg -relief $bRlf -overrelief $ovBrlf -command {set choice 44}" );
cmd( "button .bbar.descr -image descrImg -relief $bRlf -overrelief $ovBrlf -command {set choice 5}" );
cmd( "button .bbar.equation -image equationImg -relief $bRlf -overrelief $ovBrlf -command {set choice 8}" );
cmd( "button .bbar.extra -image extraImg -relief $bRlf -overrelief $ovBrlf -command {set choice 70}" );
cmd( "button .bbar.set -image setImg -relief $bRlf -overrelief $ovBrlf -command {set choice 48}" );
cmd( "button .bbar.hide -image hideImg -relief $bRlf -overrelief $ovBrlf -command {set autoHide [ expr ! $autoHide ]}" );
cmd( "button .bbar.help -image helpImg -relief $bRlf -overrelief $ovBrlf -command {LsdHelp LSD_macros.html}" );
cmd( "label .bbar.tip -textvariable ttip -font {Arial 8} -fg gray -width 30 -anchor w" );

cmd( "bind .bbar.open <Enter> {set ttip \"Browse models...\"}" );
cmd( "bind .bbar.open <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.save <Enter> {set ttip \"Save model\"}" );
cmd( "bind .bbar.save <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.undo <Enter> {set ttip \"Undo\"}" );
cmd( "bind .bbar.undo <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.redo <Enter> {set ttip \"Redo\"}" );
cmd( "bind .bbar.redo <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.cut <Enter> {set ttip \"Cut\"}" );
cmd( "bind .bbar.cut <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.copy <Enter> {set ttip \"Copy\"}" );
cmd( "bind .bbar.copy <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.paste <Enter> {set ttip \"Paste\"}" );
cmd( "bind .bbar.paste <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.find <Enter> {set ttip \"Find...\"}" );
cmd( "bind .bbar.find <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.replace <Enter> {set ttip \"Replace...\"}" );
cmd( "bind .bbar.replace <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.indent <Enter> {set ttip \"Indent selection\"}" );
cmd( "bind .bbar.indent <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.deindent <Enter> {set ttip \"De-indent selection\"}" );
cmd( "bind .bbar.deindent <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.wrap <Enter> {set ttip \"Wrap lines\"}" );
cmd( "bind .bbar.wrap <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.compile <Enter> {set ttip \"Recompile model\"}" );
cmd( "bind .bbar.compile <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.comprun <Enter> {set ttip \"Compile and run model...\"}" );
cmd( "bind .bbar.comprun <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.gdb <Enter> {set ttip \"Run in [ string toupper $DbgExe ] debugger\"}" );
cmd( "bind .bbar.gdb <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.info <Enter> {set ttip \"Model information...\"}" );
cmd( "bind .bbar.info <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.descr <Enter> {set ttip \"Show description\"}" );
cmd( "bind .bbar.descr <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.equation <Enter> {set ttip \"Show equations\"}" );
cmd( "bind .bbar.equation <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.extra <Enter> {set ttip \"Show extra files...\"}" );
cmd( "bind .bbar.extra <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.set <Enter> {set ttip \"Model compilation options...\"}" );
cmd( "bind .bbar.set <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.hide <Enter> {if $autoHide {set ttip \"Not auto-hide LMM\"} {set ttip \"Auto-hide LMM\"}}" );
cmd( "bind .bbar.hide <Leave> {set ttip \"\"}" );
cmd( "bind .bbar.help <Enter> {set ttip \"Help on macros for LSD equations\"}" );
cmd( "bind .bbar.help <Leave> {set ttip \"\"}" );

cmd( "pack .bbar.open .bbar.save .bbar.undo .bbar.redo .bbar.cut .bbar.copy .bbar.paste .bbar.find .bbar.replace .bbar.indent .bbar.deindent .bbar.wrap .bbar.compile .bbar.comprun .bbar.gdb .bbar.info .bbar.descr .bbar.equation .bbar.extra .bbar.set .bbar.hide .bbar.help .bbar.tip -padx 3 -side left" );
cmd( "pack .bbar -anchor w -fill x" );

cmd( "frame .f -bd 2" );
cmd( "frame .f.t" );
cmd( "scrollbar .f.t.vs -command \".f.t.t yview\"" );
cmd( "scrollbar .f.t.hs -orient horiz -command \".f.t.t xview\"" );
cmd( "text .f.t.t -undo 1 -height 2 -autoseparators 1 -yscroll \".f.t.vs set\" -xscroll \".f.t.hs set\"" );
cmd( "set a [.f.t.t conf -font]" );
cmd( "set b [lindex $a 3]" );
cmd( "if {$dim_character == 0} {set dim_character [lindex $b 1]}" );
cmd( "if {$dim_character == \"\"} {set dim_character $DefaultFontSize}" );
cmd( "set a [ list \"$fonttype\" $dim_character ]" );
// set preferred tab size and wrap option
cmd( "settab .f.t.t $tabsize \"$a\"" );	// adjust tabs size to font type/size
cmd( "setwrap .f.t.t $wrap" );		// adjust text wrap

// set syntax colors
cmd( ".f.t.t tag configure comment1 -foreground green4" );
cmd( ".f.t.t tag configure comment2 -foreground green4" );
cmd( ".f.t.t tag configure str -foreground blue4" );
cmd( ".f.t.t tag configure cprep -foreground SaddleBrown" );
cmd( ".f.t.t tag configure lsdvar -foreground red4" );
cmd( ".f.t.t tag configure lsdmacro -foreground DodgerBlue4" );
cmd( ".f.t.t tag configure ctype -foreground DarkViolet" );
cmd( ".f.t.t tag configure ckword -foreground purple4" );
cmd( ".f.t.t configure -selectbackground gray" );
cmd( ".f.t.t configure -selectforeground black" );

cmd( "frame .f.hea -relief groove -bd 2" );

cmd( "frame .f.hea.grp" );
cmd( "label .f.hea.grp.tit -text \"Group: \"" );
cmd( "label .f.hea.grp.dat -text \"$modelgroup\" -fg red" );
cmd( "pack .f.hea.grp.tit .f.hea.grp.dat -side left" );

cmd( "frame .f.hea.mod" );
cmd( "label .f.hea.mod.tit -text \"Model: \"" );
cmd( "label .f.hea.mod.dat -text \"(no model)\" -fg red" );
cmd( "pack .f.hea.mod.tit .f.hea.mod.dat -side left" );

cmd( "frame .f.hea.ver" );
cmd( "label .f.hea.ver.tit -text \"Version: \"" );
cmd( "label .f.hea.ver.dat -text \"\" -fg red" );
cmd( "pack .f.hea.ver.tit .f.hea.ver.dat -side left" );

cmd( "frame .f.hea.file" );
cmd( "label .f.hea.file.tit -text \"File: \"" );
cmd( "label .f.hea.file.dat -text \"(no file)\" -fg red" );
cmd( "pack .f.hea.file.tit .f.hea.file.dat -side left" );

cmd( "frame .f.hea.line" );
cmd( "label .f.hea.line.line -relief sunk -width 12 -text \"[.f.t.t index insert]\"" );
cmd( "pack .f.hea.line.line -anchor e -expand no" );
cmd( "pack .f.hea.grp .f.hea.mod .f.hea.ver .f.hea.file .f.hea.line -side left -expand yes -fill x" );

cmd( "bind .f.hea.line.line <Button-1> {set choice 10}" );
cmd( "bind .f.hea.line.line <Button-2> {set choice 10}" );
cmd( "bind .f.hea.line.line <Button-3> {set choice 10}" );

cmd( "pack .f.hea -fill x" );
cmd( "pack .f.t -expand yes -fill both" );
cmd( "pack .f.t.vs -side right -fill y" );
cmd( "pack .f.t.t -expand yes -fill both" );
cmd( "pack .f.t.hs -fill x" );

cmd( "bind . <F1> { .m.help invoke 0; break }" );
cmd( "bind . <Control-n> {tk_menuSetFocus .m.file}; bind . <Control-N> {tk_menuSetFocus .m.file}" );

// procedures to save cursor environment before and after changes in text window for syntax coloring
cmd( "proc savCurIni {} {global curSelIni curPosIni; set curSelIni [.f.t.t tag nextrange sel 1.0]; set curPosIni [.f.t.t index insert]; .f.t.t edit modified false}" );
cmd( "proc savCurFin {} {global curSelFin curPosFin; set curSelFin [.f.t.t tag nextrange sel 1.0]; set curPosFin [.f.t.t index insert]; .f.t.t edit modified false}" );
cmd( "proc updCurWnd {} {.f.hea.line.line conf -text [.f.t.t index insert]}" );

// redefine bindings to better support new syntax highlight routine
cmd( "bind .f.t.t <KeyPress> {savCurIni}" );
cmd( "bind .f.t.t <KeyRelease> {if {[.f.t.t edit modified]} {savCurFin; set choice 23}; updCurWnd}" );
cmd( "bind .f.t.t <ButtonPress> {savCurIni}" );
cmd( "bind .f.t.t <ButtonRelease> {if {[.f.t.t edit modified]} {savCurFin; set choice 23}; updCurWnd}" );

cmd( "bind .f.t.t <Control-l> {set choice 10}; bind .f.t.t <Control-L> {set choice 10}" );
cmd( "bind .f.t.t <Control-w> { if { $wrap == 0 } { set wrap 1 } { set wrap 0 }; setwrap .f.t.t $wrap }" );

cmd( "bind .f.t.t <Control-f> {set choice 11}; bind .f.t.t <Control-F> {set choice 11}" );
cmd( "bind .f.t.t <F3> {set choice 12}" );
cmd( "bind .f.t.t <Control-s> { .m.file invoke 2 }" ); 
cmd( "bind .f.t.t <Control-a> { set choice 4 }" );
cmd( "bind .f.t.t <Control-r> {set choice 2}; bind .f.t.t <Control-R> {set choice 2}" );
cmd( "bind .f.t.t <Control-e> {set choice 8}" );
cmd( "bind .f.t.t <Control-j> {set choice 70}" );
cmd( "bind .f.t.t <Control-KeyRelease-o> { if { $tk_strictMotif == 0 } { set a [ .f.t.t index insert ]; .f.t.t delete \"$a lineend\"} {}; if { $showFileCmds == 1 } { set choice 15 }; break }" );
cmd( "bind .f.t.t <Control-q> {set choice 1}; bind .f.t.t <Control-Q> {set choice 1}" );
cmd( "bind .f.t.t <Control-p> {set choice 6; break}; bind .f.t.t <Control-P> {set choice 6; break}" );
cmd( "bind .f.t.t <Control-u> {set choice 32}" );
cmd( "bind .f.t.t <Control-m> {set choice 17}" );
cmd( "bind .f.t.t <Control-g> {set choice 13}; bind .f.t.t <Control-G> {set choice 13}" );
cmd( "bind .f.t.t <Control-d> {set choice 5; break}" );
cmd( "bind .f.t.t <Control-b> {set choice 33; break}; bind .f.t.t <Control-B> {set choice 33; break}" );
cmd( "bind .f.t.t <Control-minus> {incr dim_character -2; set a [ list \"$fonttype\" $dim_character ]; .f.t.t conf -font \"$a\"}" );
cmd( "bind .f.t.t <Control-plus> {incr dim_character 2; set a [ list \"$fonttype\" $dim_character ]; .f.t.t conf -font \"$a\"}" );
cmd( "bind .f.t.t <Control-parenleft> {.f.t.t insert insert \\{}" );
cmd( "bind .f.t.t <Control-parenright> {.f.t.t insert insert \\}}" );
cmd( "bind .f.t.t <Control-greater> {set choice 42}" );
cmd( "bind .f.t.t <Control-less> {set choice 43}" );
cmd( "bind .f.t.t <Control-semicolon> {set choice 64}" );
cmd( "bind .f.t.t <Control-comma> {set choice 65}" );
cmd( "bind .f.t.t <Control-period> {set choice 66}" );
cmd( "bind .f.t.t <Alt-q> {.m postcascade 0}; bind .f.t.t <Alt-Q> {.m postcascade 0}" );

// redefine the default Tk bindings adding "break" to avoid further event processing
cmd( "bind .f.t.t <Control-z> { .m.edit invoke 0; break }; bind .f.t.t <Control-Z> { event generate .f.t.t <Control-z> }" );
cmd( "bind .f.t.t <Control-y> { .m.edit invoke 1; break }; bind .f.t.t <Control-Y> { event generate .f.t.t <Control-y> }" );
cmd( "bind .f.t.t <Control-x> { .m.edit invoke 3; break }; bind .f.t.t <Control-X> { event generate .f.t.t <Control-x> }" );
cmd( "bind .f.t.t <Control-c> { .m.edit invoke 4; break }" );
cmd( "bind .f.t.t <Control-v> { .m.edit invoke 5; break }" );
cmd( "bind .f.t.t <Delete> { .m.edit invoke 6; break }" );
cmd( "bind .f.t.t <BackSpace> { savCurIni; if { [ string equal [ .f.t.t tag ranges sel ] \"\" ] } { if { ! [ string equal [ .f.t.t index insert ] 1.0 ] } { .f.t.t delete insert-1c } } { .f.t.t delete sel.first sel.last }; if { [ .f.t.t edit modified ] } { savCurFin; set choice 23 }; updCurWnd; break }" );
cmd( "if [ string equal $tcl_platform(platform) unix ] { bind .f.t.t <Control-Insert> { .m.edit invoke 4; break } }" );
cmd( "if [ string equal $tcl_platform(platform) unix ] { bind .f.t.t <Shift-Insert> { .m.edit invoke 5; break } }" );

cmd( "bind . <KeyPress-Insert> {# nothing}" );
cmd( "bind .f.t.t <KeyPress-Return> {+set choice 16}" );
cmd( "bind .f.t.t <KeyRelease-space> {+.f.t.t edit separator}" );

/*
POPUP menu
*/
cmd( "menu .v -tearoff 0" );

cmd( "bind .f.t.t  <3> {%%W mark set insert [.f.t.t index @%%x,%%y]; set vmenuInsert [.f.t.t index insert]; tk_popup .v %%X %%Y}" );
cmd( "bind .f.t.t  <2> {%%W mark set insert [.f.t.t index @%%x,%%y]; set vmenuInsert [.f.t.t index insert]; tk_popup .v %%X %%Y}" );
cmd( ".v add command -label \"Copy\" -command {tk_textCopy .f.t.t}" );
cmd( ".v add command -label \"Cut\" -command {tk_textCut .f.t.t}" );
cmd( ".v add command -label \"Paste\" -command {tk_textPaste .f.t.t}" );

cmd( ".v add separator" );
cmd( ".v add cascade -label \"LSD Macro\" -menu .v.i" );
cmd( ".v add command -label \"Indent Selection\" -command {set choice 42}" );
cmd( ".v add command -label \"De-indent Selection\" -command {set choice 43}" );
cmd( ".v add command -label \"Place Break & Run [ string toupper $DbgExe ]\" -command {set choice 58}" );

cmd( ".v add separator" );
cmd( ".v add command -label \"Find...\" -command {set choice 11}" );
cmd( ".v add command -label \"Match \\\{ \\}\" -command {set choice 17}" );
cmd( ".v add command -label \"Match \\\( \\)\" -command {set choice 32}" );

if ( ! macro )
{
	cmd( "menu .v.i -tearoff 0" );
	cmd( ".v.i add command -label \"LSD equation/function\" -command {set choice 25} -accelerator Ctrl+E" );
	cmd( ".v.i add command -label \"cal(...)\" -command {set choice 26} -accelerator Ctrl+V" );
	cmd( ".v.i add command -label \"for ( ; ; )\" -command {set choice 27} -accelerator Ctrl+C" );
	cmd( ".v.i add command -label \"sum(...)\" -command {set choice 56} -accelerator Ctrl+U" );
	cmd( ".v.i add command -label \"increment(...)\" -command {set choice 40} -accelerator Ctrl+I" );
	cmd( ".v.i add command -label \"multiply(...)\" -command {set choice 45} -accelerator Ctrl+M" );
	cmd( ".v.i add command -label \"search(...)\" -command {set choice 55} -accelerator Ctrl+A" );
	cmd( ".v.i add command -label \"search_var_cond(...)\" -command {set choice 30} -accelerator Ctrl+S" );
	cmd( ".v.i add command -label \"lsdqsort(...)\" -command {set choice 31} -accelerator Ctrl+T" );
	cmd( ".v.i add command -label \"draw_rnd\" -command {set choice 54} -accelerator Ctrl+N" );
	cmd( ".v.i add command -label \"write(...)\" -command {set choice 29} -accelerator Ctrl+W" );
	cmd( ".v.i add command -label \"add_n_objects2\" -command {set choice 52} -accelerator Ctrl+O" );
	cmd( ".v.i add command -label \"delete_obj\" -command {set choice 53} -accelerator Ctrl+D" );
	cmd( ".v.i add command -label \"Math functions\" -command {set choice 51} -accelerator Ctrl+H" );
}
else
{
	cmd( "menu .v.i -tearoff 0" );
	cmd( ".v.i add command -label \"EQUATION\" -command {set choice 25} -accelerator Ctrl+E" );
	cmd( ".v.i add command -label \"V(...)\" -command {set choice 26} -accelerator Ctrl+V" );
	cmd( ".v.i add command -label \"CYCLE(...)\" -command {set choice 27} -accelerator Ctrl+C" );
	cmd( ".v.i add command -label \"SUM(...)\" -command {set choice 56} -accelerator Ctrl+U" );
	cmd( ".v.i add command -label \"INCR(...)\" -command {set choice 40} -accelerator Ctrl+I" );
	cmd( ".v.i add command -label \"MULT(...)\" -command {set choice 45} -accelerator Ctrl+M" );
	cmd( ".v.i add command -label \"SEARCH(...)\" -command {set choice 55} -accelerator Ctrl+A" );
	cmd( ".v.i add command -label \"SEARCH_CND(...)\" -command {set choice 30} -accelerator Ctrl+S" );
	cmd( ".v.i add command -label \"SORT(...)\" -command {set choice 31} -accelerator Ctrl+T" );
	cmd( ".v.i add command -label \"RNDDRAW(...)\" -command {set choice 54} -accelerator Ctrl+N" );
	cmd( ".v.i add command -label \"WRITE(...)\" -command {set choice 29} -accelerator Ctrl+W" );
	cmd( ".v.i add command -label \"ADDOBJ(...)\" -command {set choice 52} -accelerator Ctrl+O" );
	cmd( ".v.i add command -label \"DELETE(...)\" -command {set choice 53} -accelerator Ctrl+D" );
	cmd( ".v.i add command -label \"Network macros\" -command {set choice 72} -accelerator Ctrl+K" );
	cmd( ".v.i add command -label \"Math functions\" -command {set choice 51} -accelerator Ctrl+H" );
}

cmd( "bind .f.t.t <Control-E> {set choice 25}" );
cmd( "bind .f.t.t <Control-V> {set choice 26; break}" );
cmd( "bind .f.t.t <Control-U> {set choice 56}" );
cmd( "bind .f.t.t <Control-A> {set choice 55}" );
cmd( "bind .f.t.t <Control-C> {set choice 27; break}" );
cmd( "bind .f.t.t <Control-i> {set choice 28; break}" );
cmd( "bind .f.t.t <Control-T> {set choice 31}" );
cmd( "bind .f.t.t <Control-I> {set choice 40}" );
cmd( "bind .f.t.t <Control-M> {set choice 45}" );
cmd( "bind .f.t.t <Control-S> {set choice 30}" );
cmd( "bind .f.t.t <Control-W> {set choice 29}" );
cmd( "bind .f.t.t <Control-O> {set choice 52}" );
cmd( "bind .f.t.t <Control-D> {set choice 53}" );
cmd( "bind .f.t.t <Control-N> {set choice 54}" );
cmd( "bind .f.t.t <Control-H> {set choice 51}" );
cmd( "bind .f.t.t <Control-K> { set choice 72 }" );

cmd( "bind .f.t.t <F1> {set choice 34}" );

// set advanced double-click behavior (dblclick.tcl) and also
// reset the word boundaries to fix weird double-click behavior on Windows
cmd( "setDoubleclickBinding .f.t.t" );
cmd( "set tcl_wordchars {\\w}" );
cmd( "set tcl_nonwordchars {\\W}" );
cmd( "set textsearch \"\"" );
cmd( "set datasel \"\"" );

cmd( "pack .f -expand yes -fill both" );
cmd( "pack .f.t -expand yes -fill both" );
cmd( "pack .f.t.vs -side right -fill y" );
cmd( "pack .f.t.t -expand yes -fill both" );
cmd( "pack .f.t.hs -fill x" );

cmd( "set filename \"noname.txt\"" );
cmd( "set dirname \"[pwd]\"" );
cmd( "set modeldir \"[pwd]\"" );
cmd( "set groupdir \"[pwd]\"" );

cmd( ".f.t.t tag conf sel -foreground white" );
cmd( "set before [.f.t.t get 1.0 end]" );

if ( argn > 1 )
{
	cmd( "if [ file exists \"$filetoload\" ] { set choice 0 } { set choice -2 }" );
	if ( choice == 0 )
	{	
		cmd( "set file [open \"$filetoload\"]" );
		cmd( ".f.t.t insert end [read $file]" );
		cmd( ".f.t.t edit reset" );
		cmd( "close $file" );
		cmd( ".f.t.t mark set insert 1.0" );
		cmd( "set filename \"[file tail \"$filetoload\"]\"" );
		cmd( "set dirname [file dirname \"$filetoload\"]" );
		cmd( "set before [.f.t.t get 1.0 end]; .f.hea.file.dat conf -text \"$filename\"" );

		cmd( "set s [file extension \"$filetoload\"]" );
		s = ( char * ) Tcl_GetVar( inter, "s", 0 );
		choice = 0;
		if ( s[0] != '\0' )
		{
			strncpy( str, s, 999 );
			if ( ! strcmp( str, ".cpp" ) || ! strcmp( str, ".c" ) || ! strcmp( str, ".C" ) || ! strcmp( str, ".CPP" ) || ! strcmp( str, ".Cpp" ) || ! strcmp( str, ".c++" ) || ! strcmp( str, ".C++" ) || ! strcmp( str, ".h" ) || ! strcmp( str, ".H" ) || ! strcmp( str, ".hpp" ) || ! strcmp( str, ".HPP" ) || ! strcmp( str, ".Hpp" ) )
			{
				sourcefile = 1;
				color( shigh, 0, 0 );	// set color types (all text)
			}
			else
				sourcefile = 0;
		}
   }
   else
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"File missing\" -detail \"File '$filetoload' not found.\"" ); 
}
else
	choice = 33; 				// open model browser
  
cmd( "wm deiconify .; focus -force .f.t.t; update" );

loop:

// update file save status in titlebar
cmd( "update_title_bar" );

// main command loop
while ( ! choice )
{
	try
	{
		Tcl_DoOneEvent( 0 );
	}
	catch ( bad_alloc& ) 		// raise memory problems
	{
		throw;
	}
	catch ( ... )				// ignore the rest
	{
		goto loop;
	}
}   

// update file save status in titlebar
cmd( "update_title_bar" );

// verify if saving before command is necessary
if ( choice == 1 || choice == 2 || choice == 3 || choice == 5 || choice == 6 || choice == 8 || choice == 13 || choice == 14 || choice == 15 || choice == 33 || choice == 39 || choice == 41 || choice == 58 || choice == 71 )
	if ( ! discard_change( ) )
		goto loop;

// start evaluating the executed command

// exit LMM

if ( choice == 1 )
	return 0;


// compile the model, invoking make
// Run the model

if ( choice == 2 || choice == 6 )
{
	compile_run( choice == 2 ? true : false );
	 
	choice = 0;
	goto loop;
}


/* Insert in the text window the makefile of the selected model */

if ( choice == 3 )
{
	cmd( ".f.t.t delete 0.0 end" );
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );

	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}

	cmd( "cd \"$modeldir\"" );
	make_makefile( ); 
	cmd( "cd \"$RootLsd\"" );
	cmd( "if { [file exists \"$modeldir/makefile\"]==1} {set choice 1} {set choice 0}" );
	if ( choice == 1 )
	{
		cmd( "set file [open \"$modeldir/makefile\" r]" );
		cmd( ".f.t.t insert end [read -nonewline $file]" );
		cmd( ".f.t.t edit reset" );
		cmd( "close $file" );
	} 
	
	sourcefile = 0; 
	
	cmd( "set before [.f.t.t get 1.0 end]" );
	cmd( "set filename makefile" );
	cmd( ".f.t.t mark set insert 1.0" );
	cmd( ".f.hea.file.dat conf -text \"makefile\"" );
	cmd( "tk_messageBox -parent . -title Warning -icon warning -type ok -message \"Makefile should not be changed\" -detail \"Direct changes to the 'makefile' will not affect compilation issued through LMM. Please check 'Model Options' and 'System Options' in menu 'Model' to change compilation options.\"" );  
	
	choice = 0;
	goto loop;
}


/* Save the file currently shown */

if ( choice == 4 )
{
	cmd( "set curfilename [tk_getSaveFile -parent . -title \"Save File\" -initialfile $filename -initialdir $dirname]" );
	s = ( char * ) Tcl_GetVar( inter, "curfilename", 0 );

	if ( s != NULL && strcmp( s, "" ) )
	{
		cmd( "if [ file exist \"$dirname/$filename\" ] { file copy -force \"$dirname/$filename\" \"$dirname/[file rootname \"$filename\"].bak\" }" );
		cmd( "set file [ open \"$curfilename\" w ]" );
		cmd( "puts -nonewline $file [ .f.t.t get 0.0 end ]" );
		cmd( "close $file" );
		cmd( "set before [ .f.t.t get 0.0 end ]" );
		cmd( "set dirname [ file dirname \"$curfilename\" ]" );
		cmd( "set filename [ file tail \"$curfilename\" ]" );
		cmd( ".f.hea.file.dat conf -text \"$filename\"" );
	}
	
	choice = 0;
	goto loop;
}


/*Load the description file*/

if ( choice == 5 || choice == 50 )
{
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );

	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}

	cmd( "set dirname \"$modeldir\"" );
	cmd( "set filename description.txt" );
	  
	cmd( ".f.t.t delete 0.0 end" );
	cmd( "set choice 0; if { [ file exists \"$dirname/$filename\" ] } { set choice 1; if { [ file size \"$dirname/$filename\" ] <= 2 } { set choice 0; file delete \"$dirname/$filename\" } }" );
	if ( choice == 1 )
	{
		cmd( "set file [open \"$dirname/description.txt\" r]" );
		cmd( ".f.t.t insert end [read -nonewline $file]" );
		cmd( "close $file" );
		cmd( "set before [.f.t.t get 1.0 end]" );
	}
	else		// if no description, ask if the user wants to create it or not
	{
		cmd( "set answer [ tk_messageBox -parent . -type yesno -default no -icon question -title \"Create Description\" -message \"Create a description file?\" -detail \"There is no valid description file ('description.txt') set for the model\n\nDo you want to create a description file now?\n\nPress 'No' to just show the equations file.\" ]" );
		cmd( " if [ string equal $answer yes ] { set choice 1 } { set choice 2 } " );
		if ( choice == 2 )
		{
			cmd( " set filename \"\" " );
			cmd( "set before [.f.t.t get 0.0 end]" );
			choice = 8;		// load equations file
			goto loop;
		}
		cmd( ".f.t.t insert end \"Model $modelname (ver. $version)\n\n(Enter the Model description text here)\n\n(PRESS CTRL+E TO EDIT EQUATIONS)\n\"" );
	}

	cmd( ".f.t.t edit reset" );
	sourcefile = 0;
	cmd( ".f.t.t mark set insert 1.0" );
	cmd( "focus -force .f.t.t" );

	cmd( ".f.hea.file.dat conf -text \"$filename\"" );
	  
	cmd( "unset -nocomplain ud" );
	cmd( "unset -nocomplain udi" );
	cmd( "unset -nocomplain rd" );
	cmd( "unset -nocomplain rdi" );
	cmd( "lappend ud [.f.t.t get 0.0 end]" );
	cmd( "lappend udi [.f.t.t index insert]" );
	  
	if ( choice == 50 )
		choice = 46; 			// go to create makefile, after the model selection
	else
		choice = 0;  			// just relax
	
	goto loop;
}
 
 
/*Show compilation result*/

if ( choice == 7 )
{
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}

	create_compresult_window( );
	choice = 0;
	goto loop;
}


/* Insert in the text window the main equation file */

if ( choice == 8 )
{
	cmd( ".f.t.t delete 1.0 end" );
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );

	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}
	 
	s = get_fun_name( str );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "set filename \"\"" );
		cmd( "set dirname \"\"" );
		cmd( "set before [.f.t.t get 1.0 end]" );  
		cmd( "update" );
		choice = 0;
		goto loop;
	}
	else
		cmd( "set filename \"%s\"", s );
	
	cmd( "set dirname \"$modeldir\"" );
	cmd( "set file [open \"$dirname/$filename\" r]" );
	cmd( ".f.t.t insert end [read -nonewline $file]" );
	cmd( "close $file" );
	cmd( ".f.t.t edit reset" );
	cmd( ".f.t.t tag remove sel 1.0 end" ); 
	cmd( "focus -force .f.t.t" );

	// handle the opening of files from the compilation error window
	cmd( "if { [ info exists errfil ] && [ string equal \"$errfil\" \"[ file normalize \"$modeldir/$filename\" ]\" ] && [ info exists errlin ] && [ string is integer -strict $errlin ] } { \
			.f.t.t tag add sel $errlin.0 $errlin.end; \
			if { [ info exists errcol ] && $errcol != \"\" && [ string is integer -strict $errcol ] } { \
				.f.t.t see $errlin.$errcol; \
				.f.t.t mark set insert $errlin.$errcol \
			} else { \
				.f.t.t see $errlin.0; \
				.f.t.t mark set insert $errlin.0 \
			} \
		} else { \
			.f.t.t mark set insert 1.0 \
		}" );

	cmd( "set before [.f.t.t get 1.0 end]" );
	cmd( ".f.hea.file.dat conf -text \"$filename\"" );

	cmd( ".f.t.t tag add bc \"1.0\"" );
	cmd( ".f.t.t tag add fc \"1.0\"" );

	sourcefile=1;
	cmd( "savCurIni; savCurFin; updCurWnd" );	// save data for recolor
	color(shigh, 0, 0 );			// set color types (all text)
	  
	cmd( "catch [unset -nocomplain ud]" );
	cmd( "catch [unset -nocomplain udi]" );
	cmd( "catch [unset -nocomplain rd]" );
	cmd( "catch [unset -nocomplain rdi]" );
	cmd( "lappend ud [.f.t.t get 0.0 end]" );
	cmd( "lappend udi [.f.t.t index insert]" );

	choice = 0;
	goto loop;
}


/* Find a line in the text */

if ( choice == 10 )
{
	cmd( "if [ winfo exists .search_line ] { set choice 0; focus .search_line.e }" );
	if ( choice == 0 )
		goto loop;

	cmd( "set line \"\"" );
	cmd( "newtop .search_line \"Go To\" { .search_line.b.can invoke }" );

	cmd( "frame .search_line.l" );
	cmd( "label .search_line.l.l -text \"Line number\"" );
	cmd( "entry .search_line.l.e -justify center -width 10 -textvariable line" );
	cmd( "pack .search_line.l.l .search_line.l.e" );

	cmd( "pack .search_line.l -padx 5 -pady 5" );

	cmd( "okcancel .search_line b { \
			if { $line == \"\" } { \
				.search_line.can invoke \
			} else { \
				if [ string is integer -strict $line ] { \
					.f.t.t tag remove sel 1.0 end; \
					.f.t.t see $line.0; \
					.f.t.t tag add sel $line.0 $line.end; \
					.f.t.t mark set insert $line.0; \
					.f.hea.line.line conf -text [ .f.t.t index insert ]; \
					destroytop .search_line \
				} else { \
					destroytop .search_line; \
					tk_messageBox -parent .search_line -type ok -title Error -icon error -message \"Invalid value\" -detail \"Please check that a valid integer is used.\" \
				} \
			} \
		} { \
			destroytop .search_line \
		}" );
	cmd( "bind .search_line <KeyPress-Return> { .search_line.b.ok invoke }" );

	cmd( "showtop .search_line" );
	cmd( "focus .search_line.l.e" );

	choice = 0;
	goto loop;
}


/* Find a text pattern in the text */

if ( choice == 11 )
{
	cmd( "if [ winfo exists .find ] { set choice 0; focus .find. e}" );
	if ( choice == 0 )
		goto loop;

	cmd( "set docase 0" );
	cmd( "set dirsearch \"-forwards\"" );
	cmd( "set curcounter $lfindsize" );
	cmd( "if { ! [ string equal [ .f.t.t tag ranges sel ] \"\" ] } { set textsearch [ .f.t.t get sel.first sel.last ] } { set textsearch \"\" }" );

	cmd( "newtop .find \"Find\" { .find.b.can invoke }" );

	cmd( "frame .find.l" );
	cmd( "label .find.l.l -text \"Text to find\"" );
	cmd( "entry .find.l.e -width 25 -textvariable textsearch -justify center" );
	cmd( "pack .find.l.l .find.l.e" );

	cmd( "frame .find.r -relief groove -bd 2" );
	cmd( "radiobutton .find.r.r2 -text \"Up\" -variable dirsearch -value \"-backwards\" -command { set endsearch 1.0 }" );
	cmd( "radiobutton .find.r.r1 -text \"Down\" -variable dirsearch -value \"-forwards\" -command { set endsearch end }" );
	cmd( "pack .find.r.r2 .find.r.r1 -side left -padx 5" );

	cmd( "checkbutton .find.c -text \"Case sensitive\" -variable docase" );

	cmd( "pack .find.l .find.r .find.c -padx 5 -pady 5" );

	cmd( "Xcancel .find b Find { \
			if { $textsearch != \"\" } { \
				incr lfindsize; \
				set curcounter [ expr $lfindsize - 1 ]; \
				lappend lfind \"$textsearch\"; \
				.f.t.t tag remove sel 1.0 end; \
				set cur [ .f.t.t index insert ]; \
				if { $docase } { \
					set cur [ .f.t.t search -count length $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				} else { \
					set cur [ .f.t.t search -count length -nocase $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				}; \
				if { [ string length $cur ] > 0 } { \
					.f.t.t tag add sel $cur \"$cur + $length char\"; \
					if { [ string compare $endsearch end ] != 0 } { \
						set length 0 \
					}; \
					.f.t.t mark set insert \"$cur + $length char\" ; \
					update; \
					.f.t.t see $cur; \
					destroytop .find \
				} else { \
					.find.l.e selection range 0 end; \
					bell \
				} \
			} { \
				bell \
			} \
		} { destroytop .find }" );

	cmd( "bind .find.l.e <Up> { \
			if { $lfindsize > 0 && $curcounter > 0 } { \
				incr curcounter -1; \
				set textsearch \"[ lindex $lfind $curcounter ]\"; \
				.find.l.e selection range 0 end \
			} \
		}" );
	cmd( "bind .find.l.e <Down> { \
			if { $lfindsize > 0 && $curcounter < [ expr $lfindsize - 1 ] } { \
				incr curcounter; \
				set textsearch \"[ lindex $lfind $curcounter ]\"; \
				.find.l.e selection range 0 end \
			} \
		}" );
	cmd( "bind .find <KeyPress-Return> { .find.b.ok invoke }" );

	cmd( "showtop .find" );
	cmd( ".find.l.e selection range 0 end" );
	cmd( "focus .find.l.e" );

	choice = 0;
	goto loop;
}


/* Search again the same pattern in the text*/

if ( choice == 12 )
{
	cmd( "if { $textsearch != \"\" } { \
			.f.t.t tag remove sel 1.0 end; \
			set cur [ .f.t.t index insert ]; \
				if { $docase } { \
					set cur [ .f.t.t search -count length $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				} else { \
					set cur [ .f.t.t search -count length -nocase $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				}; \
				if { [ string length $cur ] > 0 } { \
				.f.t.t tag add sel $cur \"$cur + $length char\"; \
				if { [ string compare $endsearch end ] != 0 } { \
					set length 0 \
				}; \
				.f.t.t mark set insert \"$cur + $length char\"; \
				update; \
				.f.t.t see $cur \
			} else { \
				bell \
			} \
		}" );
		
	choice = 0;
	goto loop;
}


/* Find and replace a text pattern in the text */

if ( choice == 21 )
{
	cmd( "if [ winfo exists .l ] { set choice 0; focus .l.e }" );
	if ( choice == 0 )
		goto loop;

	cmd( "set docase 1" );
	cmd( "set dirsearch \"-forwards\"" );
	cmd( "set curcounter $lfindsize" );
	cmd( "set cur \"\"" );
	cmd( "if { ! [ string equal [ .f.t.t tag ranges sel ] \"\" ] } { set textsearch [ .f.t.t get sel.first sel.last ] } { set textsearch \"\" }" );

	cmd( "newtop .l \"Replace\" { .l.b2.can invoke }" );

	cmd( "frame .l.l" );
	cmd( "label .l.l.l -text \"Text to find\"" );
	cmd( "entry .l.l.e -width 25 -textvariable textsearch -justify center" );
	cmd( "pack .l.l.l .l.l.e" );

	cmd( "frame .l.p" );
	cmd( "label .l.p.l -text \"Text to replace\"" );
	cmd( "entry .l.p.e -width 25 -textvariable textrepl -justify center" );
	cmd( "pack .l.p.l .l.p.e" );

	cmd( "frame .l.r -relief groove -bd 2" );
	cmd( "radiobutton .l.r.r2 -text \"Up\" -variable dirsearch -value \"-backwards\" -command { set endsearch 1.0 }" );
	cmd( "radiobutton .l.r.r1 -text \"Down\" -variable dirsearch -value \"-forwards\" -command { set endsearch end }" );
	cmd( "pack .l.r.r2 .l.r.r1 -side left -padx 5" );

	cmd( "checkbutton .l.c -text \"Case sensitive\" -variable docase" );

	cmd( "pack .l.l .l.p .l.r .l.c -padx 5 -pady 5" );

	cmd( "frame .l.b1" );
	cmd( "button .l.b1.repl -width $butWid -state disabled -text Replace -command { \
			if { [ string length $cur ] > 0 } { \
				.f.t.t delete $cur \"$cur + $length char\"; \
				.f.t.t insert $cur \"$textrepl\"; \
				if { [ string compare $endsearch end ] != 0 } { \
					.f.t.t mark set insert $cur \
				}; \
				.l.b2.ok invoke \
			} \
		}" );
	cmd( "button .l.b1.all -width $butWid -state disabled -text \"Repl. All\" -command { set choice 4 }" );
	cmd( "pack .l.b1.repl .l.b1.all -padx 10 -side left" );

	cmd( "pack .l.b1 -anchor e" );

	cmd( "Xcancel .l b2 Find { \
			if { $textsearch != \"\" } { \
				incr lfindsize; \
				set curcounter [ expr $lfindsize - 1 ]; \
				lappend lfind \"$textsearch\"; \
				.f.t.t tag remove found 1.0 end; \
				.f.t.t tag remove sel 1.0 end; \
				set cur [ .f.t.t index insert ]; \
				if { $docase } { \
					set cur [ .f.t.t search -count length $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				} else { \
					set cur [ .f.t.t search -count length -nocase $dirsearch -- \"$textsearch\" $cur $endsearch ] \
				}; \
				if { [ string length $cur ] > 0 } { \
					.f.t.t tag add found $cur \"$cur + $length char\"; \
					if { [ string compare $endsearch end ] == 0 } { \
						.f.t.t mark set insert \"$cur + $length char\" \
					} else { \
						.f.t.t mark set insert $cur \
					}; \
					update; \
					.f.t.t see $cur; \
					.l.b1.repl conf -state normal; \
					.l.b1.all conf -state normal \
				} else { \
					.l.b1.all conf -state disabled; \
					.l.b1.repl conf -state disabled \
				} \
			} { \
				bell \
			} \
		} { \
			focus -force .f.t.t; \
			set choice 5 \
		}" );
		
	cmd( "bind .l.l.e <Up> { \
			if { $lfindsize > 0 && $curcounter > 0 } { \
				incr curcounter -1; \
				set textsearch \"[ lindex $lfind $curcounter ]\"; \
				.l.l.e selection range 0 end \
			} \
		}" );
	cmd( "bind .l.l.e <Down> { \
			if { $lfindsize > 0 && $curcounter < [ expr $lfindsize - 1 ] } { \
				incr curcounter; \
				set textsearch \"[ lindex $lfind $curcounter ]\"; \
				.l.l.e selection range 0 end \
			} \
		}" );
	cmd( "bind .l <KeyPress-Return> {.l.b2.ok invoke}" );

	cmd( "showtop .l" );
	cmd( ".l.l.e selection range 0 end" );
	cmd( "focus .l.l.e" );
	cmd( ".f.t.t tag conf found -background red -foreground white" );

	choice = 0;
	
	here:
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	if ( choice == 3 )
	{
		choice = 0;
		goto here;
	}

	while ( choice == 4 )
	{ 
		cmd( "if { [ string length $cur ] > 0 } { \
			.f.t.t delete $cur \"$cur + $length char\"; \
			.f.t.t see $cur; \
			.f.t.t insert $cur \"$textrepl\"; \
			if { [ string compare $endsearch end ] == 0 } { \
				set cur [ .f.t.t index \"$cur+$length char\" ] \
			} \
		} { \
			set choice 0 \
		}" );
		
		if ( choice != 0 )  
			cmd( "if { $docase } { \
						set cur [ .f.t.t search -count length $dirsearch -- \"$textsearch\" $cur $endsearch ] \
					} else { \
						set cur [ .f.t.t search -count length -nocase $dirsearch -- \"$textsearch\" $cur $endsearch ] \
					}" );
		
		cmd( "raise .l" );
		cmd( "focus .l" );
		Tcl_DoOneEvent( 0 );  
		goto here;
	}

	cmd( "destroytop .l" );

	cmd( ".f.t.t tag remove found 1.0 end" );
	color( shigh, 0, 0 );				// reevaluate colors
	choice = 0;
	goto loop;
}


// Run the model in the gdb debugger

if ( choice == 13 || choice == 58 )
{
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );

	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}
	  
	cmd( "cd \"$modeldir\"" );

	if ( choice == 58 )
	{
		cmd( "scan $vmenuInsert %%d.%%d line col" );
		cmd( "if [ string equal -nocase $DbgExe lldb ] { set breakExt lldb; set breakTxt \"breakpoint set -f $dirname/$filename -l$line\nrun\n\" } { set breakExt gdb; set breakTxt \"break $dirname/$filename:$line\nrun\n\" }" );
		cmd( "catch { set f [ open break.$breakExt w ]; puts $f $breakTxt; close $f }" );

		cmd( "if [ string equal -nocase $DbgExe lldb ] { set cmdbreak \"-sbreak.lldb\" } { set cmdbreak \"--command=break.gdb\" }" );
	}
	else
		cmd( "if [ string equal -nocase $DbgExe gdb ] { set cmdbreak \"--args\" } { set cmdbreak \"\" }" ); 

	make_makefile( );  
	cmd( "set fapp [file nativename \"$modeldir/makefile\"]" );
	s = ( char * ) Tcl_GetVar( inter, "fapp", 0 );
	f = fopen( s, "r" );
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Makefile not created\" -detail \"Please check 'Model Options' and 'System Options' in menu 'Model'.\"" );
		goto end_gdb;
	}
	fscanf( f, "%999s", str );
	while ( strncmp( str, "TARGET=", 7 ) && fscanf( f, "%999s", str ) != EOF );
	if ( strncmp(str, "TARGET=", 7) != 0 )
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Makefile corrupted\" -detail \"Please check 'Model Options' and 'System Options' in menu 'Model'.\"" );
		goto end_gdb;
	}

	strcpy( str1, str + 7 );

	cmd( "if [ string equal $tcl_platform(platform) unix ] { if [ string equal $tcl_platform(os) Darwin ] { set choice 2 } { set choice 1 } } { set choice 3 }" );
	
	switch( choice )
	{
		case 1:		// Unix
			sprintf( msg, "catch { exec $DbgTerm -e $DbgExe $cmdbreak %s & } result", str1 );
			break;
	
		case 2:		// Mac
#ifdef MAC_PKG
            cmd("if [string equal $cmdbreak \"--args\"] { set cmdbreak \"\"} {}");
			sprintf( msg, "catch { exec osascript -e \"tell application \\\"$DbgTerm\\\" to do script \\\"cd $dirname; clear; $DbgExe $cmdbreak -f %s.app/Contents/MacOS/%s\\\"\" & } result", str1, str1 );
#else
			sprintf( msg, "catch { exec osascript -e \"tell application \\\"$DbgTerm\\\" to do script \\\"cd $dirname; clear; $DbgExe $cmdbreak -f %s\\\"\" & } result", str1 );
#endif
			break;
			
		case 3:		// Windows
			strcat( str1, ".exe" );
			sprintf( msg, "catch { exec $DbgTerm /c $DbgExe $cmdbreak %s & } result", str1 );
			break;
			
		default:
			goto end_gdb;
	}
	
	// check if executable file is older than model file
	s = get_fun_name( str );
	if ( s == NULL || ! strcmp( s, "" ) )
		goto end_gdb;
	strncpy( str, s, 999 );
	s = ( char * ) Tcl_GetVar( inter, "modeldir", 0 );
	if ( s != NULL && strcmp( s, "" ) )
	{
		sprintf( str2, "%s/%s", s, str );
#ifdef MAC_PKG
		sprintf( str, "%s/%s.app/Contents/MacOS/%s", s, str1, str1 );
#else
		sprintf( str, "%s/%s", s, str1 );
#endif
	}

	// get OS info for files
	struct stat stExe, stMod;
	if ( stat( str, &stExe ) == 0 && stat( str2, &stMod ) == 0 )
	{
		if ( difftime( stExe.st_mtime, stMod.st_mtime ) < 0 )
		{
			cmd( "set answer [tk_messageBox -parent . -title Warning -icon warning -type okcancel -default cancel -message \"Old executable file\" -detail \"The existing executable file is older than the last version of the model.\n\nPress 'OK' to continue anyway or 'Cancel' to return to LMM. Please recompile the model to avoid this message.\"]; if [ string equal $answer ok ] { set choice 1 } { set choice 2 }" );
			if ( choice == 2 )
				goto end_gdb;
		}
	}
	else
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Executable not found\" -detail \"Compile the model before running it in the [ string toupper $DbgExe ] debugger.\"" );
		goto end_gdb;
	}

	cmd( msg );					// if all ok, run debug command

	end_gdb:
	cmd( "cd \"$RootLsd\"" );
	choice = 0;
	goto loop;
}


if ( choice == 14 )
{
/* Create a new model/group */

cmd( "destroytop .mm" );	// close compilation results, if open

// prevent creating new groups in LSD directory
cmd( "if { [ string equal $groupdir [ pwd ] ] && [ file exists \"$groupdir/$LsdNew/groupinfo.txt\" ] } \
		{	set answer [ tk_messageBox -parent . -type okcancel -title Warning \
			-icon warning -default ok -message \"Invalid parent group\" \
			-detail \"Cannot create group/model in the Root group. Press 'OK' to change to the '$LsdNew' group before proceeding.\" ]; \
			if [ string equal $answer ok ] { \
				set groupdir \"$groupdir/$LsdNew\"; \
				set f [open \"$groupdir/groupinfo.txt\" r]; \
				set app \"[gets $f]\"; \
				close $f; \
				if [ string equal \"$modelgroup\" \"$rootname\" ] { \
					set modelgroup \"$app\" \
				} else { \
					set modelgroup \"$modelgroup/$app\" \
				}; \
				.f.hea.grp.dat conf -text \"$modelgroup\"; \
				set choice 1 \
			} else { \
				set choice 0 \
			} \
		}");		
if ( choice == 0 )
	goto loop;

cmd( "set temp 1" );

cmd( "newtop .a \"New Model\" { set choice 2 }" );

cmd( "frame .a.tit" );
cmd( "label .a.tit.l -text \"Current group:\"" );
cmd( "label .a.tit.n -fg red -text \"$modelgroup\"" );
cmd( "pack .a.tit.l .a.tit.n" );

cmd( "frame .a.f -relief groove -bd 2" );
cmd( "radiobutton .a.f.r1 -variable temp -value 1 -text \"Create a new model in the current group\" -anchor w" );
cmd( "radiobutton .a.f.r2 -variable temp -value 2 -text \"Create a new model in a new group\" -anchor w" );
cmd( "pack .a.f.r1 .a.f.r2 -anchor w" );

cmd( "pack .a.tit .a.f -padx 5 -pady 5" );

cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
cmd( "bind .a <KeyPress-Return> { .a.b.ok invoke }" );
cmd( "bind .a <Up> {.a.f.r1 invoke}" );
cmd( "bind .a <Down> {.a.f.r2 invoke}" );

cmd( "showtop .a" );

choice = 0;
while ( choice == 0 )
 Tcl_DoOneEvent( 0 );

cmd( "destroytop .a" );

//operation cancelled
if ( choice == 2)
 {
  choice = 0;
  goto loop;
 }

cmd( "set choice $temp" );
if ( choice == 2)
{
cmd( "set mname \"New group\"" );
cmd( "set mdir \"newgroup\"" );

cmd( "newtop .a \"New Group\" { set choice 2 }" );

cmd( "frame .a.tit" );
cmd( "label .a.tit.l -text \"Current group:\"" );
cmd( "label .a.tit.n -fg red -text \"$modelgroup\"" );
cmd( "pack .a.tit.l .a.tit.n" );

cmd( "frame .a.mname" );
cmd( "label .a.mname.l -text \"New group name\"" );
cmd( "entry .a.mname.e -width 25 -textvariable mname -justify center" );
cmd( "pack .a.mname.l .a.mname.e" );

cmd( "frame .a.mdir" );
cmd( "label .a.mdir.l -text \"New (non-existing) subdirectory name\"" );
cmd( "entry .a.mdir.e -width 35 -textvariable mdir -justify center" );
cmd( "pack .a.mdir.l .a.mdir.e" );

cmd( "frame .a.tdes" );
cmd( "label .a.tdes.l -text \"Group description\"" );
cmd( "set a [ list \"$fonttype\" $small_character ]" );
cmd( "text .a.tdes.e -undo 1 -width 60 -height 15 -font \"$a\"" );
cmd( "pack .a.tdes.l .a.tdes.e" );

cmd( "pack .a.tit .a.mname .a.mdir .a.tdes -padx 5 -pady 5" );

cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
cmd( "bind .a.mname.e <Return> {focus .a.mdir.e; .a.mdir.e selection range 0 end}" );
cmd( "bind .a.mdir.e <Return> {focus .a.tdes.e}" );

cmd( "showtop .a" );
cmd( ".a.mname.e selection range 0 end" );
cmd( "focus .a.mname.e" );

here_newgroup:

choice = 0;

while ( choice == 0 )
 Tcl_DoOneEvent( 0 );

cmd( "if {[string length $mdir] == 0 || [string length $mname]==0} {set choice 2}" );

//operation cancelled
if ( choice == 2)
 {cmd( "destroytop .a" );
  choice = 0;
  goto loop;
 }

cmd( "if {[llength [split $mdir]]>1} {set choice -1}" );
if ( choice == -1)
 {cmd( "tk_messageBox -parent .a -type ok -title Error -icon error -message \"Space in path\" -detail \"Directory name must not contain spaces, please try a new name.\"" );
  cmd( "focus .a.mdir.e" );
  cmd( ".a.mdir.e selection range 0 end" );
  goto here_newgroup;
 } 
//control for existing directory
cmd( "if {[file exists \"$groupdir/$mdir\"] == 1} {tk_messageBox -parent .a -type ok -title Error -icon error -message \"Cannot create directory\" -detail \"$groupdir/$mdir\\n\\nPossibly there is already such a directory, please try a new directory.\"; set choice 3}" );
if ( choice == 3)
{
  cmd( "focus .a.mdir.e" );
  cmd( ".a.mdir.e selection range 0 end" );
  goto here_newgroup;
}

cmd( "file mkdir \"$groupdir/$mdir\"" );
cmd( "cd \"$groupdir/$mdir\"" );
cmd( "set groupdir \"$groupdir/$mdir\"" );
cmd( "set f [open groupinfo.txt w]; puts -nonewline $f \"$mname\"; close $f" );
cmd( "set f [open description.txt w]; puts -nonewline $f \"[.a.tdes.e get 0.0 end]\"; close $f" );
cmd( "if [ string equal \"$modelgroup\" \"$rootname\" ] { set modelgroup \"$mname\" } { set modelgroup \"$modelgroup/$mname\" }" );

cmd( "destroytop .a" );
//end of creation of a new group
}
else
 cmd( "cd \"$groupdir\"" ); //if no group is created, move in the current group

//create a new model
cmd( "set mname \"New model\"" );
cmd( "set mver \"1.0\"" );
cmd( "set mdir \"newmodel\"" );


cmd( "newtop .a \"New Model\" { set choice 2 }" );

cmd( "frame .a.tit" );
cmd( "label .a.tit.l -text \"Current group:\"" );
cmd( "label .a.tit.n -fg red -text \"$modelgroup\"" );
cmd( "pack .a.tit.l .a.tit.n" );

cmd( "frame .a.mname" );
cmd( "label .a.mname.l -text \"New model name\"" );
cmd( "entry .a.mname.e -width 25 -textvariable mname -justify center" );
cmd( "pack .a.mname.l .a.mname.e" );

cmd( "frame .a.mver" );
cmd( "label .a.mver.l -text \"Version\"" );
cmd( "entry .a.mver.e -width 10 -textvariable mver -justify center" );
cmd( "pack .a.mver.l .a.mver.e" );

cmd( "frame .a.mdir" );
cmd( "label .a.mdir.l -text \"New (non-existing) subdirectory name\"" );
cmd( "entry .a.mdir.e -width 35 -textvariable mdir -justify center" );
cmd( "pack .a.mdir.l .a.mdir.e" );

cmd( "pack .a.tit .a.mname .a.mver .a.mdir -padx 5 -pady 5" );

cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
cmd( "bind .a.mname.e <Return> {focus .a.mver.e; .a.mver.e selection range 0 end}" );
cmd( "bind .a.mver.e <Return> {focus .a.mdir.e; .a.mdir.e selection range 0 end}" );
cmd( "bind .a.mdir.e <Return> {focus .a.b.ok}" );

cmd( "showtop .a" );
cmd( ".a.mname.e selection range 0 end" );
cmd( "focus .a.mname.e" );

loop_copy_new:

choice = 0;

while ( choice == 0 )
 Tcl_DoOneEvent( 0 );

cmd( "if {[string length $mdir] == 0 || [string length $mname]==0} {set choice 2}" );
//operation cancelled
if ( choice == 2)
 {cmd( "destroytop .a" );
  choice = 0;
  goto loop;
 }

cmd( "if {[llength [split $mdir]]>1} {set choice -1}" );
if ( choice == -1)
 {cmd( "tk_messageBox -parent .a -type ok -title Error -icon error -message \"Space in path\" -detail \"Directory name must not contain spaces, please try a new name.\"" );
  cmd( "focus .a.mdir.e" );
  cmd( ".a.mdir.e selection range 0 end" );
  goto loop_copy_new;
 } 

//control for existing directory
cmd( "if {[file exists \"$mdir\"] == 1} {tk_messageBox -parent .a -type ok -title Error -icon error -message \"Cannot create directory\" -detail \"$groupdir/$mdir\\n\\nPossibly there is already such a directory, please try a new directory.\"; set choice 3}" );
if ( choice == 3)
 {
  cmd( "focus .a.mdir.e" );
  cmd( ".a.mdir.e selection range 0 end" );
  goto loop_copy_new;
 } 

//control for an existing model with the same name AND same version
cmd( "set dir [glob -nocomplain *]" );
cmd( "set num [llength $dir]" );
strcpy(str, " ");
for ( i = 0; i<num; ++i )
 {
  cmd( "if {[file isdirectory [lindex $dir %d] ] == 1} {set curdir [lindex $dir %i]} {set curdir ___}", i, i );
  s = ( char * ) Tcl_GetVar( inter, "curdir", 0 );
  strncpy( str, s, 499 );
  if (strcmp(str,"___") && strcmp(str, "gnu")  && strcmp(str, "gnu64") && strcmp(str, "src") && strcmp(str, "Manual") && strcmp(str, "R") )
   {
    cmd( "set ex [file exists \"$curdir/modelinfo.txt\"]" );
    cmd( "if { $ex == 0 } {set choice 0} {set choice 1}" );
    if ( choice == 1 )
    {
      cmd( "set f [open \"$curdir/modelinfo.txt\" r]" );
      cmd( "set cname [gets $f]; set cver [gets $f];" );
      cmd( "close $f" );
    } 
    else
      cmd( "set cname $curdir; set cver \"0.1\"" );
    cmd( "set comp [string compare $cname $mname]" );
    cmd( "set comp1 [string compare $cver $mver]" );
    cmd( "if {$comp == 0 && $comp1 == 0} {set choice 3; set errdir $curdir}" );
    cmd( "if {$comp == 0 } {set choice 4; set errdir $curdir}" );

   }
 }
 
if ( choice == 3)
 {cmd( "tk_messageBox -parent .a -type ok -title Error -icon error -message \"Model already exists\" -detail \"Cannot create the new model '$mname' (ver. $mver) because it already exists (directory: $errdir).\"" );
  cmd( ".a.mname.e selection range 0 end" );
  cmd( "focus .a.mname.e" );
  goto loop_copy_new;
 } 
 
if ( choice == 4)
 {
 choice = 0;
 cmd( "set answer [tk_messageBox -parent .a -type okcancel -title Warning -icon warning -default cancel -message \"Model already exists\" -detail \"A model named '$mname' already exists (ver. $mver).\\n\\nIf you want the new model to inherit the same equations, data etc. of that model you should cancel this operation, and use the 'Save Model As...' command. Or press 'OK' to continue creating a new (empty) model '$mname'.\"]" );
  s = ( char * ) Tcl_GetVar( inter, "answer", 0 );

  cmd( "if {[string compare $answer ok] == 0} {set choice 1} {set choice 0}" );
  if ( choice == 0 )
   {cmd( "destroytop .a" );
    goto loop;
   } 
 } 
 
cmd( "destroytop .a" );

//here we are: create a new empty model

cmd( "set dirname $groupdir/$mdir" );
cmd( "set modeldir $groupdir/$mdir" );
cmd( "set modelname $mname" );
cmd( "set version $mver" );
cmd( ".f.hea.mod.dat conf -text \"$modelname\"" );
cmd( ".f.hea.ver.dat conf -text \"$version\"" );
cmd( ".f.hea.grp.dat conf -text \"$modelgroup\"" );

cmd( "file mkdir \"$dirname\"" );

if ( macro )
 cmd( "set fun_base \"fun_baseMacro.cpp\"" );
else 
 cmd( "set fun_base \"fun_baseC.cpp\"" );

//create the empty equation file
cmd( "file copy \"$RootLsd/$LsdSrc/$fun_base\" \"$modeldir/fun_$mdir.cpp\"" ); 

cmd( "cd \"$dirname\"" );

//create the model_options.txt file
check_option_files( );

//insert the modelinfo file, 
cmd( "set before [.f.t.t get 1.0 end]" );
cmd( "set f [open modelinfo.txt w]" );
cmd( "puts $f \"$modelname\"" );
cmd( "puts $f \"$version\"" );
cmd( "set frmt \"%d %%B, %%Y\"" );
cmd( "puts $f \"[clock format [clock seconds] -format \"$frmt\"]\"" );
cmd( "close $f" );

cmd( "cd \"$RootLsd\"" );

cmd( ".m.file entryconf 2 -state normal" );
cmd( ".m.file entryconf 3 -state normal" );
cmd( ".m.model entryconf 0 -state normal" );
cmd( ".m.model entryconf 1 -state normal" );
cmd( ".m.model entryconf 2 -state normal" );
cmd( ".m.model entryconf 3 -state normal" );
cmd( ".m.model entryconf 4 -state normal" );
cmd( ".m.model entryconf 6 -state normal" );
cmd( ".m.model entryconf 7 -state normal" );
cmd( ".m.model entryconf 8 -state normal" );
cmd( ".m.model entryconf 9 -state normal" );
cmd( ".m.model entryconf 10 -state normal" );
cmd( ".m.model entryconf 12 -state normal" );


cmd( "tk_messageBox -parent . -type ok -title \"Model Created\" -icon info -message \"Model '$mname' created\" -detail \"Version: $mver\nDirectory: $dirname\"" );

cmd( "set before [.f.t.t get 0.0 end]" ); //avoid to re-issue a warning for non saved files
choice=50;
goto loop;

}


/* open a text file*/

if ( choice == 15 || choice == 71 )
{
	if ( choice == 15 )
	{
		cmd( "set brr [ tk_getOpenFile -parent . -title \"Load Text File\" -initialdir $dirname ]" );
		cmd( "if { [string length $brr] == 0 } { set choice 0 } { set choice 1 }" );
		if ( choice == 0 )
			goto loop;
	}

	cmd( "if [ file exists \"$brr\" ] { set choice 1 } { set choice 0 }" );
	if ( choice == 0 )
		goto loop; 

	cmd( ".f.t.t delete 1.0 end" ); 
	cmd( "focus -force .f.t.t" );
	cmd( "set dirname [file dirname \"$brr\"]" ); 
	cmd( "set filename [file tail \"$brr\"]" ); 
	cmd( "set file [open \"$brr\" r]" ); 
	cmd( ".f.t.t insert end [read -nonewline $file]" ); 
	cmd( "close $file" ); 
	cmd( ".f.t.t edit reset" ); 
	cmd( ".f.t.t tag remove sel 1.0 end" ); 

	// handle the opening of files from the compilation error window
	cmd( "if { [ info exists errfil ] && [ string equal \"$errfil\" \"[ file normalize \"$dirname/$filename\" ]\" ] && [ info exists errlin ] && [ string is integer -strict $errlin ] } { \
			.f.t.t tag add sel $errlin.0 $errlin.end; \
			if { [ info exists errcol ] && $errcol != \"\" && [ string is integer -strict $errcol ] } { \
				.f.t.t see $errlin.$errcol; \
				.f.t.t mark set insert $errlin.$errcol \
			} else { \
				.f.t.t see $errlin.0; \
				.f.t.t mark set insert $errlin.0 \
			} \
		} else { \
			.f.t.t mark set insert 1.0 \
		}" );

	cmd( "set before [.f.t.t get 1.0 end]" ); 
	cmd( ".f.hea.file.dat conf -text \"$filename\"" );

	cmd( "set s [file extension \"$filename\"]" );
	s = ( char * ) Tcl_GetVar( inter, "s", 0 );
	if ( s[0] != '\0' )
	{
	  strncpy( str, s, 499 );
	  if ( ! strcmp( str, ".cpp" ) || ! strcmp( str, ".c" ) || ! strcmp( str, ".C" ) || ! strcmp( str, ".CPP" ) || ! strcmp( str, ".Cpp" ) || ! strcmp( str, ".c++" ) || ! strcmp( str, ".C++" ) || ! strcmp( str, ".h" ) || ! strcmp( str, ".H" ) || ! strcmp( str, ".hpp" ) || ! strcmp( str, ".Hpp" ) || ! strcmp( str, ".h++" ) || ! strcmp( str, ".H++" ) )
	  {
		cmd( ".f.t.t tag add bc \"1.0\"" );
		cmd( ".f.t.t tag add fc \"1.0\"" );

		sourcefile = 1;
		cmd( "savCurIni; savCurFin; updCurWnd" );	// save data for recolor
		color( shigh, 0, 0 );						// set color types (all text)
	  }
	  else
		sourcefile = 0;
	}

	cmd( "catch [unset -nocomplain ud]" );
	cmd( "catch [unset -nocomplain udi]" );
	cmd( "catch [unset -nocomplain rd]" );
	cmd( "catch [unset -nocomplain rdi]" );
	cmd( "lappend ud [.f.t.t get 0.0 end]" );
	cmd( "lappend udi [.f.t.t index insert]" );

	choice = 0;
	goto loop;
}


/*insert automatic tabs */

if ( choice == 16 )
{
	cmd( "set in [.f.t.t index insert]" );
	cmd( "scan $in %%d.%%d line col" );
	cmd( "set line [ expr $line - 1 ]" );
	cmd( "set s [ .f.t.t get $line.0 $line.end ]" );
	
	s = ( char * ) Tcl_GetVar( inter, "s", 0 );
	for ( i = 0; s[ i ] == ' ' || s[ i ] == '\t'; ++i )
	  str[ i ] = s[ i ];
  
	if ( i > 0 )
	{
		str[ i ] = '\0';
		cmd( ".f.t.t insert insert \"%s\"", str );
	}
	
	choice = 0;
	goto loop;
}


/* find matching parenthesis */

if ( choice == 17)
{
	cmd( "set sym [.f.t.t get insert]" );
	cmd( "if {[string compare $sym \"\\{\"]!=0} {if {[string compare $sym \"\\}\"]!=0} {set choice 0} {set num -1; set direction -backwards; set fsign +; set sign \"\"; set terminal \"1.0\"}} {set num 1; set direction -forwards; set fsign -; set sign +; set terminal end}" );

	if ( choice == 0 )
		goto loop;
	cmd( "set cur [.f.t.t index insert]" );
	cmd( ".f.t.t tag add sel $cur \"$cur + 1char\"" );
	if ( num > 0 )
		cmd( "set cur [.f.t.t index \"insert + 1 char\"]" );
  
	while ( num != 0 && choice != 0 )
	{
		cmd( "set a [.f.t.t search $direction \"\\{\" $cur $terminal]" );
		cmd( "if {$a==\"\"} {set a [.f.t.t index $terminal]}" );
		cmd( "set b [.f.t.t search $direction \"\\}\" $cur $terminal]" );
		cmd( "if {$b==\"\"} {set b [.f.t.t index $terminal]}" );
		cmd( "if {$a==$b} {set choice 0}" );
		if ( choice == 0 )
			goto loop;
		
		if ( num > 0 )
			cmd( "if {[.f.t.t compare $a < $b]} {set num [expr $num + 1]; set cur [.f.t.t index \"$a+1char\"]} {set num [expr $num - 1]; set cur [.f.t.t index \"$b+1char\"]}" );
		else
			cmd( "if {[.f.t.t compare $a > $b]} {set num [expr $num + 1]; set cur [.f.t.t index $a]} {set num [expr $num - 1]; set cur [.f.t.t index $b]}" );


	}
	
	choice = 0;

	cmd( " if { [string compare $sign \"+\"]==0 } {.f.t.t tag add sel \"$cur - 1char\" $cur ; set num 1} {.f.t.t tag add sel $cur \"$cur + 1char\"; set num 0}" );

	cmd( ".f.t.t see $cur" );
	goto loop;
}


// reset colors after a sign for coloring

if ( choice == 23 )
{ 
	if ( sourcefile == 0 )
	{
		choice = 0;
		goto loop;
	}
	else
	{	
		choice = 0;
		// text window not ready?
		if ( Tcl_GetVar( inter, "curPosIni", 0 ) == NULL || Tcl_GetVar( inter, "curPosFin", 0 ) == NULL )			
			goto loop;
		// check if inside or close to multi-line comment and enlarge region appropriately
		cmd( "if { [ lsearch -exact [ .f.t.t tag names $curPosIni ] comment1 ] != -1 } { \
					set curPosFin [ .f.t.t search */ $curPosIni end ]; \
					set newPosIni [ .f.t.t search -backwards /* $curPosIni 1.0 ]; \
					set curPosIni $newPosIni \
				} else { \
					if { [ .f.t.t search -backwards */ $curPosIni \"$curPosIni linestart\" ] != \"\" } { \
						set comIni [ .f.t.t search -backwards /* $curPosIni ]; \
						if { $comIni != \"\" } { \
							set curPosIni $comIni \
						} \
					}; \
					if { [ .f.t.t search /* $curPosFin \"$curPosFin lineend\" ] != \"\" } { \
						set comFin [.f.t.t search */ $curPosFin]; \
						if { $comFin != \"\" } { \
							set curPosFin $comFin \
						} \
					} \
				}" );
		// find the range of lines to reeval the coloring
		char *curPosIni=( char* ) Tcl_GetVar( inter,"curPosIni", 0 ); // position before insertion
		char *curPosFin=( char* ) Tcl_GetVar( inter,"curPosFin", 0 ); // position after insertion
		char *curSelIni=( char* ) Tcl_GetVar( inter,"curSelIni", 0 ); // selection before insertion
		char *curSelFin=( char* ) Tcl_GetVar( inter,"curSelFin", 0 ); // selection after insertion
		// collect all selection positions, before and after change
		float curPos[ 6 ];
		int nVal = 0;
		i = sscanf(curPosIni, "%f", &curPos[nVal]);
		nVal += i < 0 ? 0 : i;
		i = sscanf(curPosFin, "%f", &curPos[nVal]);
		nVal += i < 0 ? 0 : i;
		i = sscanf(curSelIni, "%f %f", &curPos[nVal], &curPos[nVal + 1]);
		nVal += i < 0 ? 0 : i;
		i = sscanf(curSelFin, "%f %f", &curPos[nVal], &curPos[nVal + 1]);
		nVal += i < 0 ? 0 : i;
		// find previous and next lines to select (worst case scenario)
		long prevLin = (long)floor(curPos[0]);
		long nextLin = (long)floor(curPos[0]) + 1;
		for ( i = 1; i < nVal; ++i )
		{
			if ( curPos[ i ] < prevLin ) 
				prevLin = ( long ) floor( curPos[ i ] );
			if ( curPos[ i ] + 1 > nextLin ) 
				nextLin = ( long ) floor( curPos[ i ] ) + 1;
		}
		
		color( shigh, prevLin, nextLin );
	}
	goto loop;

}


// MACRO VERSION
// Insert a LSD macro, to be used in LSD equations' code
if ( choice == 28 && macro )
{
	cmd( "set res 26" );

	cmd( "newtop .a \"Insert LSD macro\" { set choice 2 }" );

	cmd( "label .a.tit -text \"Available LSD macros\"" );

	cmd( "frame .a.r -bd 2 -relief groove" );
	cmd( "radiobutton .a.r.equ -text \"EQUATION - insert a new LSD equation\" -underline 0 -variable res -value 25" );
	cmd( "radiobutton .a.r.cal -text \"V(...) - request the value of a variable\" -underline 0 -variable res -value 26" );
	cmd( "radiobutton .a.r.for -text \"CYCLE - insert a cycle over a group of objects\" -underline 0 -variable res -value 27" );
	cmd( "radiobutton .a.r.sum -text \"SUM - compute the sum of a variable over a set of objects\" -underline 1 -variable res -value 56" );
	cmd( "radiobutton .a.r.incr -text \"INCR - increment the value of a parameter\" -underline 0 -variable res -value 40" );
	cmd( "radiobutton .a.r.mult -text \"MULT - multiply the value of a parameter\" -underline 0 -variable res -value 45" );
	cmd( "radiobutton .a.r.sear -text \"SEARCH - search the first instance an object type\" -underline 2 -variable res -value 55" );
	cmd( "radiobutton .a.r.scnd -text \"SEARCH_CND - conditional search a specific object\" -underline 0 -variable res -value 30" );
	cmd( "radiobutton .a.r.lqs -text \"SORT - sort a group of objects\" -underline 3 -variable res -value 31" );
	cmd( "radiobutton .a.r.rndo -text \"RNDDRAW - draw an object\" -underline 1 -variable res -value 54" );
	cmd( "radiobutton .a.r.wri -text \"WRITE - overwrite a variable or parameter with a new value\" -underline 0 -variable res -value 29" );
	cmd( "radiobutton .a.r.addo -text \"ADDOBJ - add a new object\" -underline 3 -variable res -value 52" );
	cmd( "radiobutton .a.r.delo -text \"DELETE - delete an object\" -underline 0 -variable res -value 53" );
	cmd( "radiobutton .a.r.net -text \"Network macros\" -underline 6 -variable res -value 72" );
	cmd( "radiobutton .a.r.math -text \"Mathematical functions\" -underline 12 -variable res -value 51" );

	cmd( "pack .a.r.equ .a.r.cal .a.r.for .a.r.sum .a.r.incr .a.r.mult .a.r.sear .a.r.scnd .a.r.lqs .a.r.rndo .a.r.wri .a.r.addo .a.r.delo .a.r.net .a.r.math -anchor w" );
	cmd( "pack .a.tit .a.r -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LMM.html#LsdScript } { set choice 2 }" );

	cmd( "bind .a <KeyPress-e> {.a.r.equ invoke; set choice 1}; bind .a <KeyPress-E> {.a.r.equ invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-v> {.a.r.cal invoke; set choice 1}; bind .a <KeyPress-V> {.a.r.cal invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-u> {.a.r.sum invoke; set choice 1}; bind .a <KeyPress-U> {.a.r.sum invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-s> {.a.r.scnd invoke; set choice 1}; bind .a <KeyPress-S> {.a.r.scnd invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-a> {.a.r.sear invoke; set choice 1}; bind .a <KeyPress-A> {.a.r.sear invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-w> {.a.r.wri invoke; set choice 1}; bind .a <KeyPress-W> {.a.r.wri invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-c> {.a.r.for invoke; set choice 1}; bind .a <KeyPress-C> {.a.r.for invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-t> {.a.r.lqs invoke; set choice 1}; bind .a <KeyPress-T> {.a.r.lqs invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-o> {.a.r.addo invoke; set choice 1}; bind .a <KeyPress-O> {.a.r.addo invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-d> {.a.r.delo invoke; set choice 1}; bind .a <KeyPress-D> {.a.r.delo invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-i> {.a.r.incr invoke; set choice 1}; bind .a <KeyPress-I> {.a.r.incr invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-m> {.a.r.mult invoke; set choice 1}; bind .a <KeyPress-M> {.a.r.mult invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-h> {.a.r.math invoke; set choice 1}; bind .a <KeyPress-H> {.a.r.math invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-k> {.a.r.net invoke; set choice 1}; bind .a <KeyPress-K> {.a.r.net invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-n> {.a.r.rndo invoke; set choice 1}; bind .a <KeyPress-N> {.a.r.rndo invoke; set choice 1}" );
	cmd( "bind .a <Return> { .a.b.ok invoke }" );

	cmd( "showtop .a" );
	cmd( "focus .a.r.cal" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{ 
		choice = 0;
		goto loop;
	}

	cmd( "set choice $res" );
	goto loop;
}


// C++ VERSION
// Insert a LSD script, to be used in LSD equations' code
if ( choice == 28 && ! macro )
{
	cmd( "set res 26" );

	cmd( "newtop .a \"Insert LSD Script\" { set choice 2 }" );

	cmd( "label .a.tit -text \"Available LSD scripts\" -justify center" );

	cmd( "frame .a.r -bd 2 -relief groove" );
	cmd( "radiobutton .a.r.equ -text \"LSD equation/function - insert a new LSD equation\" -underline 0 -variable res -value 25" );
	cmd( "radiobutton .a.r.cal -text \"cal - request the value of a variable\" -underline 0 -variable res -value 26" );
	cmd( "radiobutton .a.r.for -text \"for - insert a cycle over a group of objects\" -underline 0 -variable res -value 27" );
	cmd( "radiobutton .a.r.sum -text \"sum - compute the sum of a variable over a set of objects\" -underline 1 -variable res -value 56" );
	cmd( "radiobutton .a.r.incr -text \"increment - increment the value of a parameter\" -underline 0 -variable res -value 40" );
	cmd( "radiobutton .a.r.mult -text \"multiply - multiply the value of a parameter\" -underline 0 -variable res -value 45" );
	cmd( "radiobutton .a.r.sear -text \"search - search the first instance an object type\" -underline 3 -variable res -value 55" );
	cmd( "radiobutton .a.r.scnd -text \"search_var_cond - conditional search a specific object\" -underline 0 -variable res -value 30" );
	cmd( "radiobutton .a.r.lqs -text \"lsdqsort - sort a group of objects\" -underline 7 -variable res -value 31" );
	cmd( "radiobutton .a.r.rndo -text \"draw_rnd - draw an object\" -underline 6 -variable res -value 54" );
	cmd( "radiobutton .a.r.wri -text \"write - overwrite a variable or parameter with a new value\" -underline 0 -variable res -value 29" );
	cmd( "radiobutton .a.r.addo -text \"add_n_objects2 - add new objects\" -underline 0 -variable res -value 52" );
	cmd( "radiobutton .a.r.delo -text \"delete_obj - delete an object\" -underline 0 -variable res -value 53" );
	cmd( "radiobutton .a.r.math -text \"Insert a mathematical function\" -underline 12 -variable res -value 51" );

	cmd( "pack .a.r.equ .a.r.cal .a.r.for .a.r.sum .a.r.incr .a.r.mult .a.r.sear .a.r.scnd .a.r.lqs .a.r.rndo .a.r.wri .a.r.addo .a.r.delo .a.r.math -anchor w" );
	cmd( "pack .a.tit .a.r -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LMM.html#LsdScript } { set choice 2 }" );

	cmd( "bind .a <KeyPress-e> {.a.r.equ invoke; set choice 1}; bind .a <KeyPress-E> {.a.r.equ invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-c> {.a.r.cal invoke; set choice 1}; bind .a <KeyPress-C> {.a.r.cal invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-u> {.a.r.sum invoke; set choice 1}; bind .a <KeyPress-U> {.a.r.sum invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-s> {.a.r.scnd invoke; set choice 1}; bind .a <KeyPress-S> {.a.r.scnd invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-r> {.a.r.sear invoke; set choice 1}; bind .a <KeyPress-R> {.a.r.sear invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-w> {.a.r.wri invoke; set choice 1}; bind .a <KeyPress-W> {.a.r.wri invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-f> {.a.r.for invoke; set choice 1}; bind .a <KeyPress-F> {.a.r.for invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-t> {.a.r.lqs invoke; set choice 1}; bind .a <KeyPress-T> {.a.r.lqs invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-a> {.a.r.addo invoke; set choice 1}; bind .a <KeyPress-A> {.a.r.addo invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-d> {.a.r.delo invoke; set choice 1}; bind .a <KeyPress-D> {.a.r.delo invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-i> {.a.r.incr invoke; set choice 1}; bind .a <KeyPress-I> {.a.r.incr invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-m> {.a.r.mult invoke; set choice 1}; bind .a <KeyPress-M> {.a.r.mult invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-h> {.a.r.math invoke; set choice 1}; bind .a <KeyPress-H> {.a.r.math invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-n> {.a.r.rndo invoke; set choice 1}; bind .a <KeyPress-N> {.a.r.rndo invoke; set choice 1}" );
	cmd( "bind .a <Return> { .a.b.ok invoke }" );

	cmd( "showtop .a" );
	cmd( "focus .a.r.cal" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}

	cmd( "set choice $res" );
	goto loop;
}


// insert a math function
if ( choice == 51 )
{
	cmd( "set value1 \"0\"" ); 
	cmd( "set value2 \"1\"" ); 
	cmd( "set res 9" ); 
	cmd( "set str {uniform($value1, $value2)}" );

	cmd( "newtop .a \"Math Functions\" { set choice 2 }" );

	cmd( "frame .a.e" );
	cmd( "label .a.e.l -text \"Function parameters\"" );

	cmd( "frame .a.e.e -bd 2 -relief groove" );

	cmd( "frame .a.e.e.e1" );
	cmd( "label .a.e.e.e1.l -text Minimum" );
	cmd( "entry .a.e.e.e1.e -width 20 -justify center -textvariable value1" );
	cmd( "bind .a.e.e.e1.e <Return> { if [ string equal [ .a.e.e.e2.e cget -state ] normal ] { focus .a.e.e.e2.e; .a.e.e.e2.e selection range 0 end } { .a.b.ok invoke } }" );
	cmd( "pack .a.e.e.e1.l .a.e.e.e1.e" );

	cmd( "frame .a.e.e.e2" );
	cmd( "label .a.e.e.e2.l -text Maximum" );
	cmd( "entry .a.e.e.e2.e -width 20 -justify center -textvariable value2" );
	cmd( "bind .a.e.e.e2.e <Return> { .a.b.ok invoke }" );
	cmd( "pack .a.e.e.e2.l .a.e.e.e2.e" );

	cmd( "pack .a.e.e.e1 .a.e.e.e2 -padx 5 -pady 5" );

	cmd( "pack .a.e.l .a.e.e" );

	cmd( "frame .a.f -bd 2 -relief groove" );
	cmd( "radiobutton .a.f.r1 -text \"Square root\" -variable res -value 1 -command {.a.e.e.e1.l conf -text Value; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {sqrt($value1)} }" );
	cmd( "radiobutton .a.f.r2 -text \"Power\" -variable res -value 2 -command {.a.e.e.e1.l conf -text \"Base\"; .a.e.e.e2.l conf -text \"Exponent\"; .a.e.e.e2.e conf -state normal; set str {pow($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r3 -text \"Exponential\" -variable res -value 3 -command {.a.e.e.e1.l conf -text Value; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {exp($value1)} }" );
	cmd( "radiobutton .a.f.r4 -text \"Logarithm\" -variable res -value 4 -command {.a.e.e.e1.l conf -text Value; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {log($value1)} }" );
	cmd( "radiobutton .a.f.r5 -text \"Absolute value\" -variable res -value 5 -command {.a.e.e.e1.l conf -text Value; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {abs($value1)} }" );
	cmd( "radiobutton .a.f.r6 -text \"Minimum value\" -variable res -value 6 -command {.a.e.e.e1.l conf -text \"Value 1\"; .a.e.e.e2.l conf -text \"Value 2\"; .a.e.e.e2.e conf -state normal; set str {min($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r7 -text \"Maximum value\" -variable res -value 7 -command {.a.e.e.e1.l conf -text \"Value 1\"; .a.e.e.e2.l conf -text \"Value 2\"; .a.e.e.e2.e conf -state normal; set str {max($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r8 -text \"Round to closest integer\" -variable res -value 8 -command {.a.e.e.e1.l conf -text Value; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {round($value1)} }" );
	cmd( "radiobutton .a.f.r9 -text \"Uniform random draw\" -variable res -value 9 -command {.a.e.e.e1.l conf -text Minimum; .a.e.e.e2.l conf -text Maximum; .a.e.e.e2.e conf -state normal; set str {uniform($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r10 -text \"Integer uniform random draw\" -variable res -value 10 -command {.a.e.e.e1.l conf -text Minimum; .a.e.e.e2.l conf -text Maximum; .a.e.e.e2.e conf -state normal; set str {uniform_int($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r11 -text \"Bernoulli random draw\" -variable res -value 11 -command {.a.e.e.e1.l conf -text Probability; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {bernoulli($value1)} }" );
	cmd( "radiobutton .a.f.r12 -text \"Poisson random draw\" -variable res -value 12 -command {.a.e.e.e1.l conf -text Mean; .a.e.e.e2.l conf -text (unused); .a.e.e.e2.e conf -state disabled; set str {poisson($value1)} }" );
	cmd( "radiobutton .a.f.r13 -text \"Normal random draw\" -variable res -value 13 -command {.a.e.e.e1.l conf -text Mean; .a.e.e.e2.l conf -text \"Std. deviation\"; .a.e.e.e2.e conf -state normal; set str {norm($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r14 -text \"Lognormal random draw\" -variable res -value 14 -command {.a.e.e.e1.l conf -text Mean; .a.e.e.e2.l conf -text \"Std. deviation\"; .a.e.e.e2.e conf -state normal; set str {lnorm($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r15 -text \"Beta random draw\" -variable res -value 15 -command {.a.e.e.e1.l conf -text Alpha; .a.e.e.e2.l conf -text Beta; .a.e.e.e2.e conf -state normal; set str {beta($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r16 -text \"Gamma random draw\" -variable res -value 16 -command {.a.e.e.e1.l conf -text Alpha; .a.e.e.e2.l conf -text Beta; .a.e.e.e2.e conf -state normal; set str {gamma($value1, $value2)} }" );
	cmd( "radiobutton .a.f.r17 -text \"Pareto random draw\" -variable res -value 17 -command {.a.e.e.e1.l conf -text Mean; .a.e.e.e2.l conf -text Alpha; .a.e.e.e2.e conf -state normal; set str {pareto($value1, $value2)} }" );

	cmd( "pack .a.f.r1 .a.f.r2 .a.f.r3 .a.f.r4 .a.f.r5 .a.f.r6 .a.f.r7 .a.f.r8 .a.f.r9 .a.f.r10 .a.f.r11 .a.f.r12 .a.f.r13 .a.f.r14 .a.f.r15 .a.f.r16 .a.f.r17 -anchor w" );

	cmd( "label .a.more -text \"See Help for more functions/distributions\"" );
	
	cmd( "pack .a.e .a.f .a.more -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LSD_macros.html#Math } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.e.e.e1.e" );
	cmd( ".a.e.e.e1.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{ 
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( ".f.t.t insert insert [subst $str] }" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}

// insert a LSD equation
if ( choice == 25 )
{
	cmd( "set v_label \"\"" );
	cmd( "set isfun 0" );

	cmd( "newtop .a \"Insert Equation\" { set choice 2 }" );

	cmd( "frame .a.label" );
	cmd( "label .a.label.l -text \"Variable/function name\"" );
	cmd( "entry .a.label.n -width 20 -textvariable v_label -justify center" );
	cmd( "pack .a.label.l .a.label.n" );

	cmd( "bind .a.label.n <Return> { focus .a.b.ok }" );
	cmd( "pack .a.label -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LSD_macros.html#EQUATION } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.label.n selection range 0 end" );
	cmd( "focus .a.label.n" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}

	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( ".f.t.t insert insert \"EQUATION(\\\"$v_label\\\")\\n\"" );
		cmd( ".f.t.t insert insert \"/*\\nComment\\n*/\\n\\n\"" );
		cmd( ".f.t.t insert insert \"RESULT( )\\n\"" );
		cmd( ".f.t.t mark set insert \"$a + 2 line\"" );
		cmd( ".f.t.t tag add sel insert \"insert + 7 char\"" );
	}
	else
	{
		cmd( ".f.t.t insert insert \"if (! strcmp(label,\\\"$v_label\\\") )\\n\"" );
		cmd( ".f.t.t insert insert \"{\\n\"" );
		cmd( ".f.t.t insert insert \"/*\\nComment\\n*/\\n\"" );
		cmd( ".f.t.t insert insert \"\\n\"" );
		cmd( ".f.t.t insert insert \"res=;\\n\"" );
		cmd( ".f.t.t insert insert \"goto end;\\n}\\n\"" );
		cmd( ".f.t.t mark set insert \"$a + 3 line\"" );
		cmd( ".f.t.t tag add sel insert \"insert + 7 char\"" );	
	}

	v_counter = 0;
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// insert a v[0]=p->cal("Var",0)
if ( choice == 26 )
{
	cmd( "set v_num %d", v_counter );
	cmd( "set v_label \"\"" );
	cmd( "set v_lag 0" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'V(...)' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'cal'\" { set choice 2 }" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Number v\\\[x\\] to assign to\"" );
	cmd( "entry .a.v.e -width 2 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter to retrieve\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.l.e; .a.l.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Lag to use\"" );
	cmd( "entry .a.l.e -width 2 -textvariable v_lag -justify center" );
	cmd( "bind .a.l.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.l.l .a.l.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.v .a.n .a.l .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#V } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_num!=\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"v\\\[$v_num\\] = \"}" );

		cmd( "if {$v_lag==0 && $v_obj==\"p\"} { .f.t.t insert insert \"V(\\\"$v_label\\\")\"}" );
		cmd( "if {$v_lag!=0 && $v_obj==\"p\"} { .f.t.t insert insert \"VL(\\\"$v_label\\\", $v_lag)\"}" );
		cmd( "if {$v_lag==0 && $v_obj!=\"p\"} { .f.t.t insert insert \"VS($v_obj, \\\"$v_label\\\")\"}" );
		cmd( "if {$v_lag!=0 && $v_obj!=\"p\" && [string is integer -strict $v_lag]} { .f.t.t insert insert \"VLS($v_obj, \\\"$v_label\\\", $v_lag)\"}" );
	}
	else
	{
		cmd( "if {$v_num==\"\" && [string is integer -strict $v_num] && [string is integer -strict $v_lag]} {.f.t.t insert insert \"$v_obj->cal(\\\"$v_label\\\", $v_lag)\"} {.f.t.t insert insert \"v\\\[$v_num\\] = $v_obj->cal(\\\"$v_label\\\", $v_lag)\"; incr v_num}" );
	}

	cmd( "if {$v_num!=\"\"} {.f.t.t insert insert \";\\n\"}" );

	cmd( "if {$v_num==\"\"} { set num -1} {set num $v_num}" );
	if (num!=-1)
	 v_counter=++num;
 
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a cycle for (cur=p->search("Label"); cur!=NULL; cur=go_brother(cur) )
if ( choice == 27 )
{
	cmd( "set v_label \"\"" );
	cmd( "set v_obj cur" );
	cmd( "set v_par p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'CYCLE' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'for' Cycle\" { set choice 2 }" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Object to cycle through\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.c.e; .a.c.e selection range 0 end}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Cycling pointer\"" );
	cmd( "entry .a.c.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.c.e <Return> {focus .a.p.e; .a.p.e selection range 0 end}" );
	cmd( "pack .a.c.l .a.c.e" );

	cmd( "frame .a.p" );
	cmd( "label .a.p.l -text \"Parent object\"" );
	cmd( "entry .a.p.e -width 25 -textvariable v_par -justify center" );
	cmd( "bind .a.p.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.p.l .a.p.e" );

	cmd( "pack .a.o .a.c .a.p -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#CYCLE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.o.e" );
	cmd( ".a.o.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if { $v_par == \"p\" } { .f.t.t insert insert \"CYCLE($v_obj, \\\"$v_label\\\")\\n\" } { .f.t.t insert insert \"CYCLES($v_par, $v_obj, \\\"$v_label\\\")\\n\" }" );

		cmd( "set in [ .f.t.t index insert ]" );
		cmd( "scan $in %%d.%%d line col" );
		cmd( "set line [ expr $line -1 ]" );
		cmd( "set s [ .f.t.t get $line.0 $line.end ]" );
		s = ( char * ) Tcl_GetVar( inter, "s", 0 );
		for ( i = 0; s[ i ] == ' ' || s[ i ] == '\t'; ++i )
			str[ i ] = s[ i ];
		if ( i > 0 )
		{
			str[ i ] = '\0';
			cmd( ".f.t.t insert insert \"%s\"", str );
			cmd( ".f.t.t insert insert \"{\\n\"" );
			cmd( ".f.t.t insert insert \"%s\\t\"", str );
			cmd( "set b [.f.t.t index insert]" );
			cmd( ".f.t.t insert insert \"\\n%s\"", str );
		}
		else
		{
			cmd( ".f.t.t insert insert \"{\\n\"" );
			cmd( ".f.t.t insert insert \"\\t\"" );
			cmd( "set b [.f.t.t index insert]" );
			cmd( ".f.t.t insert insert \"\\n\"" );
		}

		cmd( ".f.t.t insert insert \"}\\n\"" );
		cmd( ".f.t.t mark set insert \"$b\"" );
	}
	else
	{
		cmd( ".f.t.t insert insert \"for ($v_obj = $v_par->search(\\\"$v_label\\\"); $v_obj != NULL; $v_obj = go_brother($v_obj) )\\n\"" );
		cmd( ".f.t.t insert insert \"{\\n\\t\\n}\\n\"" );
		cmd( ".f.t.t mark set insert \"$a + 2 line + 2 char\"" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a v[0]=p->increment("Var",value)
if ( choice == 40 )
{
	cmd( "set v_num %d", v_counter );
	cmd( "set v_label \"\"" );
	cmd( "set v_val 1" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'INCR' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'increment' Command\" { set choice 2 }" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Number v\\\[x\\] to assign the result after increment\"" );
	cmd( "entry .a.v.e -width 2 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable to increase\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.a.e; .a.a.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.a" );
	cmd( "label .a.a.l -text \"Value to add\"" );
	cmd( "entry .a.a.e -width 15 -textvariable v_val -justify center" );
	cmd( "bind .a.a.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.a.l .a.a.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.v .a.n .a.a .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#INCR } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_num!=\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"v\\\[$v_num\\] = \"}" );
		cmd( "if {$v_obj!=\"p\"} {.f.t.t insert insert \"INCRS($v_obj, \\\"$v_label\\\", $v_val)\"} {.f.t.t insert insert \"INCR(\\\"$v_label\\\", $v_val)\"}" );
	}
	else
	{
		cmd( "if {$v_num==\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"$v_obj->increment(\\\"$v_label\\\", $v_val)\"} {.f.t.t insert insert \"v\\\[$v_num\\] = $v_obj->increment(\\\"$v_label\\\", $v_val)\";incr v_num}" );
	}

	cmd( "if {$v_num!=\"\"} {.f.t.t insert insert \";\\n\"}" );

	cmd( "if {$v_num==\"\" } {set num -1} {set num $v_num}" );
	if (num!=-1)
	 v_counter=++num;

	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a v[0]=p->multiply("Var",0)
if ( choice == 45 )
{
	cmd( "set v_num %d", v_counter );
	cmd( "set v_label \"\"" );
	cmd( "set v_val 1" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'MULT' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'multiply' Command\" { set choice 2 }" );
		
	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Number v\\\[x\\] to assign the result after multiplication\"" );
	cmd( "entry .a.v.e -width 2 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable to multiply\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.a.e; .a.a.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.a" );
	cmd( "label .a.a.l -text \"Value to multiply\"" );
	cmd( "entry .a.a.e -width 15 -textvariable v_val -justify center" );
	cmd( "bind .a.a.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.a.l .a.a.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.v .a.n .a.a .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#MULT } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_num!=\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"v\\\[$v_num\\] = \"}" );
		cmd( "if {$v_obj!=\"p\"} {.f.t.t insert insert \"MULTS($v_obj, \\\"$v_label\\\", $v_val)\"} {.f.t.t insert insert \"MULT(\\\"$v_label\\\", $v_val)\"}" );
	}
	else
	{
		cmd( "if {$v_num==\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"$v_obj->multiply(\\\"$v_label\\\", $v_val)\"} {.f.t.t insert insert \"v\\\[$v_num\\] = $v_obj->multiply(\\\"$v_label\\\", $v_val)\";incr v_num}" );
	}

	cmd( "if {$v_num!=\"\"} {.f.t.t insert insert \";\\n\"}" );

	cmd( "if {$v_num==\"\" } {set num -1} {set num $v_num}" );
	if (num!=-1)
	 v_counter=++num;

	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a p->write("Var",0,0)
if ( choice == 29 )
{
	cmd( "set v_num 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_lag 0" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'WRITE' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'write' Command\" { set choice 2 }" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Value to write\"" );
	cmd( "entry .a.v.e -width 15 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter to write\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.l.e; .a.l.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Time step appearing as latest computation\"" );
	cmd( "entry .a.l.e -width 3 -textvariable v_lag -justify center" );
	cmd( "bind .a.l.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.l.l .a.l.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.v .a.n .a.l .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#WRITE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_obj == \"p\" && $v_lag == 0} { .f.t.t insert insert \"WRITE(\\\"$v_label\\\", $v_num);\\n\"}" );
		cmd( "if {$v_obj == \"p\" && [string is integer -strict $v_lag] && $v_lag != 0} { .f.t.t insert insert \"WRITEL(\\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
		cmd( "if {$v_obj != \"p\" && $v_lag == 0} { .f.t.t insert insert \"WRITES($v_obj, \\\"$v_label\\\", $v_num);\\n\"}" );
		cmd( "if {$v_obj != \"p\" && [string is integer -strict $v_lag] && $v_lag != 0} { .f.t.t insert insert \"WRITELS($v_obj, \\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
	}
	else
	{
		cmd( "if {[string is integer -strict $v_lag]} {.f.t.t insert insert \"$v_obj->write(\\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a cur=p->search_var_cond("Var",1,0)
if ( choice == 30 )
{
	cmd( "set v_obj0 cur" );
	cmd( "set v_num 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_lag 0" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'SEARCH_CND' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'search_var_cond' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the object found\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.v.e; .a.v.e selection range 0 end}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Value to search for\"" );
	cmd( "entry .a.v.e -width 15 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter to search\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.l.e; .a.l.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Lag to use\"" );
	cmd( "entry .a.l.e -width 2 -textvariable v_lag -justify center" );
	cmd( "bind .a.l.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.l.l .a.l.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Object from which to search\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.v .a.n .a.l .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SEARCH_CND } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_obj == \"p\" && $v_lag == 0} { .f.t.t insert insert \"$v_obj0 = SEARCH_CND(\\\"$v_label\\\", $v_num);\\n\"}" );
		cmd( "if {$v_obj == \"p\" && [string is integer -strict $v_lag] && $v_lag != 0} { .f.t.t insert insert \"$v_obj0 = SEARCH_CNDL(\\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
		cmd( "if {$v_obj != \"p\" && $v_lag == 0} { .f.t.t insert insert \"$v_obj0 = SEARCH_CNDS($v_obj, \\\"$v_label\\\", $v_num);\\n\"}" );
		cmd( "if {$v_obj != \"p\" && [string is integer -strict $v_lag] && $v_lag != 0} { .f.t.t insert insert \"$v_obj0 = SEARCH_CNDLS($v_obj, \\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
	}
	else
	{
		cmd( "if {[string is integer -strict $v_lag]} {.f.t.t insert insert \"$v_obj0 = $v_obj->search_var_cond(\\\"$v_label\\\", $v_num, $v_lag);\\n\"}" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a p->lsdqsort("Object", "Variable", "DIRECTION")
if ( choice == 31 )
{
	cmd( "set v_obj p" );
	cmd( "set v_obj0 \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_direction 1" );

	if ( macro )
		cmd( "newtop .a \"Insert 'SORT' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'lsdqsort' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Object to be sorted\"" );
	cmd( "entry .a.d.e -width 25 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter for sorting\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.s.u}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.s" );
	cmd( "label .a.s.l -text \"Sorting direction\"" );
	cmd( "radiobutton .a.s.u -text Increasing -variable v_direction -value 1" );
	cmd( "radiobutton .a.s.d -text Decreasing -variable v_direction -value 2" );
	cmd( "bind .a.s <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.s.l .a.s.u .a.s.d" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.s .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SORT } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}

	cmd( "set choice $v_direction" );
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		if ( choice == 1)
		  cmd( "set direction \"UP\"" );
		else
		  cmd( "set direction \"DOWN\"" );

		cmd( "if {$v_obj == \"p\"} {.f.t.t insert insert \"SORT(\\\"$v_obj0\\\", \\\"$v_label\\\", \\\"$direction\\\");\\n\"}" );
		cmd( "if {$v_obj != \"p\"} {.f.t.t insert insert \"SORTS($v_obj, \\\"$v_obj0\\\", \\\"$v_label\\\", \\\"$direction\\\");\\n\"}" );
	}
	else
	{
		if ( choice == 1 )
		  cmd( ".f.t.t insert insert \"$v_obj->lsdqsort(\\\"$v_obj0\\\", \\\"$v_label\\\", \\\"UP\\\");\\n\"" );
		else
		  cmd( ".f.t.t insert insert \"$v_obj->lsdqsort(\\\"$v_obj0\\\", \\\"$v_label\\\", \\\"DOWN\\\");\\n\"" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a add_an_obj
if ( choice == 52 )
{
	cmd( "set v_obj0 cur" );
	cmd( "set v_label \"\"" );
	cmd( "set numobj \"1\"" );
	cmd( "set v_num \"\"" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'ADDOBJ' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'add_n_objects2' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the object created\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Object to create\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.x.e; .a.x.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.x" );
	cmd( "label .a.x.l -text \"Number of objects to create\"" );
	cmd( "entry .a.x.e -width 6 -textvariable numobj -justify center" );
	cmd( "bind .a.x.e <Return> {focus .a.v.e; .a.v.e selection range 0 end}" );
	cmd( "pack .a.x.l .a.x.e" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Example object (if available)\"" );
	cmd( "entry .a.v.e -width 25 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object for new object(s)\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.x .a.v .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#ADDOBJ } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	 
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if { $numobj == \"1\"} {set choice 1} {set choice 0}" );
		cmd( "if {$v_obj0 != \"\"} {.f.t.t insert insert \"$v_obj0 = \"}" );

		if ( choice  == 1 )
		{
		cmd( "if {$v_obj == \"p\" && $v_num==\"\" } { .f.t.t insert insert \"ADDOBJ(\\\"$v_label\\\");\\n\"}" );
		cmd( "if {$v_obj == \"p\" && $v_num!=\"\"} { .f.t.t insert insert \"ADDOBJ_EX(\\\"$v_label\\\", $v_num);\\n\"}" );
		cmd( "if {$v_obj != \"p\" && $v_num == \"\" } { .f.t.t insert insert \"ADDOBJS($v_obj, \\\"$v_label\\\");\\n\"}" );
		cmd( "if {$v_obj != \"p\" && $v_num != \"\"} { .f.t.t insert insert \"ADDOBJ_EXS($v_obj, \\\"$v_label\\\", $v_num);\\n\"}" );
		}
		else
		{
		cmd( "if {$v_obj == \"p\" && $v_num!=\"\" && [string is integer -strict $numobj]} { .f.t.t insert insert \"ADDNOBJ_EX(\\\"$v_label\\\", $numobj, $v_num);\\n\"; set choice -3}" );
		cmd( "if {$v_obj != \"p\" && $v_num!= \"\" && [string is integer -strict $numobj]} { .f.t.t insert insert \"ADDNOBJ_EXS($v_obj, \\\"$v_label\\\", $numobj, $v_num);\\n\"; set choice -3}" );
		cmd( "if {$v_obj == \"p\" && $v_num==\"\" && [string is integer -strict $numobj]} { .f.t.t insert insert \"ADDNOBJ(\\\"$v_label\\\", $numobj);\\n\"; set choice -3}" );
		cmd( "if {$v_obj != \"p\" && $v_num== \"\" && [string is integer -strict $numobj]} { .f.t.t insert insert \"ADDNOBJS($v_obj, \\\"$v_label\\\", $numobj);\\n\"; set choice -3}" );
		}
	}
	else
	{
		cmd( "if {$v_num==\"\" && [string is integer -strict $numobj]} { .f.t.t insert insert \"$v_obj0 = $v_obj->add_n_objects2(\\\"$v_label\\\", $numobj);\\n\"}" );
		cmd( "if {$v_num!=\"\" && [string is integer -strict $numobj]} {.f.t.t insert insert \"$v_obj0 = $v_obj->add_n_objects2(\\\"$v_label\\\", $numobj, $v_num);\\n\"}" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a delete_obj
if ( choice == 53 )
{
	cmd( "set v_obj0 cur" );

	if ( macro )
		cmd( "newtop .a \"Insert 'DELETE' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'delete_obj' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Object to delete\"" );
	cmd( "entry .a.d.e -width 25 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "pack .a.d -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#DELETE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
		cmd( ".f.t.t insert insert \"DELETE($v_obj0);\\n\"" );
	else
		cmd( ".f.t.t insert insert \"$v_obj0->delete_obj();\\n\"" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a RNDDRAW
if ( choice == 54 )
{
	cmd( "set v_obj0 cur" );
	cmd( "set v_num \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_lag 0" );
	cmd( "set v_tot \"\"" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'RNDDRAW' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'draw_rnd' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the object found\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.v.e; .a.v.e selection range 0 end}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Object to draw\"" );
	cmd( "entry .a.v.e -width 25 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter defining probabilities\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.l.e; .a.l.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Lag to use\"" );
	cmd( "entry .a.l.e -width 2 -textvariable v_lag -justify center" );
	cmd( "bind .a.l.e <Return> {focus .a.t.e; .a.t.e selection range 0 end}" );
	cmd( "pack .a.l.l .a.l.e" );

	cmd( "frame .a.t" );
	cmd( "label .a.t.l -text \"Sum over all values (if available)\"" );
	cmd( "entry .a.t.e -width 15 -textvariable v_tot -justify center" );
	cmd( "bind .a.t.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.t.l .a.t.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Object from which to search\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.v .a.n .a.l .a.t .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#RNDDRAW } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );
	cmd( "if {$v_tot == \"\"} {set choice 1} {set choice 2}" );

	if ( macro )
	{
		if ( choice == 1 )
		 {
		  cmd( "if {$v_obj == \"p\" && $v_lag == 0 && $v_label != \"\"} { .f.t.t insert insert \"$v_obj0 = RNDDRAW(\\\"$v_num\\\", \\\"$v_label\\\");\\n\"}" );
		  cmd( "if {$v_obj == \"p\" && $v_lag == 0 && $v_label == \"\"} { .f.t.t insert insert \"$v_obj0 = RNDDRAW_FAIR(\\\"$v_num\\\");\\n\"}" );  
		  cmd( "if {$v_obj == \"p\" && $v_lag != 0 && [string is integer -strict $v_lag]} { .f.t.t insert insert \"$v_obj0 = RNDDRAWL(\\\"$v_num\\\", \\\"$v_label\\\", $v_lag);\\n\"}" );
		  cmd( "if {$v_obj != \"p\" && $v_lag == 0 && $v_label != \"\" } { .f.t.t insert insert \"$v_obj0 = RNDDRAWS($v_obj, \\\"$v_num\\\", \\\"$v_label\\\");\\n\"}" );
		  cmd( "if {$v_obj != \"p\" && $v_lag == 0 && $v_label == \"\" } { .f.t.t insert insert \"$v_obj0 = RNDDRAW_FAIRS($v_obj, \\\"$v_num\\\");\\n\"}" );  
		  cmd( "if {$v_obj != \"p\" && $v_lag != 0 && [string is integer -strict $v_lag]} { .f.t.t insert insert \"$v_obj0 = RNDDRAWLS($v_obj, \\\"$v_num\\\", \\\"$v_label\\\", $v_lag);\\n\"}" );
		 }
		else
		 {
		  cmd( "if {$v_obj == \"p\" && $v_lag == 0} { .f.t.t insert insert \"$v_obj0 = RNDDRAWTOT(\\\"$v_num\\\", \\\"$v_label\\\", $v_tot);\\n\"}" );
		  cmd( "if {$v_obj == \"p\" && $v_lag != 0 && [string is integer -strict $v_lag]} { .f.t.t insert insert \"$v_obj0 = RNDDRAW_TOTL(\\\"$v_num\\\", \\\"$v_label\\\", $v_lag, $v_tot);\\n\"}" );
		  cmd( "if {$v_obj != \"p\" && $v_lag == 0} { .f.t.t insert insert \"$v_obj0 = RNDDRAWTOTS($v_obj, \\\"$v_num\\\", \\\"$v_label\\\", $v_tot);\\n\"}" );
		  cmd( "if {$v_obj != \"p\" && $v_lag != 0 && [string is integer -strict $v_lag]} { .f.t.t insert insert \"$v_obj0 = RNDDRAW_TOTLS($v_obj, \\\"$v_num\\\", \\\"$v_label\\\", $v_lag, $v_tot);\\n\"}" );
		 }
	}
	else
	{
		if ( choice == 1 )
		  cmd( "if {[string is integer -strict $v_lag]} {.f.t.t insert insert \"$v_obj0 = $v_obj->draw_rnd(\\\"$v_num\\\", \\\"$v_label\\\", $v_lag);\\n\"}" );
		else
		  cmd( "if {[string is integer -strict $v_lag]} {.f.t.t insert insert \"$v_obj0 = $v_obj->draw_rnd(\\\"$v_num\\\", \\\"$v_label\\\", $v_lag, $v_tot);\\n\"}" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a SEARCH
if ( choice == 55 )
{
	cmd( "set v_obj0 cur" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'SEARCH' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'search' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the object found\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Object to search\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Object from which to search\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LSD_macros.html#SEARCH } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
		cmd( "if { $v_obj == \"p\" } {.f.t.t insert insert \"$v_obj0 = SEARCH(\\\"$v_label\\\");\\n\"} {.f.t.t insert insert \"$v_obj0 = SEARCHS($v_obj, \\\"$v_label\\\");\\n\"}" );
	else
		cmd( ".f.t.t insert insert \"$v_obj0 = $v_obj->search(\\\"$v_label\\\");\\n\"" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a SUM
if ( choice == 56 )
{
	cmd( "set v_num %d", v_counter );
	cmd( "set v_label \"\"" );
	cmd( "set v_lag 0" );
	cmd( "set v_obj p" );

	if ( macro )
		cmd( "newtop .a \"Insert 'SUM' Command\" { set choice 2 }" );
	else
		cmd( "newtop .a \"Insert 'sum' Command\" { set choice 2 }" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Number v\\\[x\\] to assign the result\"" );
	cmd( "entry .a.v.e -width 2 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> {focus .a.n.e; .a.n.e selection range 0 end}" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Variable or parameter to sum\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> {focus .a.l.e; .a.l.e selection range 0 end}" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Lag to use\"" );
	cmd( "entry .a.l.e -width 2 -textvariable v_lag -justify center" );
	cmd( "bind .a.l.e <Return> {focus .a.o.e; .a.o.e selection range 0 end}" );
	cmd( "pack .a.l.l .a.l.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Parent object\"" );
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> {focus .a.f.ok}" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.v .a.n .a.l .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SUM } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );	// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	if ( macro )
	{
		cmd( "if {$v_num!=\"\" && [string is integer -strict $v_num]} {.f.t.t insert insert \"v\\\[$v_num\\] = \"}" );

		cmd( "if {$v_lag==0 && $v_obj==\"p\"} { .f.t.t insert insert \"SUM(\\\"$v_label\\\")\"}" );
		cmd( "if {$v_lag!=0 && [string is integer -strict $v_lag] && $v_obj==\"p\"} { .f.t.t insert insert \"SUML(\\\"$v_label\\\", $v_lag)\"}" );
		cmd( "if {$v_lag==0 && $v_obj!=\"p\"} { .f.t.t insert insert \"SUMS($v_obj, \\\"$v_label\\\")\"}" );
		cmd( "if {$v_lag!=0 && [string is integer -strict $v_lag] && $v_obj!=\"p\"} { .f.t.t insert insert \"SUMLS($v_obj, \\\"$v_label\\\", $v_lag)\"}" );
	}
	else
	{
		cmd( "if {$v_num!=\"\" && [string is integer -strict $v_num] && [string is integer -strict $v_lag]} {.f.t.t insert insert \"v\\\[$v_num\\] = $v_obj->sum(\\\"$v_label\\\", $v_lag)\"} {.f.t.t insert insert \"$v_obj->sum(\\\"$v_label\\\", $v_lag)\"}" );
	}

	cmd( "if {$v_num!=\"\"} {.f.t.t insert insert \";\\n\"}" );

	cmd( "if {$v_num==\"\"} { set num -1} {set num $v_num}" );
	if ( num != -1 )
		v_counter=++num;

 	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	
	choice = 23;	// do syntax coloring
	goto loop;
}


// insert a network macro
if ( choice == 72 )
{
	cmd( "set res 73");
	
	cmd( "newtop .a \"Network Macros\" { set choice 2 }" );

	cmd( "label .a.tit -text \"Available LSD network macros\"" );

	cmd( "frame .a.f -bd 2 -relief groove" );
	
	cmd( "radiobutton .a.f.r1 -text \"INIT - create a new network\" -variable res -value 73 -underline 0" );
	cmd( "radiobutton .a.f.r2 -text \"LOAD - load a network from a file\" -variable res -value 74 -underline 0" );
	cmd( "radiobutton .a.f.r3 -text \"SAVE - save a network to a file\" -variable res -value 75 -underline 0" );
	cmd( "radiobutton .a.f.r4 -text \"SNAP - add a network snapshot to a file\" -variable res -value 76 -underline 1" );
	cmd( "radiobutton .a.f.r5 -text \"ADD - add a node or link\" -variable res -value 77 -underline 0" );
	cmd( "radiobutton .a.f.r6 -text \"V - get the values of a node or link\" -variable res -value 78 -underline 0" );
	cmd( "radiobutton .a.f.r7 -text \"WRITE - set the values of a node or link\" -variable res -value 79 -underline 0" );
	cmd( "radiobutton .a.f.r8 -text \"CYCLE - cycle through links\" -variable res -value 80 -underline 0" );
	cmd( "radiobutton .a.f.r9 -text \"SEARCH - search for a node or link\" -variable res -value 81 -underline 1" );
	cmd( "radiobutton .a.f.r10 -text \"LINK - get objects connected by link\" -variable res -value 82 -underline 3" );
	cmd( "radiobutton .a.f.r11 -text \"SHUFFLE - shuffle nodes in a network\" -variable res -value 83 -underline 1" );
	cmd( "radiobutton .a.f.r12 -text \"RNDDRAW - random draw a node or link\" -variable res -value 84 -underline 0" );
	cmd( "radiobutton .a.f.r13 -text \"DELETE - delete a network, node or link\" -variable res -value 85 -underline 0" );
	cmd( "radiobutton .a.f.r14 -text \"STAT - statistics about a network or node\" -variable res -value 86 -underline 1" );

	cmd( "pack .a.f.r1 .a.f.r2 .a.f.r3 .a.f.r4 .a.f.r5 .a.f.r6 .a.f.r7 .a.f.r8 .a.f.r9 .a.f.r10 .a.f.r11 .a.f.r12 .a.f.r13 .a.f.r14 -anchor w" );

	cmd( "pack .a.tit .a.f -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LSD_macros.html#Networks } { set choice 2 }" );

	cmd( "bind .a <KeyPress-i> {.a.f.r1  invoke; set choice 1}; bind .a <KeyPress-I> {.a.f.r1  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-l> {.a.f.r2  invoke; set choice 1}; bind .a <KeyPress-L> {.a.f.r2  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-s> {.a.f.r3  invoke; set choice 1}; bind .a <KeyPress-S> {.a.f.r3  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-n> {.a.f.r4  invoke; set choice 1}; bind .a <KeyPress-N> {.a.f.r4  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-a> {.a.f.r5  invoke; set choice 1}; bind .a <KeyPress-A> {.a.f.r5  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-v> {.a.f.r6  invoke; set choice 1}; bind .a <KeyPress-V> {.a.f.r6  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-w> {.a.f.r7  invoke; set choice 1}; bind .a <KeyPress-W> {.a.f.r7  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-c> {.a.f.r8  invoke; set choice 1}; bind .a <KeyPress-C> {.a.f.r8  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-e> {.a.f.r9  invoke; set choice 1}; bind .a <KeyPress-E> {.a.f.r9  invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-k> {.a.f.r10 invoke; set choice 1}; bind .a <KeyPress-K> {.a.f.r10 invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-h> {.a.f.r11 invoke; set choice 1}; bind .a <KeyPress-H> {.a.f.r11 invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-r> {.a.f.r12 invoke; set choice 1}; bind .a <KeyPress-R> {.a.f.r12 invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-d> {.a.f.r13 invoke; set choice 1}; bind .a <KeyPress-D> {.a.f.r13 invoke; set choice 1}" );
	cmd( "bind .a <KeyPress-t> {.a.f.r14 invoke; set choice 1}; bind .a <KeyPress-T> {.a.f.r14.a.r.net invoke; set choice 1}" );
	cmd( "bind .a <Return> { .a.b.ok invoke }" );

	cmd( "showtop .a" );
	cmd( "focus .a.f.r1" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{ 
		choice = 0;
		goto loop;
	}
	 
	cmd( "set choice $res" );		// execute selected macro

	goto loop;
}


// create a new network
if ( choice == 73 )
{
	cmd( "set netListLong [ list \"Disconnected (no links)\" \"Star (undirected)\" \"Circle (undirected)\" \"Lattice (directed)\" \"Random (directed)\" \"Random (undirected)\" \"Uniform random (directed)\" \"Renyi-Erdos random (undirected)\" \"Small World (undirected)\" \"Scale-free (undirected)\" ]");
	cmd( "set netListShort [ list \"DISCONNECTED\" \"STAR\" \"CIRCLE\" \"LATTICE\" \"RANDOM-DIR\" \"RANDOM-UNDIR\" \"UNIFORM\" \"RENYI-ERDOS\" \"SMALL-WORLD\" \"SCALE-FREE\" ]");
	cmd( "set netListPar [ list [ list ] [ list ] [ list \"Number of neighbors\" ] [ list \"Number of columns\" \"Neighbors (4 or 8)\" ] [ list \"Number of links\" ] [ list \"Number of links (2 per connection)\" ] [ list \"Out-degree\" ] [ list \"Out-degree\" ] [ list \"Out-degree\" \"Reconnection probability\" ] [ list \"Out-degree\" \"Power-law degree\" ] ]");
	
	cmd( "set v_net [ lindex $netListLong 0 ]" );
	cmd( "set v_label \"\"" );
	cmd( "set v_num 1" );
	cmd( "set v_obj p" );
	cmd( "set v_par1 \"\"" );
	cmd( "set v_par2 \"\"" );

	cmd( "newtop .a \"Insert 'INIT_NET' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Type of network to create\"" );
	cmd( "ttk::combobox .a.d.e -width 30 -textvariable v_net -justify center -values $netListLong" );
	cmd( "bind .a.d.e <<ComboboxSelected>> { set a [ lindex [ lindex $netListPar [ .a.d.e current ] ] 0 ]; if { $a == \"\" } { set a \"(unused)\"; .a.p1.e configure -state disabled } { .a.p1.e configure -state normal }; .a.p1.l configure -text $a; set a [ lindex [ lindex $netListPar [ .a.d.e current ] ] 1 ]; if { $a == \"\" } { set a \"(unused)\"; .a.p2.e configure -state disabled } { .a.p2.e configure -state normal }; .a.p2.l configure -text $a }" );
	cmd( "bind .a.d.e <Return> { focus .a.x.e; .a.x.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.x" );
	cmd( "label .a.x.l -text \"Number of nodes\"" );
	cmd( "entry .a.x.e -width 6 -textvariable v_num -justify center" );
	cmd( "bind .a.x.e <Return> { focus .a.p1.e; .a.p1.e selection range 0 end }" );
	cmd( "pack .a.x.l .a.x.e" );

	cmd( "frame .a.p1" );
	cmd( "label .a.p1.l -text \"(unused)\"" );
	cmd( "entry .a.p1.e -width 6 -textvariable v_par1 -justify center -state disabled" );
	cmd( "bind .a.p1.e <Return> { focus .a.p2.e; .a.p2.e selection range 0 end }" );
	cmd( "pack .a.p1.l .a.p1.e" );

	cmd( "frame .a.p2" );
	cmd( "label .a.p2.l -text \"(unused)\"" );
	cmd( "entry .a.p2.e -width 6 -textvariable v_par2 -justify center -state disabled" );
	cmd( "bind .a.p2.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.p2.l .a.p2.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Network node object name\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Network nodes parent object\"" );
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.x .a.p1 .a.p2 .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#INIT_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	 
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set res [ .a.d.e current ]" );
	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "if { $v_par1 == \"\"} { set v_par1 0 }" );
	cmd( "if { $v_par2 == \"\"} { set v_par2 0 }" );
	
	cmd( "if { $v_obj == \"p\" && [ llength [ lindex $netListPar $res ] ] == 2 && [ string is integer -strict $v_num ] && [ string is integer -strict $v_par1 ] && [ string is double -strict $v_par2 ] } { .f.t.t insert insert \"INIT_NET(\\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num, $v_par1, $v_par2);\\n\"}" );
	cmd( "if { $v_obj == \"p\" && [ llength [ lindex $netListPar $res ] ] == 1 && [ string is integer -strict $v_num ] && [ string is integer -strict $v_par1 ] } { .f.t.t insert insert \"INIT_NET(\\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num, $v_par1);\\n\"}" );
	cmd( "if { $v_obj == \"p\" && [ llength [ lindex $netListPar $res ] ] == 0 && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"INIT_NET(\\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num);\\n\"}" );
	
	cmd( "if { $v_obj != \"p\" && [ llength [ lindex $netListPar $res ] ] == 2 && [ string is integer -strict $v_num ] && [ string is integer -strict $v_par1 ] && [ string is double -strict $v_par2 ] } { .f.t.t insert insert \"INIT_NETS($v_obj, \\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num, $v_par1, $v_par2);\\n\"}" );
	cmd( "if { $v_obj != \"p\" && [ llength [ lindex $netListPar $res ] ] == 1 && [ string is integer -strict $v_num ] && [ string is integer -strict $v_par1 ] } { .f.t.t insert insert \"INIT_NETS($v_obj, \\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num, $v_par1);\\n\"}" );
	cmd( "if { $v_obj != \"p\" && [ llength [ lindex $netListPar $res ] ] == 0 && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"INIT_NETS($v_obj, \\\"$v_label\\\", \\\"[ lindex $netListShort $res ]\\\", $v_num);\\n\"}" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// load a network from a file
if ( choice == 74 )
{
	cmd( "set v_net \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'LOAD_NET' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Network file name (without .net Pajek extension)\"" );
	cmd( "entry .a.d.e -width 25 -textvariable v_net -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Network node object name\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Network nodes parent object\"" );
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#LOAD_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"LOAD_NET(\\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"LOAD_NETS($v_obj, \\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// save a network to a file
if ( choice == 75 )
{
	cmd( "set v_net \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'SAVE_NET' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Network file name (without .net Pajek extension)\"" );
	cmd( "entry .a.d.e -width 25 -textvariable v_net -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Network node object name\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Network nodes parent object\"" );
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SAVE_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"SAVE_NET(\\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"SAVE_NETS($v_obj, \\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// add a network snapshot to a file
if ( choice == 76 )
{
	cmd( "set v_net \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'SNAP_NET' Command\" { set choice 2 }" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Network file name (without .paj Pajek extension)\"" );
	cmd( "entry .a.d.e -width 25 -textvariable v_net -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Network node object name\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Network nodes parent object\"" );
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.d .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SNAP_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"SNAP_NET(\\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"SNAP_NETS($v_obj, \\\"$v_label\\\", \\\"$v_net\\\");\\n\"}" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// add a node or link
if ( choice == 77 )
{
	cmd( "set v_type 0" );
	cmd( "set v_obj0 cur" );
	cmd( "set v_num \"\"" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'ADDNODE/LINK' Command\" { set choice 2 }" );
	
	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Add to the network a\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text Node -width 6 -variable v_type -value 0 -command { write_any .a.d.e cur; .a.i.l configure -text \"Unique ID number\"; .a.n.l configure -text \"Node name (optional)\"; write_any .a.n.e \"\"; .a.o.l configure -text \"Object for new network node\" }" );
	cmd( "radiobutton .a.c.b.f -text Link -width 6 -variable v_type -value 1 -command { write_any .a.d.e curl; .a.i.l configure -text \"Link weight\"; .a.n.l configure -text \"Destination node object\"; write_any .a.n.e cur; .a.o.l configure -text \"Origin node object\" }" );
	cmd( "bind .a.c.b.e <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the object created\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.i.e; .a.i.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.i" );
	cmd( "label .a.i.l" ); 			// ID or weight
	cmd( "entry .a.i.e -width 6 -textvariable v_num -justify center" );
	cmd( "bind .a.i.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.i.l .a.i.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l" );			// name or destination
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// object or origin object
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.d .a.i .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#ADDNODE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "set choice $v_type" );
	cmd( "if { $v_obj0 != \"\" } { .f.t.t insert insert \"$v_obj0 = \" }" );
	
	if ( choice == 0 )
	{
		cmd( "if { $v_obj == \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"ADDNODE($v_num, \\\"$v_label\\\");\\n\" }" );
		cmd( "if { $v_obj != \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"ADDNODES($v_obj, $v_num, \\\"$v_label\\\");\\n\" }" );
	}
	else
	{
		cmd( "if { $v_obj == \"p\" && $v_num ==\"\" } { .f.t.t insert insert \"ADDLINK($v_label);\\n\" }" );
		cmd( "if { $v_obj == \"p\" && $v_num !=\"\" && [ string is double -strict $v_num ] } { .f.t.t insert insert \"ADDLINKW($v_label, $v_num);\\n\" }" );
		cmd( "if { $v_obj != \"p\" && $v_num ==\"\" } { .f.t.t insert insert \"ADDLINKS($v_obj, $v_label);\\n\" }" );
		cmd( "if { $v_obj != \"p\" && $v_num !=\"\" && [ string is double -strict $v_num ] } { .f.t.t insert insert \"ADDLINKWS($v_obj, $v_label, $v_num);\\n\" }" );
	}
	
	cmd( "if { $v_obj0!=\"\" } { .f.t.t insert insert \";\\n\" }" );
	cmd( ".f.t.t see insert" );
	
	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// get the values of a node or link
if ( choice == 78 )
{
	cmd( "set v_type 0" );
	cmd( "set v_num %d", v_counter );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'V_NODE/LINK' Command\" { set choice 2 }" );
	
	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Get the value of\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text \"Node ID\" -width 8 -variable v_type -value 0 -command { .a.v.l configure -text \"Number v\\\[x\\] to assign to\"; write_any .a.v.e %d; .a.o.l configure -text \"Object node\"; write_any .a.o.e p }", v_counter );
	cmd( "radiobutton .a.c.b.f -text \"Node name\" -width 8 -variable v_type -value 1 -command { .a.v.l configure -text \"char  pointer to assign to\"; write_any .a.v.e \"\"; .a.o.l configure -text \"Object node\"; write_any .a.o.e p }" );
	cmd( "radiobutton .a.c.b.g -text \"Link weight\" -width 8 -variable v_type -value 2 -command { .a.v.l configure -text \"Number v\\\[x\\] to assign to\"; write_any .a.v.e %d; .a.o.l configure -text \"Link pointer\"; write_any .a.o.e curl }", v_counter );
	cmd( "bind .a.c.b.e <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "bind .a.c.b.g <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f .a.c.b.g -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l" );			// v[x] or char pointer name
	cmd( "entry .a.v.e -width 15 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// object node or link
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.v .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#V_NODEID } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "set choice $v_type" );
	
	switch ( choice )
	{
		case 0:
			cmd( "if { $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"v\\\[$v_num\\] = \" }" );
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"V_NODEID()\" }" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"V_NODEIDS($v_obj)\" }" );

			cmd( "if { $v_num == \"\" } { set num -1 } { set num $v_num }" );
			if ( num != -1 )
				v_counter = ++num;
			break;
			
		case 1:
			cmd( "if { $v_num != \"\" } { .f.t.t insert insert \"$v_num = \" }" );
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"V_NODENAME()\"}" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"V_NODENAMES($v_obj)\" }" );
			break;

		case 2:
			cmd( "if { $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"v\\\[$v_num\\] = \" }" );
			cmd( ".f.t.t insert insert \"V_LINK($v_obj)\"" );

			cmd( "if { $v_num == \"\" } { set num -1 } { set num $v_num }" );
			if ( num != -1 )
				v_counter = ++num;
			break;

		default:
			break;
	}
	
	cmd( "if { $v_num!=\"\" } { .f.t.t insert insert \";\\n\" }" );
	cmd( ".f.t.t see insert" );
	
	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// write values to a node or link
if ( choice == 79 )
{
	cmd( "set v_type 0" );
	cmd( "set v_num \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'WRITE_NODE/LINK' Command\" { set choice 2 }" );
	
	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Write a value to\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text \"Node ID\" -width 8 -variable v_type -value 0 -command { .a.v.l configure -text \"Node ID to set\"; .a.o.l configure -text \"Object node\"; write_any .a.o.e p }", v_counter );
	cmd( "radiobutton .a.c.b.f -text \"Node name\" -width 8 -variable v_type -value 1 -command { .a.v.l configure -text \"Node name to set\"; .a.o.l configure -text \"Object node\"; write_any .a.o.e p }" );
	cmd( "radiobutton .a.c.b.g -text \"Link weight\" -width 8 -variable v_type -value 2 -command { .a.v.l configure -text \"Link weight to set\"; .a.o.l configure -text \"Link pointer\"; write_any .a.o.e curl }", v_counter );
	cmd( "bind .a.c.b.e <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "bind .a.c.b.g <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f .a.c.b.g -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l" );			// ID, name, weight
	cmd( "entry .a.v.e -width 25 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// object node or link
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.v .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#WRITE_NODEID } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.v.e" );
	cmd( ".a.v.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	
	switch ( choice )
	{
		case 0:
			cmd( "if { $v_obj == \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"WRITE_NODEID($v_num);\\n\" }" );
			cmd( "if { $v_obj != \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"WRITE_NODEIDS($v_obj, $v_num);\\n\" }" );
			break;
			
		case 1:
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"WRITE_NODENAME(\\\"$v_num\\\");\\n\"}" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"WRITE_NODENAMES($v_obj, \\\"$v_num\\\");\\n\" }" );
			break;

		case 2:
			cmd( "if { $v_num !=\"\" && [ string is double -strict $v_num ] } { .f.t.t insert insert \"WRITE_LINK($v_obj, $v_num);\\n\" }" );
			break;

		default:
			break;
	}
	cmd( ".f.t.t see insert" );
	
	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// cycle through links
if ( choice == 80 )
{
	cmd( "set v_obj curl" );
	cmd( "set v_par p" );

	cmd( "newtop .a \"Insert 'CYCLE_LINK' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Cycling link pointer\"" );
	cmd( "entry .a.c.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.c.e <Return> { focus .a.p.e; .a.p.e selection range 0 end }" );
	cmd( "pack .a.c.l .a.c.e" );

	cmd( "frame .a.p" );
	cmd( "label .a.p.l -text \"Parent node object\"" );
	cmd( "entry .a.p.e -width 25 -textvariable v_par -justify center" );
	cmd( "bind .a.p.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.p.l .a.p.e" );

	cmd( "pack .a.c .a.p -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#CYCLE_LINK } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.c.e" );
	cmd( ".a.c.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [.f.t.t index insert]" );

	cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"CYCLE_LINK($v_obj)\\n\"}" );
	cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"CYCLE_LINKS($v_par, $v_obj)\\n\" }" );

	cmd( "set in [ .f.t.t index insert ]" );
	cmd( "scan $in %%d.%%d line col" );
	cmd( "set line [ expr $line -1 ]" );
	cmd( "set s [ .f.t.t get $line.0 $line.end ]" );
	s = ( char * ) Tcl_GetVar( inter, "s", 0 );
	for ( i = 0; s[ i ] == ' ' || s[ i ] == '\t'; ++i )
		str[ i ] = s[ i ];
	if ( i > 0 )
	{
		str[ i ] = '\0';
		cmd( ".f.t.t insert insert \"%s\"", str );
		cmd( ".f.t.t insert insert \"{\\n\"" );
		cmd( ".f.t.t insert insert \"%s\\t\"", str );
		cmd( "set b [.f.t.t index insert]" );
		cmd( ".f.t.t insert insert \"\\n%s\"", str );
	}
	else
	{
		cmd( ".f.t.t insert insert \"{\\n\"" );
		cmd( ".f.t.t insert insert \"\\t\"" );
		cmd( "set b [.f.t.t index insert]" );
		cmd( ".f.t.t insert insert \"\\n\"" );
	}

	cmd( ".f.t.t insert insert \"}\\n\"" );
	cmd( ".f.t.t mark set insert \"$b\"" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// search for a node or link
if ( choice == 81 )
{
	cmd( "set v_obj0 cur" );
	cmd( "set v_num 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'SEARCH_NODE/LINK' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Search for a\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text Node -width 6 -variable v_type -value 0 -command { write_any .a.d.e cur; .a.n.l configure -text \"Network node object name\"; .a.n.e configure -state normal; .a.o.l configure -text \"Network nodes parent object\" }" );
	cmd( "radiobutton .a.c.b.f -text Link -width 6 -variable v_type -value 1 -command { write_any .a.d.e curl; .a.n.l configure -text \"(unused)\"; write_any .a.n.e \"\"; .a.n.e configure -state disabled; .a.o.l configure -text \"Node object\" }" );
	cmd( "bind .a.c.b.e <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the element found\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"ID to search for\"" );
	cmd( "entry .a.v.e -width 15 -textvariable v_num -justify center" );
	cmd( "bind .a.v.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l" );			// object name or unused
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// parent object or node object
	cmd( "entry .a.o.e -width 25 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.d .a.v .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LSD_macros.html#SEARCH_NODE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	cmd( "if { $v_obj0 != \"\" } { .f.t.t insert insert \"$v_obj0 = \" }" );
	
	if ( choice == 0 )
	{
		cmd( "if { $v_obj == \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"SEARCH_NODE(\\\"$v_label\\\", $v_num)\" }" );
		cmd( "if { $v_obj != \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"SEARCH_NODES($v_obj, \\\"$v_label\\\", $v_num)\" }" );
	}
	else
	{
		cmd( "if { $v_obj == \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"SEARCH_LINK($v_num)\" }" );
		cmd( "if { $v_obj != \"p\" && $v_num !=\"\" && [ string is integer -strict $v_num ] } { .f.t.t insert insert \"SEARCH_LINKS($v_obj, $v_num)\" }" );
	}
	
	cmd( "if { $v_obj0!=\"\" } { .f.t.t insert insert \";\\n\" }" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// get objects connected by link
if ( choice == 82 )
{
	cmd( "set v_type 0" );
	cmd( "set v_obj0 cur" );
	cmd( "set v_obj curl" );

	cmd( "newtop .a \"Insert 'LINKTO/FROM' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Get the node object\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text \"Link points to\" -width 12 -variable v_type -value 0" );
	cmd( "radiobutton .a.c.b.f -text \"Link points from\" -width 12 -variable v_type -value 1" );
	cmd( "bind .a.c.b.e <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the node pointed\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.v.e; .a.v.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.v" );
	cmd( "label .a.v.l -text \"Link pointer\"" );
	cmd( "entry .a.v.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.v.e <Return>  { focus .a.f.ok }" );
	cmd( "pack .a.v.l .a.v.e" );

	cmd( "pack .a.c .a.d .a.v -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#LINKTO } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	cmd( "if { $v_obj0 != \"\" } { .f.t.t insert insert \"$v_obj0 = \" }" );
	
	if ( choice == 0 )
		cmd( ".f.t.t insert insert \"LINKTO($v_obj)\" }" );
	else
		cmd( ".f.t.t insert insert \"LINKFROM($v_obj)\" }" );

	cmd( "if { $v_obj0!=\"\" } { .f.t.t insert insert \";\\n\" }" );
	cmd( ".f.t.t see insert" );
	
	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// shuffle nodes in a network
if ( choice == 83 )
{
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'SHUFFLE_NET' Command\" { set choice 2 }" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l -text \"Network node object name\"" );
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l -text \"Network nodes parent object\"" );
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#SHUFFLE_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.n.e" );
	cmd( ".a.n.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"SHUFFLE_NET(\\\"$v_label\\\");\\n\"}" );
	cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"SHUFFLE_NETS($v_obj, \\\"$v_label\\\");\\n\"}" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// random draw a node or link
if ( choice == 84 )
{
	cmd( "set v_type 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );
	cmd( "set v_obj0 cur" );

	cmd( "newtop .a \"Insert 'RNDDRAW_NODE/LINK' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Randomly draw a\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text Node -width 6 -variable v_type -value 0 -command { write_any .a.d.e cur; .a.n.l configure -text \"Network node object name\"; .a.n.e configure -state normal; .a.o.l configure -text \"Network nodes parent object\" }" );
	cmd( "radiobutton .a.c.b.f -text Link -width 6 -variable v_type -value 1 -command { write_any .a.d.e curl; .a.n.l configure -text \"(unused)\"; write_any .a.n.e \"\"; .a.n.e configure -state disabled; .a.o.l configure -text \"Network node object\" }" );
	cmd( "bind .a.c.b.e <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.d.e; .a.d.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.d" );
	cmd( "label .a.d.l -text \"Pointer to return the element drawn\"" );
	cmd( "entry .a.d.e -width 6 -textvariable v_obj0 -justify center" );
	cmd( "bind .a.d.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.d.l .a.d.e" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l" );			// network name or unused
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// parent object or node object
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.d .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#RNDDRAW_NODE } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.d.e" );
	cmd( ".a.d.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	cmd( "if { $v_obj0 != \"\" } { .f.t.t insert insert \"$v_obj0 = \" }" );
	
	if ( choice == 0 )
	{
		cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"RNDDRAW_NODE(\\\"$v_label\\\")\" }" );
		cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"RNDDRAW_NODES($v_obj, \\\"$v_label\\\")\" }" );
	}
	else
	{
		cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"RNDDRAW_LINK()\" }" );
		cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"RNDDRAW_LINKS($v_obj)\" }" );
	}

	cmd( "if { $v_obj0!=\"\" } { .f.t.t insert insert \";\\n\" }" );
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// delete a network, node or link
if ( choice == 85 )
{
	cmd( "set v_type 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'DELETE_NET/NODE/LINK' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Delete a\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text Network -width 6 -variable v_type -value 0 -command { .a.n.l configure -text \"Network node object name\"; .a.n.e configure -state normal; .a.o.l configure -text \"Network nodes parent object\"; write_any .a.o.e p }" );
	cmd( "radiobutton .a.c.b.f -text Node -width 6 -variable v_type -value 1 -command { .a.n.l configure -text \"(unused)\"; write_any .a.n.e \"\"; .a.n.e configure -state disabled; .a.o.l configure -text \"Node object\"; write_any .a.o.e p }" );
	cmd( "radiobutton .a.c.b.g -text Link -width 6 -variable v_type -value 2 -command { .a.n.l configure -text \"(unused)\"; write_any .a.n.e \"\"; .a.n.e configure -state disabled; .a.o.l configure -text \"Link pointer\"; write_any .a.o.e curl }" );
	cmd( "bind .a.c.b.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "bind .a.c.b.g <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f .a.c.b.g -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l" );			// object name or unused
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// parent object or node object
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#DELETE_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.n.e" );
	cmd( ".a.n.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	
	switch ( choice )
	{
		case 0:
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"DELETE_NET(\\\"$v_label\\\");\\n\" }" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"DELETE_NETS($v_obj, \\\"$v_label\\\");\\n\" }" );
			break;
			
		case 1:
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"DELETE_NODE();\\n\"}" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"DELETE_NODES($v_obj);\\n\" }" );
			break;

		case 2:
			cmd( ".f.t.t insert insert \"DELETE_LINK($v_obj);\\n\"" );
			break;

		default:
			break;
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// statistics about a network or node
if ( choice == 86 )
{
	cmd( "set v_type 0" );
	cmd( "set v_label \"\"" );
	cmd( "set v_obj p" );

	cmd( "newtop .a \"Insert 'STAT_NET/NODE' Command\" { set choice 2 }" );

	cmd( "frame .a.c" );
	cmd( "label .a.c.l -text \"Get statistics from a\"" );
	
	cmd( "frame .a.c.b -bd 2 -relief groove" );
	cmd( "radiobutton .a.c.b.e -text Network -width 6 -variable v_type -value 0 -command { .a.n.l configure -text \"Network node object name\"; .a.n.e configure -state normal; .a.o.l configure -text \"Network nodes parent object\"; write_any .a.o.e p }" );
	cmd( "radiobutton .a.c.b.f -text Node -width 6 -variable v_type -value 1 -command { .a.n.l configure -text \"(unused)\"; write_any .a.n.e \"\"; .a.n.e configure -state disabled; .a.o.l configure -text \"Node object\"; write_any .a.o.e p }" );
	cmd( "bind .a.c.b.e <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "bind .a.c.b.f <Return> { focus .a.n.e; .a.n.e selection range 0 end }" );
	cmd( "pack .a.c.b.e .a.c.b.f -side left" );
	
	cmd( "pack .a.c.l .a.c.b" );

	cmd( "frame .a.n" );
	cmd( "label .a.n.l" );			// object name or unused
	cmd( "entry .a.n.e -width 25 -textvariable v_label -justify center" );
	cmd( "bind .a.n.e <Return> { focus .a.o.e; .a.o.e selection range 0 end }" );
	cmd( "pack .a.n.l .a.n.e" );

	cmd( "frame .a.o" );
	cmd( "label .a.o.l" );			// parent object or node object
	cmd( "entry .a.o.e -width 6 -textvariable v_obj -justify center" );
	cmd( "bind .a.o.e <Return> { focus .a.f.ok }" );
	cmd( "pack .a.o.l .a.o.e" );

	cmd( "pack .a.c .a.n .a.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .a f { set choice 1 } { LsdHelp LSD_macros.html#STAT_NET } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( ".a.c.b.e invoke" );
	cmd( "focus .a.n.e" );
	cmd( ".a.n.e selection range 0 end" );
	
	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );
	  
	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "savCurIni" );				// save data for recolor
	cmd( "set a [ .f.t.t index insert ]" );

	cmd( "set choice $v_type" );
	
	if ( choice == 0 )
	{
		cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"STAT_NET(\\\"$v_label\\\");\\n\" }" );
		cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"STAT_NETS($v_obj, \\\"$v_label\\\");\\n\" }" );
	}
	else
	{
			cmd( "if { $v_obj == \"p\" } { .f.t.t insert insert \"STAT_NODE();\\n\"}" );
			cmd( "if { $v_obj != \"p\" } { .f.t.t insert insert \"STAT_NODES($v_obj);\\n\" }" );
	}
	cmd( ".f.t.t see insert" );

	cmd( "savCurFin; updCurWnd" );	// save data for recolor
	choice = 23;					// do syntax coloring
	goto loop;
}


// find matching parenthesis
if ( choice == 32 )
{
	cmd( "set sym [ .f.t.t get insert ]" );
	cmd( "if { [ string compare $sym \"\\(\" ] != 0 } { \
			if { [ string compare $sym \"\\)\"] != 0 } { \
				set choice 0 \
			} { \
				set num -1; \
				set direction -backwards; \
				set fsign +; \
				set sign \"\"; \
				set terminal \"1.0\" \
			} \
		} { \
			set num 1; \
			set direction -forwards; \
			set fsign -; \
			set sign +; \
			set terminal end \
		}" );

	if ( choice == 0 )
		goto loop;
	
	cmd( "set cur [ .f.t.t index insert ]" );
	cmd( ".f.t.t tag add sel $cur \"$cur + 1char\"" );
	if ( num > 0 )
		cmd( "set cur [.f.t.t index \"insert + 1 char\"]" );
	
	while ( num != 0 && choice != 0 )
	{
		cmd( "set a [ .f.t.t search $direction \"\\(\" $cur $terminal ]" );
		cmd( "if { $a == \"\" } { set a [ .f.t.t index $terminal ] }" );
		cmd( "set b [ .f.t.t search $direction \"\\)\" $cur $terminal ]" );
		cmd( "if { $b == \"\" } { set b [ .f.t.t index $terminal ] }" );
		cmd( "if { $a == $b } { set choice 0 }" );
		if ( choice == 0 )
			goto loop;
		if ( num > 0 )
			cmd( "if [ .f.t.t compare $a < $b ] { set num [ expr $num + 1 ]; set cur [ .f.t.t index \"$a+1char\" ] } { set num [ expr $num - 1 ]; set cur [ .f.t.t index \"$b+1char\" ] }" );
		else
			cmd( "if [ .f.t.t compare $a > $b ] { set num [ expr $num + 1 ]; set cur [ .f.t.t index $a ] } { set num [ expr $num - 1 ]; set cur [ .f.t.t index $b ] }" );
	}
	
	choice = 0;

	cmd( " if { ! [ string compare $sign \"+\" ] } { .f.t.t tag add sel \"$cur - 1char\" $cur ; set num 1 } { .f.t.t tag add sel $cur \"$cur + 1char\"; set num 0 }" );

	cmd( ".f.t.t see $cur" );
	goto loop;
}


// select a model
if ( choice == 33 )
{
	Tcl_LinkVar( inter, "choiceSM", ( char * ) & num, TCL_LINK_INT );
	num = 0;

	cmd( "showmodel $groupdir" );
	cmd( "focus .l" );

	choice = 0;
	while ( choice == 0 && num == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .l; bind .f.t.t <Enter> { }" );
	choice = num;
	Tcl_UnlinkVar( inter, "choiceSM" );

	if ( choice == 2 || choice == 0 )
	{
		choice = 0;
		goto loop;
	}

	cmd( "set groupdir [ lindex $lrn 0 ]" );	//the group dir is the same for every element
	if ( choice == 14 )
		goto loop; 		//create a new model/group

	cmd( "set modelname [ lindex $lmn $result ]" );
	cmd( "set version [ lindex $lver $result ]" );
	cmd( "set modeldir [ lindex $ldn $result ]" );
	cmd( "set dirname $modeldir" );

	cmd( ".f.hea.grp.dat conf -text \"$modelgroup\"" );
	cmd( ".f.hea.mod.dat conf -text \"$modelname\"" );
	cmd( ".f.hea.ver.dat conf -text \"$version\"" );

	cmd( ".m.file entryconf 2 -state normal" );
	cmd( ".m.file entryconf 3 -state normal" );
	cmd( ".m.model entryconf 0 -state normal" );
	cmd( ".m.model entryconf 1 -state normal" );
	cmd( ".m.model entryconf 2 -state normal" );
	cmd( ".m.model entryconf 3 -state normal" );
	cmd( ".m.model entryconf 4 -state normal" );
	cmd( ".m.model entryconf 6 -state normal" );
	cmd( ".m.model entryconf 7 -state normal" );
	cmd( ".m.model entryconf 8 -state normal" );
	cmd( ".m.model entryconf 9 -state normal" );
	cmd( ".m.model entryconf 10 -state normal" );
	cmd( ".m.model entryconf 12 -state normal" );

	choice = 50;		// load description file
	goto loop;
}


// create a new version of the current model
if ( choice == 41 )
{
	cmd( "set mname $modelname" );
	cmd( "set mver $version" );
	cmd( "set mdir $dirname" );

	cmd( "newtop .a \"Save Model As...\" { set choice 2 }" );

	cmd( "frame .a.tit" );
	cmd( "label .a.tit.l -text \"Original model:\"" );

	cmd( "frame .a.tit.n" );
	cmd( "label .a.tit.n.n -fg red -text \"$modelname\"" );
	cmd( "label .a.tit.n.l1 -text \"( version\"" );
	cmd( "label .a.tit.n.v -fg red -text \"$version\"" );
	cmd( "label .a.tit.n.l2 -text \")\"" );
	cmd( "pack .a.tit.n.n .a.tit.n.l1 .a.tit.n.v .a.tit.n.l2 -side left" );

	cmd( "pack .a.tit.l .a.tit.n" );

	cmd( "frame .a.mname" );
	cmd( "label .a.mname.l -text \"New model name\"" );
	cmd( "entry .a.mname.e -width 25 -textvariable mname -justify center" );
	cmd( "pack .a.mname.l .a.mname.e" );

	cmd( "frame .a.mver" );
	cmd( "label .a.mver.l -text \"Version\"" );
	cmd( "entry .a.mver.e -width 10 -textvariable mver -justify center" );
	cmd( "pack .a.mver.l .a.mver.e" );

	cmd( "frame .a.mdir" );
	cmd( "label .a.mdir.l -text \"New (non-existing) directory name\"" );
	cmd( "entry .a.mdir.e -width 35 -textvariable mdir -justify center" );
	cmd( "pack .a.mdir.l .a.mdir.e" );

	cmd( "pack .a.tit .a.mname .a.mver .a.mdir -padx 5 -pady 5" );

	cmd( "okhelpcancel .a b { set choice 1 } { LsdHelp LMM.html#copy } { set choice 2 }" );
	cmd( "bind .a.mname.e <Return> {focus .a.mver.e; .a.mver.e selection range 0 end}" );
	cmd( "bind .a.mver.e <Return> {focus .a.mdir.e; .a.mdir.e selection range 0 end}" );
	cmd( "bind .a.mdir.e <Return> {focus .a.b.ok}" );

	cmd( "showtop .a" );
	cmd( ".a.mname.e selection range 0 end" );
	cmd( "focus .a.mname.e" );

	loop_copy:

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	// operation cancelled
	if ( choice == 2 )
	{
		cmd( "destroytop .a" );  
		choice = 0;
		goto loop;
	}

	cmd( "if { [ llength [ split $mdir ] ] > 1 } { set choice -1 }" );
	if ( choice == -1 )
	{
		cmd( "tk_messageBox -parent .a -type ok -title Error -icon error -message \"Space in path\" -detail \"Directory name must not contain spaces, please try a new name.\"" );
		cmd( "focus .a.mdir.e" );
		cmd( ".a.mdir.e selection range 0 end" );
		goto loop_copy;
	} 

	// control for existing directory
	cmd( "if { [ file exists \"$mdir\" ] == 1 } { tk_messageBox -parent .a -type ok -title Error -icon error -message \"Cannot create directory\" -detail \"$groupdir/$mdir\\n\\nPossibly there is already such a directory, please try a new directory.\"; set choice 3 }" );
	if ( choice == 3 )
	{
		cmd( "focus .a.mdir.e" );
		cmd( ".a.mdir.e selection range 0 end" );
		goto loop_copy;
	} 

	// control for an existing model with the same name AND same version
	cmd( "set dir [glob -nocomplain *]" );
	cmd( "set num [llength $dir]" );
	strcpy( str, " " );
	for ( i = 0; i < num && choice != 3; ++i )
	{
		cmd( "if { [ file isdirectory [ lindex $dir %d ] ] == 1 } { set curdir [ lindex $dir %i ] } { set curdir ___ }", i, i );
		s = ( char * ) Tcl_GetVar( inter, "curdir", 0 );
		strncpy( str, s, 499 );
		if ( strcmp( str, "___" ) && strcmp( str, "gnu" ) && strcmp( str, "gnu64" ) && strcmp( str, "src" ) && strcmp( str, "LMM.app" ) && strcmp( str, "R" ) && strcmp( str, "Manual" ) )
		{
			cmd( "set ex [ file exists \"$curdir/modelinfo.txt\" ]" );
			cmd( "if { $ex == 0 } { set choice 0 } { set choice 1 }" );
			if ( choice == 1 )
			{
				cmd( "set f [ open \"$curdir/modelinfo.txt\" r ]" );
				cmd( "set cname [ gets $f ]; set cver [ gets $f ];" );
				cmd( "close $f" );
			}
			else
				cmd( "set cname $curdir; set cver \"0.1\"" );
			cmd( "set comp [ string compare $cname $mname ]" );
			cmd( "set comp1 [ string compare $cver $mver ]" );
			cmd( "if { $comp == 0 && $comp1 == 0 } { set choice 3; set errdir $curdir }" );
		}
	}
	 
	if ( choice == 3 )
	{
		 cmd( "tk_messageBox -parent .a -type ok -title Error -icon error -message \"Cannot create new model\" -detail \"The model '$mname' already exists (directory: $errdir).\"" );
		cmd( ".a.mname.e selection range 0 end" );
		cmd( "focus .a.mname.e" );
		goto loop_copy;
	} 

	cmd( "destroytop .a" );

	// create a new copycat model
	cmd( "file copy \"$dirname\" \"$mdir\"" );
	cmd( "set dirname \"$mdir\"" );
	cmd( "set modeldir \"$mdir\"" );
	cmd( "set modelname \"$mname\"" );
	cmd( "set version \"$mver\"" );
	cmd( ".f.hea.mod.dat conf -text \"$modelname\"" );
	cmd( ".f.hea.ver.dat conf -text \"$version\"" );
	cmd( "set ex [file exists \"$dirname/modelinfo.txt\"]" );
	cmd( "if { $ex == 0 } { set choice 0 } { set choice 1 }" );
	if ( choice == 1 )
		cmd( "file delete \"$dirname/modelinfo.txt\"" );
	
	cmd( "set f [open \"$dirname/modelinfo.txt\" w]" );
	cmd( "puts $f \"$modelname\"" );
	cmd( "puts $f \"$version\"" );
	cmd( "set frmt \"%d %%B, %%Y\"" );
	cmd( "puts $f \"[ clock format [clock seconds] -format \"$frmt\" ]\"" );
	cmd( "close $f" );
	cmd( "tk_messageBox -parent . -type ok -title \"Save Model As...\" -icon info -message \"Model '$mname' created\" -detail \"Version: $mver\nDirectory: $dirname\"" );

	choice = 49;
	goto loop;
}


// increase indent
if ( choice == 42 )
{
	cmd( "set in [ .f.t.t tag range sel ]" );
	cmd( "if { [ string length $in ] == 0 } { set choice 0 } { set choice 1 }" );
	if ( choice == 0 )
		goto loop;
	
	cmd( "scan $in \"%%d.%%d %%d.%%d\" line1 col1 line2 col2" );
	cmd( "set num $line1" );
	i = num;
	cmd( "set num $line2" );

	for ( ; i <= num; ++i )
		cmd( ".f.t.t insert %d.0 \" \"", i );

	choice = 0;
	goto loop;
}


// decrease indent
if ( choice == 43 )
{
	cmd( "set in [ .f.t.t tag range sel ]" );
	cmd( "if { [ string length $in ] == 0 } { set choice 0 } { set choice 1 }" );
	if ( choice == 0 )
		goto loop;
	
	cmd( "scan $in \"%%d.%%d %%d.%%d\" line1 col1 line2 col2" );
	cmd( "set num $line1" );
	i = num;
	cmd( "set num $line2" );

	for ( ; i <= num; ++i )
	{
		cmd( "set c [.f.t.t get %d.0]", i );
		cmd( "if { $c == \" \" } {set choice 1} {set choice 0}" );
		if ( choice == 1 )
			cmd( ".f.t.t delete %d.0 ", i );
	}
	 
	choice = 0;
	goto loop;
}


// show and edit model info
if ( choice == 44 )
{
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}

	cmd( "set choice [file exists \"$modeldir/modelinfo.txt\"]" );
	i = choice;
	if ( i == 0 )
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Cannot find file for model info\" -detail \"Please, check the date of creation.\"" );
	else
	{
		cmd( "set f [open \"$modeldir/modelinfo.txt\" r]" );
		cmd( "gets $f; gets $f; set date [gets $f]" ); 
		cmd( "close $f" );
	}
	  
	cmd( "set complete_dir [file nativename [file join [pwd] \"$modeldir\"]]" );

	s = get_fun_name( str );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		choice = 0;
		goto loop;
	}

	cmd( "set eqname \"%s\"", s );

	cmd( "set frmt \"%%d %%B, %%Y\"" );
	cmd( "set last \"[clock format [file mtime \"$modeldir/$eqname\"] -format \"$frmt\"]\"" );

	cmd( "set mname $modelname" );
	cmd( "set mver $version" );

	cmd( "newtop .a \"Model Info\" { set choice 2 }" );

	cmd( "frame .a.mname" );
	cmd( "label .a.mname.l -text \"Model name\"" );
	cmd( "entry .a.mname.e -width 25 -textvariable mname -justify center" );
	cmd( "pack .a.mname.l .a.mname.e" );

	cmd( "frame .a.mver" );
	cmd( "label .a.mver.l -text \"Version\"" );
	cmd( "entry .a.mver.e -width 10 -textvariable mver -justify center" );
	cmd( "pack .a.mver.l .a.mver.e" );

	cmd( "frame .a.mdir" );
	cmd( "label .a.mdir.l -text \"Model home subdirectory\"" );
	cmd( "entry .a.mdir.e -width 35 -state disabled -textvariable complete_dir -justify center" );
	cmd( "pack .a.mdir.l .a.mdir.e" );

	cmd( "frame .a.date" );
	cmd( "label .a.date.l -text \"Creation date\"" );
	cmd( "entry .a.date.e -width 20 %s -textvariable date -justify center", i == 1 ? "-state disabled" : "" );
	cmd( "pack .a.date.l .a.date.e" );

	cmd( "frame .a.mdate" );
	cmd( "label .a.mdate.l -text \"Modification date (equations)\"" );
	cmd( "entry .a.mdate.e -width 20 -state disabled -textvariable last -justify center" );
	cmd( "pack .a.mdate.l .a.mdate.e" );

	cmd( "pack .a.mname .a.mver .a.mdir .a.date .a.mdate -padx 5 -pady 5" );

	cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
	cmd( "bind .a.mname.e <Return> {focus .a.mver.e; .a.mver.e selection range 0 end}" );
	cmd( "bind .a.mver.e <Return> {focus .a.b.ok}" );

	cmd( "showtop .a" );
	cmd( ".a.mname.e selection range 0 end" );
	cmd( "focus .a.mname.e" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 1 )
	{
		cmd( "set modelname $mname" );
		cmd( "set version $mver" );
		cmd( "set f [open \"$modeldir/modelinfo.txt\" w]" );
		cmd( "puts $f \"$modelname\"" );
		cmd( "puts $f \"$version\"" );
		cmd( "puts $f \"$date\"" );
		cmd( "close $f" );
		cmd( ".f.hea.mod.dat conf -text \"$modelname\"" );
		cmd( ".f.hea.ver.dat conf -text \"$version\"" );
	}

	choice = 0;
	goto loop;
}


// create a new file
if ( choice == 39 )
{
	cmd( ".f.t.t delete 1.0 end" );
	cmd( "set before [.f.t.t get 1.0 end]" );
	cmd( "set filename noname.txt" );
	cmd( "set dirname [pwd]" );
	cmd( ".f.t.t mark set insert 1.0" );
	cmd( ".f.hea.file.dat conf -text \"noname.txt\"" );
	cmd( "catch [unset -nocomplain ud]" );
	cmd( "catch [unset -nocomplain udi]" );
	cmd( "catch [unset -nocomplain rd]" );
	cmd( "catch [unset -nocomplain rdi]" );
	cmd( "lappend ud [.f.t.t get 0.0 end]" );
	cmd( "lappend udi [.f.t.t index insert]" );

	choice = 0;
	goto loop;
}


// create the makefile
if ( choice == 46 || choice == 49 )
{
	make_makefile( );

	if ( choice == 46 )
		choice = 0;		//just create the makefile
	if ( choice == 49 )	//after this show the description file (and a model is created)
		choice = 50;
	  
	goto loop;
} 


// System Options
if ( choice == 47 )
{
	cmd( "set choice [file exists \"$LsdSrc/system_options.txt\"]" );
	if ( choice == 1 )
	{
		cmd( "set f [open \"$LsdSrc/system_options.txt\" r]" );
		cmd( "set a [read -nonewline $f]" );
		cmd( "close $f" );
		choice = 0;
	}
	else
		cmd( "set a \"\"" );

	cmd( "newtop .l \"System Options\" { set choice 2 }" );

	cmd( "frame .l.t" );
	cmd( "scrollbar .l.t.yscroll -command \".l.t.text yview\"" );
	cmd( "text .l.t.text -undo 1 -wrap word -font \"$DefaultFont $small_character normal\" -width 70 -height 20 -yscrollcommand \".l.t.yscroll set\"" );
	cmd( ".l.t.text insert end $a" );
	cmd( "pack .l.t.yscroll -side right -fill y" );

	cmd( "frame .l.t.d" );

	cmd( "frame .l.t.d.os" );
	cmd( "if [ string equal $tcl_platform(machine) intel ] { \
			button .l.t.d.os.win -width [ expr $butWid + 6 ] -text \"Default Windows32\" -command { .l.t.text delete 1.0 end; \
				set file [ open \"$RootLsd/$LsdSrc/sysopt_win32.txt\" r ]; \
				set a [ read -nonewline $file ]; \
				close $file; \
				.l.t.text insert end \"LSDROOT=[pwd]\\n\"; \
				.l.t.text insert end \"$a\" \
			} \
		} else { \
			button .l.t.d.os.win -width [ expr $butWid + 6 ] -text \"Default Windows64\" -command { \
				.l.t.text delete 1.0 end; \
				set file [ open \"$RootLsd/$LsdSrc/sysopt_win64.txt\" r ]; \
				set a [ read -nonewline $file ]; \
				close $file; \
				.l.t.text insert end \"LSDROOT=[pwd]\\n\"; \
				.l.t.text insert end \"$a\" \
			} \
		} " ); 
	cmd( "button .l.t.d.os.lin -width [ expr $butWid + 6 ] -text \"Default Linux\" -command { \
			.l.t.text delete 1.0 end; \
			set file [open \"$RootLsd/$LsdSrc/sysopt_linux.txt\" r]; \
			set a [read -nonewline $file]; \
			close $file; \
			.l.t.text insert end \"LSDROOT=[pwd]\\n\"; \
			.l.t.text insert end \"$a\" \
		}" );
#ifdef MAC_PKG
	cmd( "button .l.t.d.os.mac -width [ expr $butWid + 6 ] -text \"Default MacOS\" -command { \
			.l.t.text delete 1.0 end; \
			set file [open \"$RootLsd/$LsdSrc/sysopt_mac.txt\" r]; \
			set a [read -nonewline $file]; \
			close $file; \
			.l.t.text insert end \"LSDROOT=[pwd]\\n\"; \
			.l.t.text insert end \"$a\" \
		}" ); 
#else
	cmd( "button .l.t.d.os.mac -width [ expr $butWid + 6 ] -text \"Default OSX\" -command { \
			.l.t.text delete 1.0 end; \
			set file [open \"$RootLsd/$LsdSrc/sysopt_osx.txt\" r]; \
			set a [read -nonewline $file]; \
			close $file; \
			.l.t.text insert end \"LSDROOT=[pwd]\\n\"; \
			.l.t.text insert end \"$a\" \
		}" ); 
#endif
	cmd( "pack .l.t.d.os.win .l.t.d.os.lin .l.t.d.os.mac -padx 10 -pady 5 -side left" );

	cmd( "pack .l.t.d.os" );
	cmd( "pack .l.t.text .l.t.d" );
	cmd( "pack .l.t" );

	cmd( "okhelpcancel .l b { set choice 1 } { LsdHelp LMM.html#compilation_options } { set choice 2 }" );

	cmd( "showtop .l" );
	cmd( "focus .l.t.text" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	if ( choice == 1 )
	{
		cmd( "set f [open \"$LsdSrc/system_options.txt\" w]" );
		cmd( "puts -nonewline $f [.l.t.text get 1.0 end]" );
		cmd( "close $f" );
		choice = 46; 	//go to create makefile
	}
	else
		choice = 0; 

	cmd( "destroytop .l" );

	goto loop;
} 


// Model Options
if ( choice == 48 )
{
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}

	s = get_fun_name( str );
	if ( s == NULL || ! strcmp( s, "" ) )
		check_option_files( );

	cmd( "cd \"$modeldir\"" );

	cmd( "set b \"%s\"", s );
	cmd( "set f [ open model_options.txt r ]" );
	cmd( "set a [ read -nonewline $f ]" );
	cmd( "close $f" );

	cmd( "set gcc_conf \"TARGET=$DefaultExe\\nFUN=[file rootname \"$b\"]\\nFUN_EXTRA=\\nSWITCH_CC=\"" );
	cmd( "set gcc_deb \"$gcc_conf-O0 -ggdb3\\nSWITCH_CC_LNK=\"" );
	cmd( "set gcc_opt \"$gcc_conf-O3\\nSWITCH_CC_LNK=\"" );

	cmd( "set pos [ string first \"SWITCH_CC=\" $a ]; if { $pos == -1 } { set choice 0 } { if { [ string first \" -g\" $a $pos ] == -1 } { set debug 0 } { set debug 1 } }" );

	cmd( "newtop .l \"Model Options\" { set choice 2 }" );

	cmd( "frame .l.t" );
	cmd( "scrollbar .l.t.yscroll -command \".l.t.text yview\"" );
	cmd( "text .l.t.text -undo 1 -wrap word -font \"$DefaultFont $small_character normal\" -width 70 -height 16 -yscrollcommand \".l.t.yscroll set\"" );
	cmd( ".l.t.text insert end $a" );
	cmd( "pack .l.t.yscroll -side right -fill y" );

	cmd( "frame .l.t.d" );

	cmd( "frame .l.t.d.opt" );
	cmd( "checkbutton .l.t.d.opt.debug -text Debug -variable debug -command { \
			set a [.l.t.text get 1.0 end]; \
			set pos [ string first \"SWITCH_CC=\" $a ]; \
			if { $pos == -1 } { \
				.l.t.d.opt.def invoke \
			} else { \
				if $debug { \
					if { [ string first \" -g\" $a $pos ] != -1 } { \
						return \
					}; \
					set pos1 [ string first \"\n\" $a $pos ]; \
					if { $pos1 == -1 } { \
						set a \"$a -ggdb3\" \
					} else { \
						set a [ string replace $a $pos1 $pos1 \" -ggdb3\n\" ] \
					}; \
					set pos2 [ string first \"-O\" $a $pos ]; \
					if { $pos2 != -1 } { \
						set a [ string replace $a $pos2 $pos2+2 \"-O0\" ] \
					} \
				} else { \
					set pos1 [ string first \" -ggdb3\" $a $pos ]; \
					if { $pos1 != -1 } { \
						set a [ string replace $a $pos1 $pos1+6 \"\" ] \
					} else { \
						set pos1 [ string first \" -ggdb\" $a $pos ]; \
						if { $pos1 != -1 } { \
							set a [ string replace $a $pos1 $pos1+5 \"\" ] \
						} else { \
							set pos1 [ string first \" -g\" $a $pos ]; \
							if { $pos1 != -1 } { \
								set a [ string replace $a $pos1 $pos1+2 \"\" ] \
							} else { \
								return \
							} \
						} \
					}; \
					set pos2 [ string first \"-O\" $a $pos ]; \
					if { $pos2 != -1 } { \
						set a [ string replace $a $pos2 $pos2+2 \"-O3\" ] \
					} \
				}; \
				.l.t.text delete 1.0 end; \
				.l.t.text insert end \"[ string trim $a ]\n\" \
			} \
		}" );
	cmd( "button .l.t.d.opt.ext -width $butWid -text \"Add Extra\" -command { \
			set a [.l.t.text get 1.0 end]; \
			set pos [ string first \"FUN_EXTRA=\" $a ]; \
			if { $pos == -1 } { \
				.l.t.d.opt.def invoke; \
				set a [.l.t.text get 1.0 end]; \
				set pos [ string first \"FUN_EXTRA=\" $a ]; \
			}; \
			set fun_extra [ tk_getOpenFile -parent .l -title \"Select Additional Source Files\" -multiple yes -initialdir \"$modeldir\" -filetypes {{{C++ header files} {.h .hpp .h++}} {{C++ source files} {.c .cpp .c++}} {{All files} {*}} } ]; \
			if { $fun_extra == \"\" } { \
				return \
			}; \
			set extra_files [ list ]; \
			foreach x $fun_extra { \
				set dirlen [ string length $modeldir ]; \
				if { [ string equal -length $dirlen $modeldir $x ] } { \
					if { [ string index $x $dirlen ] == \"/\" || [ string index $x $dirlen ] == \"\\\\\" } {  \
						incr dirlen; \
					}; \
					lappend extra_files [ string range $x $dirlen end ] \
				} else { \
					lappend extra_files $x \
				} \
			}; \
			set pos1 [ string first \"\n\" $a $pos ]; \
			if { $pos1 == -1 } { \
				set a \"$a $extra_files\" \
			} else { \
				set a [ string replace $a $pos1 $pos1 \" $extra_files\n\" ] \
			}; \
			.l.t.text delete 1.0 end; \
			.l.t.text insert end \"[ string trim $a ]\n\" \
		}" );
	cmd( "button .l.t.d.opt.def -width $butWid -text \"Default\" -command { \
			if { $debug == 0 } { \
				set default \"$gcc_opt\" \
			} else { \
				set default \"$gcc_deb\" \
			}; \
			.l.t.text delete 1.0 end; \
			.l.t.text insert end \"$default\" \
		}" );
	cmd( "button .l.t.d.opt.cle -width $butWid -text \"Clean Obj.\" -command { \
			if { [ catch { glob \"$RootLsd/$LsdSrc/*.o\" } objs ] == 0 } { \
				foreach i $objs { \
					catch { \
						file delete -force \"$i\" \
					} \
				} \
			} \
		}" );
	cmd( "pack .l.t.d.opt.debug .l.t.d.opt.ext .l.t.d.opt.def .l.t.d.opt.cle -padx 10 -pady 5 -side left" );

	cmd( "pack .l.t.d.opt" );
	cmd( "pack .l.t.text .l.t.d" );
	cmd( "pack .l.t" );

	cmd( "okhelpcancel .l b { set choice 1 } { LsdHelp LMM.html#model_options } { set choice 2 }" );

	cmd( "showtop .l" );
	cmd( "focus .l.t.text" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	if ( choice == 1 )
	{
		cmd( "set f [open model_options.txt w]" );
		cmd( "puts -nonewline $f [.l.t.text get 1.0 end]" );
		cmd( "close $f" );
		choice = 46;		//go to create makefile
	}
	else
		choice = 0; 

	cmd( "cd \"$RootLsd\"" );

	cmd( "destroytop .l" );

	goto loop;
} 


// launch tkdiff
if ( choice == 57 )
{
	cmd( "destroytop .mm" );	// close compilation results, if open

	cmd( "if [ string equal $tcl_platform(platform) unix ] { set choice 1 } { set choice 2 }" );
	if ( choice == 1 )		// unix
		cmd( "exec $wish $LsdSrc/tkdiff.tcl &" );
	if ( choice == 2 )		// windows
		cmd( "exec $wish $LsdSrc/tkdiff.tcl &" );
	 
	choice = 0;
	goto loop;
}


// start tkdiff with model selection
if ( choice == 61 )
{
	cmd( "destroytop .mm" );	// close compilation results, if open

	choice = 0;
	cmd( "chs_mdl" );

	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	if ( choice == -1 )
	{
		choice = 0;
		goto loop;
	}
	 
	cmd( "if [ string equal $tcl_platform(platform) unix ] { set choice 1 } { set choice 2 }" );
	if ( choice == 1 )		// unix
		cmd( "exec $wish src/tkdiff.tcl [file join \"$d1\" \"$f1\"] [file join \"$d2\" \"$f2\"] &" );
	if ( choice == 2 )		// windows
		cmd( "exec $wish src/tkdiff.tcl [file join \"$d1\" \"$f1\"] [file join \"$d2\" \"$f2\"]  &" );
	 
	choice = 0;
	goto loop;
}


// change font
if ( choice == 59 )
{
	cmd( "set ifont \"$fonttype\"" );
	cmd( "set idim $dim_character" );

	cmd( "newtop .a \"Change Font\" { set choice 2 }" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Font family and size\"" );

	cmd( "frame .a.l.e" );
	cmd( "ttk::combobox .a.l.e.font -justify center -textvariable ifont -values [ font families ] -width 15" );
	cmd( "ttk::combobox .a.l.e.dim -justify center -textvariable idim -values [ list 4 6 8 9 10 11 12 14 18 24 32 48 60 ] -width 3" );
	cmd( "pack .a.l.e.font .a.l.e.dim -side left" );

	cmd( "pack .a.l.l .a.l.e" );
	cmd( "pack .a.l -padx 5 -pady 5" );

	cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
	cmd( "bind .a.l.e.font <Return> {invoke .a.b.ok}" );
	cmd( "bind .a.l.e.dim <Return> {invoke .a.b.ok}" );

	cmd( "showtop .a" );
	cmd( "focus .a.l.e.font" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}

	cmd( "set fonttype \"$ifont\"" );
	cmd( "set dim_character $idim" );
	cmd( "set a [ list \"$fonttype\" $dim_character ]; .f.t.t conf -font \"$a\"; settab .f.t.t $tabsize \"$a\"" );

	choice = 0;
	goto loop;
}


// LMM system options
if ( choice == 60 )
{
	cmd( "set temp_var1 \"$DbgTerm\"" );
	cmd( "set temp_var2 \"$HtmlBrowser\"" );
	cmd( "set temp_var3 \"$fonttype\"" );
	cmd( "set temp_var4 $wish" );
	cmd( "set temp_var5 $LsdSrc" );
	cmd( "set temp_var6 $dim_character" );
	cmd( "set temp_var7 $tabsize" );
	cmd( "set temp_var8 $wrap" );
	cmd( "set temp_var9 $shigh" );
	cmd( "set temp_var10 $autoHide" );
	cmd( "set temp_var11 $showFileCmds" );
	cmd( "set temp_var12 $LsdNew" );
	cmd( "set temp_var13 $DbgExe" );

	cmd( "newtop .a \"Options\" { set choice 2 }" );

	cmd( "frame .a.num" );
	cmd( "label .a.num.l -text \"Terminal for debugger\"" );
	cmd( "entry .a.num.v -width 25 -textvariable temp_var1 -justify center" );
	cmd( "pack .a.num.l .a.num.v" );
	cmd( "bind .a.num.v <Return> {focus .a.num13.v; .a.num13.v selection range 0 end}" );

	cmd( "frame .a.num13" );
	cmd( "label .a.num13.l -text \"Debugger command\"" );
	cmd( "entry .a.num13.v -width 25 -textvariable temp_var13 -justify center" );
	cmd( "pack .a.num13.l .a.num13.v" );
	cmd( "bind .a.num13.v <Return> {focus .a.num2.v; .a.num2.v selection range 0 end}" );

	cmd( "frame .a.num2" );
	cmd( "label .a.num2.l -text \"HTML browser\"" );
	cmd( "entry .a.num2.v -width 25 -textvariable temp_var2 -justify center" );
	cmd( "pack .a.num2.l .a.num2.v" );
	cmd( "bind .a.num2.v <Return> {focus .a.num4.v; .a.num4.v selection range 0 end}" );

	cmd( "frame .a.num4" );
	cmd( "label .a.num4.l -text \"Wish program\"" );
	cmd( "entry .a.num4.v -width 25 -textvariable temp_var4 -justify center" );
	cmd( "pack .a.num4.l .a.num4.v" );
	cmd( "bind .a.num4.v <Return> {focus .a.num12.v; .a.num12.v selection range 0 end}" );

	cmd( "frame .a.num12" );
	cmd( "label .a.num12.l -text \"New models subdirectory\"" );
	cmd( "entry .a.num12.v -width 25 -textvariable temp_var12 -justify center" );
	cmd( "pack .a.num12.l .a.num12.v" );
	cmd( "bind .a.num12.v <Return> {focus .a.num5.v; .a.num5.v selection range 0 end}" );

	cmd( "frame .a.num5" );
	cmd( "label .a.num5.l -text \"Source code subdirectory\"" );
	cmd( "entry .a.num5.v -width 25 -textvariable temp_var5 -justify center" );
	cmd( "pack .a.num5.l .a.num5.v" );
	cmd( "bind .a.num5.v <Return> {focus .a.num3.fv}" );

	cmd( "frame .a.num3" );
	cmd( "label .a.num3.l -text \"Font family and size\"" );
	cmd( "frame .a.num3.f" );
	cmd( "ttk::combobox .a.num3.f.v -justify center -textvariable temp_var3 -values [ font families ] -width 15" );
	cmd( "ttk::combobox .a.num3.f.s -justify center -textvariable temp_var6 -values [ list 4 6 8 9 10 11 12 14 18 24 32 48 60 ] -width 3" );
	cmd( "pack .a.num3.f.v .a.num3.f.s -side left" );
	cmd( "pack .a.num3.l .a.num3.f" );
	cmd( "bind .a.num3.f.v <Return> {focus .a.num7.v; .a.num7.v selection range 0 end}" );

	cmd( "frame .a.num7" );
	cmd( "label .a.num7.l -text \"Tab size (characters)\"" );
	cmd( "entry .a.num7.v -width 2 -textvariable temp_var7 -justify center" );
	cmd( "pack .a.num7.l .a.num7.v" );
	cmd( "bind .a.num7.v <Return> {focus .a.num9.r.v3}" );

	cmd( "frame .a.num9" );
	cmd( "label .a.num9.l -text \"Syntax highlights\"" );

	cmd( "frame .a.num9.r -relief groove -bd 2" );
	cmd( "radiobutton .a.num9.r.v1 -variable temp_var9 -value 0 -text None" );
	cmd( "radiobutton .a.num9.r.v2 -variable temp_var9 -value 1 -text Partial" );
	cmd( "radiobutton .a.num9.r.v3 -variable temp_var9 -value 2 -text Full" );
	cmd( "pack .a.num9.r.v1 .a.num9.r.v2 .a.num9.r.v3 -side left" );

	cmd( "pack .a.num9.l .a.num9.r" );
	cmd( "bind .a.num9.r.v1 <Return> { focus .a.num8.v_num8 }" );
	cmd( "bind .a.num9.r.v2 <Return> { focus .a.num8.v_num8 }" );
	cmd( "bind .a.num9.r.v3 <Return> { focus .a.num8.v_num8 }" );

	cmd( "frame .a.num8" );
	cmd( "checkbutton .a.num8.v_num8 -variable temp_var8 -text \"Wrap text\"" );
	cmd( "checkbutton .a.num8.v_num10 -variable temp_var10 -text \"Auto-hide on run\"" );
	cmd( "checkbutton .a.num8.v_num11 -variable temp_var11 -text \"Show text file commands\"" );
	cmd( "pack .a.num8.v_num8 .a.num8.v_num10 .a.num8.v_num11" );
	cmd( "bind .a.num8.v_num8 <Return> { focus .a.num8.v_num10 }" );
	cmd( "bind .a.num8.v_num10 <Return> { focus .a.num8.v_num11 }" );
	cmd( "bind .a.num8.v_num11 <Return> { focus .a.f2.ok }" );

	cmd( "pack .a.num .a.num13 .a.num2 .a.num4 .a.num12 .a.num5 .a.num3 .a.num7 .a.num9 .a.num8 -padx 5 -pady 5" );

	cmd( "frame .a.f1" );
	cmd( "button .a.f1.def -width $butWid -text Default -command {set temp_var1 \"$DefaultDbgTerm\"; set temp_var2 \"$DefaultHtmlBrowser\"; set temp_var3 \"$DefaultFont\"; set temp_var5 src; set temp_var6 $DefaultFontSize; set temp_var7 2; set temp_var8 1; set temp_var9 2; set temp_var10 0; set temp_var11 0; set temp_var12 Work; set temp_var13 \"$DefaultDbgExe\"}" );
	cmd( "button .a.f1.help -width $butWid -text Help -command {LsdHelp LMM.html#SystemOpt}" );
	cmd( "pack .a.f1.def .a.f1.help -padx 10 -side left" );
	cmd( "pack .a.f1 -anchor e" );

	cmd( "okcancel .a f2 { set choice 1 } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.num.v" );
	cmd( ".a.num.v selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 1 )
	{
		cmd( "if { $showFileCmds != $temp_var11 } { tk_messageBox -parent . -icon warning -title Warning -type ok -message \"Restart required\" -detail \"Restart required after configuration changes. Only after LMM is closed and restarted the changes will be applied.\" }" );

		cmd( "set DbgTerm \"$temp_var1\"" );
		cmd( "set HtmlBrowser \"$temp_var2\"" );
		cmd( "set fonttype \"$temp_var3\"" );
		cmd( "set wish $temp_var4" );
		cmd( "set LsdSrc $temp_var5" );
		cmd( "if [string is integer -strict $temp_var6] {set dim_character $temp_var6}" );
		cmd( "if [string is integer -strict $temp_var7] {set tabsize $temp_var7}" );
		cmd( "set wrap $temp_var8" );
		cmd( "set shigh $temp_var9" );
		cmd( "set autoHide $temp_var10" );
		cmd( "set showFileCmds $temp_var11" );
		cmd( "set LsdNew $temp_var12" );
		cmd( "set DbgExe $temp_var13" );

		cmd( "set a [ list \"$fonttype\" $dim_character ]" );
		cmd( ".f.t.t conf -font \"$a\"" );
		cmd( "settab .f.t.t $tabsize \"$a\"" );	// adjust tabs size to font type/size
		cmd( "setwrap .f.t.t $wrap" );			// adjust text wrap
		color( shigh, 0, 0 );					// set color highlights (all text)

		// save to config file
		cmd( "set f [open \"$RootLsd/lmm_options.txt\" w]" );
		cmd( "puts $f  \"$DbgTerm\"" );
		cmd( "puts $f \"$HtmlBrowser\"" );
		cmd( "puts $f \"$fonttype\"" );
		cmd( "puts $f $wish" );
		cmd( "puts $f $LsdSrc" );
		cmd( "puts $f $dim_character" );
		cmd( "puts $f $tabsize" );
		cmd( "puts $f $wrap" );
		cmd( "puts $f $shigh" );
		cmd( "puts $f $autoHide" );
		cmd( "puts $f $showFileCmds" );
		cmd( "puts $f $LsdNew" );
		cmd( "puts $f  \"$DbgExe\"" );
		cmd( "close $f" );
	}

	choice = 0;
	goto loop;
}


// generate the no window distribution
if ( choice == 62 )
{
	// copy the base LSD source files to distribution directory
	cmd( "if { ! [ file exists \"$modeldir/src\" ] } { file mkdir \"$modeldir/src\" }" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/lsdmain.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/main_gnuwin.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/file.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/variab.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/object.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/report.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/util.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/nets.cpp\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/fun_head.h\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/fun_head_fast.h\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/decl.h\" \"$modeldir/src\"" );
	cmd( "file copy -force \"$RootLsd/$LsdSrc/system_options.txt\" \"$modeldir/src\"" );
	
	// copy Eigen library files if in use, just once to save time
	if( use_eigen( ) )
		cmd( "if { ! [ file exists \"$modeldir/src/Eigen\" ] } { file copy -force \"$RootLsd/$LsdSrc/Eigen\" \"$modeldir/src\" }" );

	// define the no window compilation macro
	cmd( "set f [ open \"$modeldir/src/choose.h\" w ]" );
	cmd( "puts -nonewline $f \"#define NO_WINDOW\\n\"" );
	cmd( "close $f" );

	// create makefileNW and compile a local machine version of lsd_gnuNW
	compile_run( false, true );

	choice = 0;
	goto loop;
}


// Full syntax highlighting
if ( choice == 64 )
{
	shigh = 2;
	Tcl_UpdateLinkedVar( inter, "shigh" );
	color( shigh, 0, 0 );			// set color types (all text)

	choice = 0;
	goto loop;
}


// Partial syntax highlighting
if ( choice == 65 )
{
	shigh = 1;
	Tcl_UpdateLinkedVar( inter, "shigh" );
	// select entire text to be color adjusted
	color( shigh, 0, 0 );			// set color types (all text)

	choice = 0;
	goto loop;
}


// No syntax highlighting
if ( choice == 66 )
{
	shigh = 0;
	Tcl_UpdateLinkedVar( inter, "shigh" );
	color( shigh, 0, 0 );			// set color types (all text)

	choice = 0;
	goto loop;
}


// Change tab size
if ( choice == 67 )
{
	cmd( "newtop .a \"Tab Size\" { set choice 2 }" );

	cmd( "frame .a.l" );
	cmd( "label .a.l.l -text \"Tab size (characters)\"" );
	cmd( "entry .a.l.v_num -justify center -width 3 -textvariable tabsize -justify center" );
	cmd( "pack .a.l.l .a.l.v_num" );

	cmd( "pack .a.l -padx 5 -pady 5" );

	cmd( "okcancel .a b { set choice 1 } { set choice 2 }" );
	cmd( "bind .a <KeyPress-Return> {focus .a.f.ok}" );

	cmd( "showtop .a" );
	cmd( "focus .a.l.v_num" );
	cmd( ".a.l.v_num selection range 0 end" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .a" );

	if ( choice == 2 )
	{
		choice = 0;
		goto loop;
	}
	cmd( "if [string is integer -strict $tabsize] {settab .f.t.t $tabsize \"[.f.t.t cget -font]\"}" );
	
	choice = 0;
	goto loop;
}


// Adjust context menu for LSD macros
if ( choice == 68 )
{	
	cmd( "destroy .v.i" );
	cmd( "menu .v.i -tearoff 0" );
	cmd( ".v.i add command -label \"EQUATION/FUNCTION\" -command {set choice 25} -accelerator Ctrl+E" );
	cmd( ".v.i add command -label \"V(...)\" -command {set choice 26} -accelerator Ctrl+V" );
	cmd( ".v.i add command -label \"CYCLE(...)\" -command {set choice 27} -accelerator Ctrl+C" );
	cmd( ".v.i add command -label \"SUM(...)\" -command {set choice 56} -accelerator Ctrl+U" );
	cmd( ".v.i add command -label \"INCR(...)\" -command {set choice 40} -accelerator Ctrl+I" );
	cmd( ".v.i add command -label \"MULT(...)\" -command {set choice 45} -accelerator Ctrl+M" );
	cmd( ".v.i add command -label \"SEARCH(...)\" -command {set choice 55} -accelerator Ctrl+A" );
	cmd( ".v.i add command -label \"SEARCH_CND(...)\" -command {set choice 30} -accelerator Ctrl+S" );
	cmd( ".v.i add command -label \"SORT(...)\" -command {set choice 31} -accelerator Ctrl+T" );
	cmd( ".v.i add command -label \"RNDDRAW(...)\" -command {set choice 54} -accelerator Ctrl+N" );
	cmd( ".v.i add command -label \"WRITE(...)\" -command {set choice 29} -accelerator Ctrl+W" );
	cmd( ".v.i add command -label \"ADDOBJ(...)\" -command {set choice 52} -accelerator Ctrl+O" );
	cmd( ".v.i add command -label \"DELETE(...)\" -command {set choice 53} -accelerator Ctrl+D" );
	cmd( ".v.i add command -label \"Network macros\" -command { set choice 72 } -accelerator Ctrl+K" );
	cmd( ".v.i add command -label \"Math functions\" -command {set choice 51} -accelerator Ctrl+H" );
	choice = 0;
	goto loop;
}


// Adjust context menu for LSD C++
if ( choice == 69 )
{	
	cmd( "destroy .v.i" );
	cmd( "menu .v.i -tearoff 0" );
	cmd( ".v.i add command -label \"LSD equation/function\" -command {set choice 25} -accelerator Ctrl+E" );
	cmd( ".v.i add command -label \"cal(...)\" -command {set choice 26} -accelerator Ctrl+V" );
	cmd( ".v.i add command -label \"for ( ; ; )\" -command {set choice 27} -accelerator Ctrl+C" );
	cmd( ".v.i add command -label \"sum(...)\" -command {set choice 56} -accelerator Ctrl+U" );
	cmd( ".v.i add command -label \"increment(...)\" -command {set choice 40} -accelerator Ctrl+I" );
	cmd( ".v.i add command -label \"multiply(...)\" -command {set choice 45} -accelerator Ctrl+M" );
	cmd( ".v.i add command -label \"search(...)\" -command {set choice 55} -accelerator Ctrl+A" );
	cmd( ".v.i add command -label \"search_var_cond(...)\" -command {set choice 30} -accelerator Ctrl+S" );
	cmd( ".v.i add command -label \"lsdqsort(...)\" -command {set choice 31} -accelerator Ctrl+T" );
	cmd( ".v.i add command -label \"draw_rnd\" -command {set choice 54} -accelerator Ctrl+N" );
	cmd( ".v.i add command -label \"write(...)\" -command {set choice 29} -accelerator Ctrl+W" );
	cmd( ".v.i add command -label \"add_n_objects2\" -command {set choice 52} -accelerator Ctrl+O" );
	cmd( ".v.i add command -label \"delete_obj\" -command {set choice 53} -accelerator Ctrl+D" );
	cmd( ".v.i add command -label \"Math functions\" -command {set choice 51} -accelerator Ctrl+H" );
	choice = 0;
	goto loop;
}


// Show extra source files
if ( choice == 70 )
{	
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		choice = 0;
		goto loop;
	}
	
	// Create model options file if it doesn't exist
	cmd( "set choice [ file exists \"$modeldir/model_options.txt\" ]" );
	if ( choice == 0 )
		make_makefile( );

	choice = 0;
	cmd( "set fapp [ file nativename \"$modeldir/model_options.txt\" ]" );
	s = ( char * ) Tcl_GetVar( inter, "fapp", 0 );
	if ( s == NULL || ( f = fopen( s, "r" ) ) == NULL )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Makefile not created\" -detail \"Please check 'Model Options' and 'System Options' in menu 'Model'.\"" );
		goto loop;
	}
	
	while ( fgets( str, MAX_LINE_SIZE - 1, f ) != NULL && strncmp( str, "FUN_EXTRA=", 10 ) );
	fclose( f );
	if ( strncmp( str, "FUN_EXTRA=", 10 ) || sscanf( str + 10, "%s", str1 ) < 1 )
	{
		cmd( "tk_messageBox -parent . -title Warning -icon warning -type ok -message \"No extra files defined\" -detail \"Open 'Model Options' in menu 'Model' and include all extra files names in the line starting with 'FUN_EXTRA='. Add the names after the '=' character and separate them with spaces or use 'Add Extra' button to select one or more files.\n\nIf there is no 'FUN_EXTRA=' line, click on 'Default' button first.\"" );
		goto loop;
	}
	i = strlen( str ) - 1;
	str[ i ] = '\0';				// remove LF
	if ( str[ --i ] == '\r' )
		str[ i ] = '\0';			// remove CR (Windows)
	
	cmd( "set fun_extra [ split [ string trim \"%s\" ] \" \t\" ]", str + 10 );
	cmd( "set extra_files [ list ]" );
	cmd( "foreach x $fun_extra { if { [ string trim $x ] != \"\" && ( [ file exists \"$x\" ] || [ file exists \"$modeldir/$x\" ] ) } { lappend extra_files \"$x\" } }" );
	cmd( "set brr \"\"" );
	cmd( "set e .extra" );

	cmd( "newtop $e \"Extra Files\" { set choice 2 }"  );

	cmd( "frame $e.lf " );
	cmd( "label $e.lf.l1 -justify center -text \"Show additional\nsource files for model:\"" );
	cmd( "label $e.lf.l2 -fg red -text \"$modelname\"" );
	cmd( "pack $e.lf.l1 $e.lf.l2" );
	
	cmd( "frame $e.l" );
	cmd( "scrollbar $e.l.v_scroll -command \"$e.l.l yview\"" );
	cmd( "listbox $e.l.l -listvariable extra_files -width 30 -height 15 -selectmode single -yscroll \"$e.l.v_scroll set\"" );
	cmd( "pack $e.l.l $e.l.v_scroll -side left -fill y" );
	cmd( "mouse_wheel $e.l.l" );

	cmd( "set choice [ $e.l.l size ]" );
	if ( choice > 0 )
		cmd( "bind $e.l.l <Double-Button-1> { set brr [ .extra.l.l curselection ]; set choice 1 }" );
	else
		cmd( "$e.l.l insert end \"(none)\"" );

	cmd( "label $e.l3 -text \"(double-click to show the file)\"" );
	cmd( "label $e.l4 -text \"Extra files are added using\n'Model Options' in menu 'Model'\"" );
	
	cmd( "pack $e.lf $e.l $e.l3 $e.l4 -pady 5 -padx 5" );

	cmd( "okcancel $e b { set choice 1 } { set choice 2 }" );

	cmd( "showtop $e" );

	choice = 0;
	while ( choice == 0 )
		Tcl_DoOneEvent( 0 );
	
	cmd( "set i [ $e.l.l curselection ]" );
	cmd( "destroytop $e" );
	
	cmd( "if { $i == \"\" } { set brr \"\" } { set brr [ lindex $extra_files $i ] }" );
	s = ( char * ) Tcl_GetVar( inter, "brr", 0 );
	
	if ( choice == 1 && strlen( s ) > 0 )
	{
		cmd( "if { ! [ file exists \"$brr\" ] && [ file exists \"$modeldir/$brr\" ] } { set brr \"$modeldir/$brr\" }" );
		choice = 71;
	}
	else
		choice = 0;
	
	goto loop;
}


// show the file/line/column containing error from compilation results window
if ( choice == 87 )
{	
	// check if file exists and normalize name for comparisons
	cmd( "if { [ file exists \"$errfil\" ] } { \
			set errfil \"[ file normalize \"$errfil\" ]\" \
		} elseif { $errfil != \"\" && [ file exists \"$modeldir/$errfil\" ] } { \
			set errfil \"[ file normalize \"$modeldir/$errfil\" ]\" \
		} else { \
			set errfil \"\" \
		}" );

	cmd( "if { $errfil == \"\" } { \
				set choice 0 \
			} { \
				if [ string equal -nocase [ file tail \"$errfil\" ] \"fun_head.h\" ] { \
					tk_messageBox -parent . -title Warning -icon warning -type ok -message \"Error in LSD macro expansion\" -detail \"Please check the offending file and line in the description following the error message.\"; \
					set choice 0 \
				} else { \
					set choice 1 \
				} \
			}" );
	if ( choice == 0 )
		goto loop;				// insufficient data to show error
	
	// check if file is already loaded
	cmd( "if { [ string equal \"$errfil\" \"[ file normalize \"$dirname/$filename\" ]\" ] } { \
				set choice 1 \
			} { \
				set choice 0 \
			}" );
	if ( choice == 0 )
	{	
		// check if main equation file is not the current file
		s = get_fun_name( str );
		if ( s != NULL && strlen( s ) > 0 )
			cmd( "if [ string equal \"$errfil\" \"[ file normalize \"$modeldir/%s\" ]\" ] { set choice 8 }", s );
												// open main equation file
												
		// try to open an extra file defined by the user
		if ( choice == 0 )
		{
			// open the configuration file
			cmd( "set fapp [ file nativename \"$modeldir/model_options.txt\" ]" );
			s = ( char * ) Tcl_GetVar( inter, "fapp", 0 );
			if ( s == NULL || ( f = fopen( s, "r" ) ) == NULL )
			{
				cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Makefile not created\" -detail \"Please check 'Model Options' and 'System Options' in menu 'Model' and then 'Recompile'.\"" );
				goto loop;
			}

			// try to extract the extra files list
			while ( fgets( str, MAX_LINE_SIZE - 1, f ) != NULL && strncmp( str, "FUN_EXTRA=", 10 ) );
			fclose( f );
			// make sure extra section exists and have elements
			if ( ! strncmp( str, "FUN_EXTRA=", 10 ) && sscanf( str + 10, "%s", str1 ) > 0 )
			{
				i = strlen( str ) - 1;
				str[ i ] = '\0';				// remove LF
				if ( str[ --i ] == '\r' )
					str[ i ] = '\0';			// remove CR (Windows)
				
				// search error file in the extra files list
				cmd( "set fun_extra [ split [ string trim \"%s\" ] \" \t\" ]", str + 10 );
				cmd( "foreach x $fun_extra { \
						set x \"[ string trim $x ]\"; \
						if { $x != \"\" } { \
							if { [ file exists \"$x\" ] } { \
								set x \"[ file normalize \"$x\" ]\" \
							} elseif { [ file exists \"$modeldir/$x\" ] } { \
								set x \"[ file normalize \"$modeldir/$x\" ]\" \
							} else { \
								set x \"\" \
							}; \
							if { $x != \"\" && [ string equal \"$errfil\" \"$x\" ] } { \
								set choice 71; \
								break \
							} \
						} \
					}" );
			}
			
			if ( choice == 0 )
			{
				cmd( "tk_messageBox -parent .mm -title Warning -icon warning -type ok -message \"File not tracked\" -detail \"LMM is able to show error only in your main equation file or in extra files explicitly included.\n\nIf you want to track additional extra files, please open 'Model Options' in menu 'Model' and include all (additional) extra files names at the end of the line starting with 'FUN_EXTRA='. Add the names after the '=' character and separate them with spaces or use 'Add Extra' button to select one or more files.\n\nIf there is no 'FUN_EXTRA=' line, click on 'Default' button first.\"" );
				goto loop;				// file not available
			}
		}
	}
	
	// show the error position on screen
	cmd( "wm deiconify ." );
	cmd( "focus -force .f.t.t" );

	if ( choice == 1 )
	{	// file already loaded, just point error
		cmd( "if { [ info exists errlin ] && $errlin != \"\" && [ string is integer -strict $errlin ] } { \
				.f.t.t tag remove sel 1.0 end; \
				.f.t.t tag add sel $errlin.0 $errlin.end; \
				if { [ info exists errcol ] && $errcol != \"\" && [ string is integer -strict $errcol ] } { \
					.f.t.t see $errlin.$errcol; \
					.f.t.t mark set insert $errlin.$errcol \
				} else { \
					.f.t.t see $errlin.0; \
					.f.t.t mark set insert $errlin.0 \
				} \
			}" );
		choice = 0;						// nothing else to do
	}
	else
		// load the found extra file as correct "choice" is already set
		cmd( "set brr \"$errfil\"" );
	
	goto loop;
}


if ( choice != 1 )
{
	choice = 0;
	goto loop;
}

Tcl_UnlinkVar( inter, "choice");
Tcl_UnlinkVar( inter, "num");
Tcl_UnlinkVar( inter, "tosave");
Tcl_UnlinkVar( inter, "macro");
Tcl_UnlinkVar( inter, "shigh");

cmd( "LsdExit" );

return 0;
}


/*************************************
 CMD
 *************************************/
bool firstCall = true;

// enhanced cmd with embedded sprintf capabilities and integrated buffer underrun protection
void cmd( const char *cm, ... )
{
	char message[ TCL_BUFF_STR ];
	
	// abort if Tcl interpreter not initialized
	if ( inter == NULL )
	{
		printf( "\nTcl interpreter not initialized. Quitting LSD now.\n" );
		abort( );
	}
	
	if ( strlen( cm ) >= TCL_BUFF_STR )
	{
		sprintf( message, "Tcl buffer overrun. Please increase TCL_BUFF_STR to at least %ld bytes.", strlen( cm ) );
		log_tcl_error( cm, message );
		if ( tk_ok )
			cmd( "tk_messageBox -type ok -title Error -icon warning -message \"Tcl buffer overrun (memory corrupted!)\" -detail \"Save your data and close LMM after pressing 'OK'.\"" );
	}

	char buffer[ TCL_BUFF_STR ];
	va_list argptr;
	
	va_start( argptr, cm );
	int reqSz = vsnprintf( buffer, TCL_BUFF_STR, cm, argptr );
	va_end( argptr );
	
	if ( reqSz >= TCL_BUFF_STR )
	{
		sprintf( message, "Tcl buffer too small. Please increase TCL_BUFF_STR to at least %d bytes.", reqSz + 1 );
		log_tcl_error( cm, message );
		if ( tk_ok )
			cmd( "tk_messageBox -type ok -title Error -icon error -message \"Tcl buffer too small\" -detail \"Tcl/Tk command was canceled.\"" );
	}
	else
	{
		int code = Tcl_Eval( inter, buffer );

		if ( code != TCL_OK )
		{
			log_tcl_error( cm, Tcl_GetStringResult( inter ) );
			if ( tk_ok )
				cmd( "tk_messageBox -type ok -title Error -icon error -message \"Tcl error\" -detail \"More information in file 'LMM.err'.\"" );
		}
	}
}


/*************************************
 LOG_TCL_ERROR
 *************************************/
void log_tcl_error( const char *cm, const char *message )
{
	FILE *f;
	char fname[ MAX_PATH_LENGTH ];
	time_t rawtime;
	struct tm *timeinfo;
	char ftime[ 80 ];

	if ( strlen( rootLsd ) > 0 )
		sprintf( fname, "%s/LMM.err", rootLsd );
	else
		sprintf( fname, "LMM.err" );
	
	f = fopen( fname, "a" );
	if ( f == NULL )
	{
		printf( "\nCannot write log file to disk. Check write permissions\n" );
		return;
	}

	time( &rawtime );
	timeinfo = localtime( &rawtime );
	strftime ( ftime, 80, "%x %X", timeinfo );

	if ( firstCall )
	{
		firstCall = false;
		fprintf( f,"\n\n====================> NEW TCL SESSION\n" );
	}
	fprintf( f, "\n(%s)\nCommand:\n%s\nMessage:\n%s\n-----\n", ftime, cm, message );
	fclose( f );
}


/*********************************
 COLOR
 *********************************/
// data structures for color syntax (used by color/rm_color)
struct hit
{
	int type, count;
	long iniLin, iniCol;
	char previous, next;
};
// color types (0-n) to Tk tags mapping
const char *cTypes[ ] = {"comment1", "comment2", "cprep", "str", "lsdvar", "lsdmacro", "ctype", "ckword"};
// regular expressions identifying colored text types
 const char *cRegex[ ] = {
	"/\[*].*\[*]/",		// each item define one different color
	"//.*",
	"^(\\s)*#\[^/]*",
	"\\\"\[^\\\"]*\\\"",
	"v\\[\[0-9]{1,3}]|curl?\[1-9]?|root|up|next|hook",
	"MODEL(BEGIN|END)|(END_)?EQUATION|FUNCTION|RESULT|ABORT|DEBUG_(START|STOP)(_AT)?|CURRENT|VL?S?|V_(CHEATL?S?|NODEIDS?|NODENAMES?|LINKS?|EXTS?|LAT)|SUM|SUML?S?|COUNTS?(_ALLS?)?|STATS?|STAT_(NETS?|NODES?)|(WHT)?AVEL?S?|SDL?S?|INCRS?|MULTS?|CYCLES?|CYCLE_(EXTS?|LINKS?)|CYCLE2?3?_SAFES?|MAXL?S?|MINL?S?|WRITEL?L?S?|WRITE_(NODEIDS?|NODENAMES?|LINK|EXTS?|LAT)|SEARCH_CNDL?S?|SEARCHS?|SEARCH_(NODES?|LINKS?)|TSEARCHS?|SORT2?S?|ADDN?OBJL?S?|ADDN?OBJ_EXL?S?|ADD(NODES?|LINKW?S?|EXTS?)|DELETE|DELETE_(EXTS?|NETS?|NODES?|LINKS?)|RND|RND_(GENERATOR|SEED|SETSEED)|RNDDRAWL?S?|RNDDRAW_(FAIRS?|TOTL?S?|NODES?|LINKS?)|DRAWPROB_(NODES?|LINK)|PARAMETER|INTERACTS?|P?LOG|INIT_(TSEARCHT?S?|NETS?|LAT)|LOAD_NETS?|SAVE_(NETS?|LAT)|(SNAP|SHUFFLE)_NETS?|LINK(TO|FROM)|EXTS?|(P|DO|EXEC)_EXTS?|(USE|NO)_NAN|PATH|CONFIG|(LAST_)?T|SLEEP|FAST(_FULL)?|OBSERVE|RECALCS?|DEFAULT_RESULT|abs|min|max|round|(sq|cb)rt|pow|exp|log(10)?|fact|(t|l)?gamma|a?sin|a?cos|a?tan|pi|is_(finite|inf|nan)|uniform(_int)?|l?norm(cdf)?|poisson(cdf)?|beta(cdf)?|alapl(cdf)?|unifcdf|gammacdf|close_sim",
	"auto|const|double|float|int|short|struct|unsigned|long|signed|void|enum|volatile|char|extern|static|union|asm|bool|explicit|template|typename|class|friend|private|inline|public|virtual|mutable|protected|wchar_t",
	"break|continue|else|for|switch|case|default|goto|sizeof|typedef|do|if|return|while|dynamic_cast|namespace|reinterpret_cast|try|new|static_cast|typeid|catch|false|operator|this|using|throw|delete|true|const_cast|cin|endl|iomanip|main|npos|std|cout|include|iostream|NULL|string"
};

// count words in a string (used by color)
int strwrds(char string[ ])
{
	int i = 0, words = 0;
	char lastC = '\0';
	if (string == NULL) return 0;
	while (isspace(string[ i ]) ) i++;
	if (string[ i ] == '\0') return 0;
	for ( ; string[ i ] != '\0'; lastC = string[i++])
		if (isspace(string[ i ]) && ! isspace(lastC) ) words++;
	if (isspace(lastC) ) return words;
	else return words + 1;
}

// map syntax highlight level to the number of color types to use
#define ITEM_COUNT( ptrArray )  ( sizeof( ptrArray ) / sizeof( ptrArray[0] ) )
int map_color(int hiLev)
{
	if (hiLev == 0)
		return 0;
	if (hiLev == 1)
		return 4;
	if (ITEM_COUNT(cTypes) > ITEM_COUNT(cRegex) )
		return ITEM_COUNT(cRegex);
	return ITEM_COUNT(cTypes);
}

// compare function for qsort to compare different color hits (used by color)
int comphit(const void *p1, const void *p2)
{
	if (((hit*)p1)->iniLin < ((hit*)p2)->iniLin) return -1;
	if (((hit*)p1)->iniLin > ((hit*)p2)->iniLin) return 1;
	if (((hit*)p1)->iniCol < ((hit*)p2)->iniCol) return -1;
	if (((hit*)p1)->iniCol > ((hit*)p2)->iniCol) return 1;
	if (((hit*)p1)->type < ((hit*)p2)->type) return -1;
	if (((hit*)p1)->type > ((hit*)p2)->type) return 1;
	return 0;
}

/* New color routine, using new tcl/tk 8.5 search for all feature */
#define TOT_COLOR ITEM_COUNT( cTypes )
void color(int hiLev, long iniLin, long finLin)
{
int i, maxColor, newCnt;
long j, k, tsize = 0, curLin = 0, curCol = 0, newLin, newCol, size[TOT_COLOR];
char type, *pcount, *ppos, *count[TOT_COLOR], *pos[TOT_COLOR], finStr[16], *s;
struct hit *hits;

// prepare parameters
maxColor = map_color(hiLev);	// convert option to # of color types
if (finLin == 0)			// convert code 0 for end of text
	sprintf(finStr, "end");
else
	sprintf(finStr, "%ld.end", finLin);

// remove color tags
for (i = 0; i < TOT_COLOR; ++i )
{
	cmd( ".f.t.t tag remove %s %ld.0 %s", cTypes[ i ], iniLin == 0 ? 1 : iniLin, finStr );
}

// find & copy all occurrence types to arrays of C strings
for (i = 0; i < maxColor; ++i )
{
	// locate all occurrences of each color group
	Tcl_UnsetVar( inter, "ccount", 0 );
	if (! strcmp(cTypes[ i ], "comment1") )	// multi line search element?
		cmd( "set pos [.f.t.t search -regexp -all -nolinestop -count ccount -- {%s} %ld.0 %s]", cRegex[ i ], iniLin == 0 ? 1 : iniLin, finStr);
	else
		cmd( "set pos [.f.t.t search -regexp -all -count ccount -- {%s} %ld.0 %s]", cRegex[ i ], iniLin == 0 ? 1 : iniLin, finStr );

	// check number of ocurrences
	pcount = ( char* ) Tcl_GetVar( inter, "ccount", 0 );
	size[ i ] = strwrds(pcount);
	if (size[ i ] == 0)				// nothing to do?
		continue;
	tsize += size[ i ];

	// do intermediate store in C memory
	count[ i ] = ( char* ) calloc(strlen(pcount) + 1, sizeof( char ) );
	strcpy(count[ i ], pcount);
	ppos = ( char* ) Tcl_GetVar( inter, "pos", 0 );
	pos[ i ] = ( char* ) calloc(strlen(ppos) + 1, sizeof( char ) );
	strcpy(pos[ i ], ppos); 
}
if (tsize == 0)
	return;							// nothing to do

// organize all occurrences in a single array of C numbers (struct hit)
hits = (hit*)calloc(tsize, sizeof(hit) );
for (i = 0, k = 0; i < maxColor; ++i )
{
	if (size[ i ] == 0)				// nothing to do?
		continue;
	pcount = ( char* ) count[ i ] - 1;
	ppos = ( char* ) pos[ i ] - 1;
	for (j = 0; j < size[ i ] && k < tsize; j++, ++k )
	{
		hits[k].type = i;
		s = strtok(pcount + 1, " \t");
		hits[k].count = atoi(s);
		pcount = s + strlen(s);
		s = strtok(ppos + 1, " \t");
		sscanf(strtok( s, " \t"), "%ld.%ld", &hits[k].iniLin, &hits[k].iniCol);
		ppos = s + strlen(s);
	}
	free(count[ i ]);
	free(pos[ i ]);
}

// Sort the single list for processing
qsort((void *)hits, tsize, sizeof(hit), comphit);

// process each occurrence, if applicable
Tcl_LinkVar( inter, "lin", ( char* ) &newLin, TCL_LINK_LONG | TCL_LINK_READ_ONLY);
Tcl_LinkVar( inter, "col", ( char* ) &newCol, TCL_LINK_LONG | TCL_LINK_READ_ONLY);
Tcl_LinkVar( inter, "cnt", ( char* ) &newCnt, TCL_LINK_INT | TCL_LINK_READ_ONLY);
for (k = 0; k < tsize; ++k )
	// skip occurrences inside other occurrence
	if (hits[k].iniLin > curLin || (hits[k].iniLin == curLin && hits[k].iniCol >= curCol) )
	{
		newLin = hits[k].iniLin;
		newCol = hits[k].iniCol;
		newCnt = hits[k].count;
		cmd( "set end [.f.t.t index \"$lin.$col + $cnt char\"]" );
		// treats each type of color case properly
		if (hits[k].type < 4)			// non token?
			cmd( ".f.t.t tag add %s $lin.$col $end", cTypes[hits[k].type]);
		else							// token - should not be inside another word
			cmd( "if {[regexp {\\w} [.f.t.t get \"$lin.$col - 1 any chars\"]]==0 && [regexp {\\w} [.f.t.t get $end]]==0} {.f.t.t tag add %s $lin.$col $end}", cTypes[hits[k].type] );
		// next search position
		ppos = ( char* ) Tcl_GetVar( inter, "end", 0 );
		sscanf(ppos, "%ld.%ld", &curLin, &curCol);
	}
Tcl_UnlinkVar( inter, "lin");
Tcl_UnlinkVar( inter, "col");
Tcl_UnlinkVar( inter, "cnt");
free(hits);
}


/*********************************
 MAKE_MAKEFILE
 *********************************/
void make_makefile( bool nw )
{
	char suffix[ 3 ] = "";

	if ( nw )
		strcpy( suffix, "NW" );

	check_option_files( );

	cmd( "set f [ open \"$modeldir/model_options.txt\" r ]" );
	cmd( "set a [ read -nonewline $f ]" );
	cmd( "close $f" );

	cmd( "set f [ open \"$RootLsd/$LsdSrc/system_options.txt\" r ]" );
	cmd( "set d [ read -nonewline $f ]" );
	cmd( "close $f" );
	
#ifdef MAC_PKG
	if ( nw )
		cmd( "set f [ open \"$RootLsd/$LsdSrc/makefile_base%s.txt\" r ]", suffix );
	else
		cmd( "if [ string equal $tcl_platform(os) Darwin ] { set f [ open \"$RootLsd/$LsdSrc/makefile_base_mac.txt\" r ] } { set f [ open \"$RootLsd/$LsdSrc/makefile_base.txt\" r ] }" );
#else
	cmd( "set f [ open \"$RootLsd/$LsdSrc/makefile_base%s.txt\" r ]", suffix );
#endif
	cmd( "set b [ read -nonewline $f ]" );
	cmd( "close $f" );

	cmd( "set c \"# Model compilation options\\n$a\\n\\n# System compilation options\\n$d\\nLSDROOT=$RootLsd\\n\\n# Body of makefile%s (from src/makefile_base%s.txt)\\n$b\"", suffix, suffix );
	cmd( "set f [ open \"$modeldir/makefile%s\" w ]", suffix );
	cmd( "puts -nonewline $f $c" );
	cmd( "close $f" );
}


/*********************************
 CHECK_OPTION_FILES
 *********************************/
void check_option_files( bool sys )
{
	int exists;
	Tcl_LinkVar( inter, "exists", ( char * ) &exists, TCL_LINK_BOOLEAN );
	
	if ( ! sys )
	{
		cmd( "set exists [ file exists \"$modeldir/model_options.txt\" ]" );
		
		if ( ! exists )
		{
			cmd( "set dir [ glob -nocomplain \"$modeldir/fun_*.cpp\" ]" );
			cmd( "if { $dir != \"\" } { set b [ file tail [ lindex $dir 0 ] ] } { set b \"fun_UNKNOWN.cpp\" }" );
			cmd( "set a \"TARGET=$DefaultExe\\nFUN=[file rootname \"$b\"]\\nFUN_EXTRA=\\nSWITCH_CC=-O3 -ggdb3\\nSWITCH_CC_LNK=\"" );
			cmd( "set f [ open \"$modeldir/model_options.txt\" w ]" );
			cmd( "puts -nonewline $f $a" );
			cmd( "close $f" );
		}
	}
	
	cmd( "set exists [ file exists \"$RootLsd/$LsdSrc/system_options.txt\"]" );
	if ( ! exists )
	{
		cmd( "if [ string equal $tcl_platform(platform) windows ] { \
				if [ string equal $tcl_platform(machine) intel ] { \
					set sysfile \"sysopt_win32.txt\" \
				} { \
					set sysfile \"sysopt_win64.txt\" \
				} \
			}" );
		cmd( "if [ string equal $tcl_platform(platform) unix ] { set sysfile \"sysopt_linux.txt\" }" );
#ifdef MAC_PKG
		cmd( "if [ string equal $tcl_platform(os) Darwin ] { set sysfile \"sysopt_mac.txt\" }" );
#else
		cmd( "if [ string equal $tcl_platform(os) Darwin ] { set sysfile \"sysopt_osx.txt\" }" );
#endif
		cmd( "set f [ open \"$RootLsd/$LsdSrc/system_options.txt\" w ]" );
		cmd( "set f1 [ open \"$RootLsd/$LsdSrc/$sysfile\" r ]" );
		cmd( "puts -nonewline $f \"LSDROOT=$RootLsd\\n\"" );
		cmd( "puts -nonewline $f [ read $f1 ]" );
		cmd( "close $f" );
		cmd( "close $f1" );    
	}
	
	Tcl_UnlinkVar( inter, "exists" );
}

/*********************************
 GET_FUN_NAME
 *********************************/
char *get_fun_name( char *str, bool nw )
{
	char *s;
	FILE *f;
	
	make_makefile( nw );

	cmd( "set fapp [ file nativename \"$modeldir/makefile%s\" ]", nw ? "NW" : "" );
	s = ( char * ) Tcl_GetVar( inter, "fapp", 0 );
  
	f = fopen( s, "r" );
	if ( f == NULL )
		goto error;
	
	fscanf( f, "%999s", str );
	while ( strncmp( str, "FUN=", 4 ) && fscanf( f, "%999s", str ) != EOF );   
	
	fclose( f );
	
	if ( strncmp( str, "FUN=", 4 ) != 0 )
		goto error;

	s = str + 4;
	strcat( s, ".cpp" );
	
	return s;
	
error:
	cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Makefile not found or corrupted\" -detail \"Please check 'Model Options' and 'System Options' in menu 'Model'.\"" );
	return NULL;
}


/*********************************
 USE_EIGEN
 *********************************/

bool use_eigen( void )
{
	bool nfound = true;
	char *path, *fun_file, full_name[ MAX_PATH_LENGTH ], buffer[ 2 * MAX_PATH_LENGTH ];
	FILE *f;
	
	path = ( char * ) Tcl_GetVar( inter, "modeldir", 0 );
	fun_file = get_fun_name( buffer, true );
	
	if( path == NULL || fun_file == NULL )
		return false;
	
	snprintf( full_name, MAX_PATH_LENGTH, "%s/%s", path, fun_file );
	f = fopen( full_name, "r" );
	if( f == NULL )
		return false;

	while ( fgets( buffer, 2 * MAX_PATH_LENGTH, f ) != NULL && 
			( nfound = strncmp( buffer, EIGEN, strlen( EIGEN ) ) ) );   
	
	fclose( f );
	
	if ( nfound )
		return false;
	
	return true;
}


/*********************************
 COMPILE_RUN
 *********************************/
bool compile_run( bool run, bool nw )
{
	bool ret = false;
	char *s, str[ 2 * MAX_PATH_LENGTH ];
	FILE *f;
	int res;
	Tcl_LinkVar( inter, "res", ( char * ) &res, TCL_LINK_INT );
	
	cmd( "destroytop .mm" );	// close any open compilation results window
	cmd( "cd \"$modeldir\"" );
	
	s = ( char * ) Tcl_GetVar( inter, "modelname", 0 );
	if ( s == NULL || ! strcmp( s, "" ) )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"No model selected\" -detail \"Choose an existing model or create a new one.\"" );
		goto end;
	}

	if ( ! run && ! nw )			// delete existing object file if it's just compiling
	{								// to force recompilation
		cmd( "set oldObj \"[ file rootname [ lindex [ glob -nocomplain fun_*.cpp ] 0 ] ].o\"" );
		cmd( "if { [ file exists \"$oldObj\" ] } { file delete \"$oldObj\" }" );
	}
  
	// get source name
	s = get_fun_name( str, nw );
	if ( s == NULL || ! strcmp( s, "" ) )
		goto end;
	cmd( "set fname \"%s\"", s );
	
	// get target exec name
	cmd( "set fapp [ file nativename \"$modeldir/makefile%s\" ]", nw ? "NW" : "" );
	s = ( char * )Tcl_GetVar( inter, "fapp", 0 );
	f = fopen( s, "r" );
	fscanf( f, "%999s", str );
	while ( strncmp( str, "TARGET=", 7 ) && fscanf( f, "%999s", str ) != EOF );
	fclose( f );
	if ( strncmp( str, "TARGET=", 7 ) != 0 )
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Makefile%s corrupted\" -detail \"Check 'Model Options' and 'System Options' in menu 'Model'.\"", nw ? "NW" : "" );
		goto end;
	}
	if ( nw )
		strcpy( str, "TARGET=lsd_gnuNW" );		// NW version use fixed name because of batches

	// show compilation banner
	cmd( "set init_time [clock seconds]" ); 
	cmd( "if { ! $autoHide || ! %d } { set parWnd .; set posWnd centerW } { set parWnd \"\"; set posWnd centerS }", run );
	cmd( "newtop .t \"Please Wait\" \"\" $parWnd" );
	if ( nw )
		cmd( "label .t.l1 -font {-weight bold } -text \"Making 'No Window' version of model...\"" );
	else
		cmd( "label .t.l1 -font {-weight bold } -text \"Making model...\"" );
	if ( run )
		if ( nw )
			cmd( "label .t.l2 -text \"The executable 'lsd_gnuNW' for this system is being created.\nThe make file 'makefileNW' and the 'src' folder are being created\nin the model folder and can be used to recompile the\n'No Window' version in other systems.\"" );
		else
			cmd( "label .t.l2 -text \"The system is checking the files modified since the last compilation and recompiling as necessary.\nOn success the new model program will be launched.\nOn failure a text window will show the compiling error messages.\"" );
	else
		cmd( "label .t.l2 -text \"The system is recompiling the model.\nOn failure a text window will show the compiling error messages.\"" );
	cmd( "pack .t.l1 .t.l2 -padx 5 -pady 5" );
	cmd( "showtop .t $posWnd" );
  
	// minimize LMM if required
	cmd( "set res $autoHide" );		// get auto hide status
	if ( res && run )				// hide LMM?
		cmd( "wm iconify ." );
		
	// start compilation
	cmd( "if [ string equal $tcl_platform(platform) windows ] { set add_exe \".exe\" } { set add_exe \"\" }" );  
	cmd( "if { [ string equal $tcl_platform(platform) windows ] && ! [ string equal $tcl_platform(machine) amd64 ] } { set res 1 } { set res 0 }" );
	if ( res == 0 )
		cmd( "catch { exec $MakeExe -f makefile%s 2> makemessage.txt } result", nw ? "NW" : "" ); 
	else
	{   // handle Windows 32-bit old compiler
		cmd( "set file [ open make.bat w ]" );
		cmd( "puts -nonewline $file \"$MakeExe -f makefile%s 2> makemessage.txt\\n\"", nw ? "NW" : "" );
		cmd( "close  $file" );
		cmd( "if [ file exists \"$RootLsd/$LsdSrc/system_options.txt\" ] { set res 0 } {set res 1 }" );
		cmd( "if [ file exists \"%s.exe\" ] { file rename -force \"%s.exe\" \"%sOld.exe\" }", str + 7, str + 7, str + 7 );
		cmd( "if [ file exists \"$RootLsd/$LsdGnu/bin/crtend.o\" ] { file copy -force \"$RootLsd/$LsdGnu/bin/crtend.o\" .; file copy -force \"$RootLsd/$LsdGnu/bin/crtbegin.o\" .; file copy -force \"$RootLsd/$LsdGnu/bin/crt2.o\" . }" );
		cmd( "catch { exec make.bat } result" );
		cmd( "file delete make.bat" );
		cmd( "if [ file exists crtend.o] { file delete crtend.o; file delete crtbegin.o; file delete crt2.o }" );
	}
   
	// close banner
	cmd( "destroytop .t" );
	
	// check compilation error
	cmd( "if { [ file size makemessage.txt] == 0 } { file delete makemessage.txt; set res 0 } { set res 1 }" );
	if ( res == 1 )
	{	// Check if the executable is newer than the compilation command, implying just warnings
		cmd( "set funtime [ file mtime \"$fname\"] " );
#ifdef MAC_PKG
		if ( ! nw )
			cmd( "if [ string equal $tcl_platform(os) Darwin ] { set pkgPath \"%s.app/Contents/MacOS/\" } { set pkgPath \"\" }", str + 7 );
		else
#endif
			cmd( "set pkgPath \"\"" );
			
		cmd( "if [ file exist \"${pkgPath}%s$add_exe\" ] { set exectime [ file mtime \"${pkgPath}%s$add_exe\" ] } { set exectime $init_time }", str + 7 , str + 7 );
		cmd( "if { $init_time < $exectime } { set res 0 }" );
	}
	if ( res == 1 )
	{ 	// real problem
		cmd( "set res $autoHide" );			// get auto hide status
		if ( run && res )					// auto unhide LMM if necessary
			cmd( "wm deiconify ." );  		// only reopen if error
		create_compresult_window( nw );		// show errors
	}
	else
	{
		if ( nw )
			cmd( "tk_messageBox -parent . -type ok -icon info -title \"No Window Version\" -message \"Compilation successful\" -detail \"LMM has created a non-graphical version of the model, to be transported on any system endowed with a GCC compiler and standard libraries.\\n\\nA local system version of the executable 'lsd_gnuNW$add_exe' was also generated in your current model folder and is ready to use in this computer.\\n\\nTo move the model in another system copy the content of the model's directory:\\n\\n$modeldir\\n\\nincluding also its new subdirectory 'src'.\\n\\nTo create a 'No Window' version of the model program follow these steps, to be executed within the directory of the model:\\n- compile with the command 'make -f makefileNW'\\n- run the model with the command 'lsd_gnuNW -f mymodelconf.lsd'\\n- the simulation will run automatically saving the results (for the variables indicated in the conf. file) in LSD result files named after the seed generator used.\"" );
		
		if ( run )
		{	// no problem - execute
#ifdef MAC_PKG
			cmd( "if [ string equal $tcl_platform(platform) windows ] { set res 3 } { if [ string equal $tcl_platform(os) Darwin ] { set res 2 } { set res 1 } }" );
#else
			cmd( "if [ string equal $tcl_platform(platform) windows ] { set res 3 } { set res 1 }" );
#endif
			switch ( res )
			{
				case 1:	// unix
					cmd( "catch { exec ./%s & } result", str + 7 );
					break;
				case 2:	// mac package
					cmd( "catch { exec open -F -n ./%s.app & } result", str + 7 );
					break;
				case 3:	// win2k, XP, 7, 8, 10...
					cmd( "catch { exec %s.exe & } result", str + 7 );
					break;
			}
		}
	}

	ret = true;
end:
	cmd( "cd \"$RootLsd\"" );
	Tcl_UnlinkVar( inter, "res" );
	return ret;
}


/*********************************
 CREATE_COMPRESULT_WINDOW
 *********************************/
void create_compresult_window( bool nw )
{
	cmd( "destroytop .mm" );
	cmd( "set cerr 1.0" );						// search start position in file
	cmd( "set error \" error:\"" );				// error string to be searched
	cmd( "set errfil \"\"" );
	cmd( "set errlin \"\"" );
	cmd( "set errcol \"\"" );

	cmd( "newtop .mm \"Compilation Errors%s\" { .mm.b.close invoke } \"\"", nw ? " (No Window Version)" : "" );

	cmd( "label .mm.lab -justify left -text \"- Each error is indicated by the file name and line number where it has been identified.\n- Check the relative file and search on the indicated line number, considering that the error may have occurred in the previous line.\n- Fix first errors at the beginning of the list, since the following errors may be due to previous ones.\n- Check the 'Readme.txt' in LSD installation directory for information on particular problems.\"" );
	cmd( "pack .mm.lab" );

	cmd( "frame .mm.t" );
	cmd( "scrollbar .mm.t.yscroll -command \".mm.t.t yview\"" );
	cmd( "set a [ list \"$fonttype\" $small_character ]" );
	cmd( "text .mm.t.t -yscrollcommand \".mm.t.yscroll set\" -wrap word -font \"$a\"" );
	cmd( "pack .mm.t.yscroll -side right -fill y" );
	cmd( "pack .mm.t.t -expand yes -fill both" );
	
	cmd( "pack .mm.t -expand yes -fill both" );
	
	cmd( "frame .mm.i" );
	
	cmd( "frame .mm.i.f" );
	cmd( "label .mm.i.f.l -text \"File:\"" );
	cmd( "label .mm.i.f.n -anchor w -width 50 -fg red" );
	cmd( "pack .mm.i.f.l .mm.i.f.n -side left" );

	cmd( "frame .mm.i.l" );
	cmd( "label .mm.i.l.l -text \"Line:\"" );
	cmd( "label .mm.i.l.n -anchor w -width 5 -fg red" );
	cmd( "pack .mm.i.l.l .mm.i.l.n -side left" );

	cmd( "frame .mm.i.c" );
	cmd( "label .mm.i.c.l -text \"Column:\"" );
	cmd( "label .mm.i.c.n -anchor w -width 5 -fg red" );
	cmd( "pack .mm.i.c.l .mm.i.c.n -side left" );

	cmd( "pack .mm.i.f .mm.i.l .mm.i.c -padx 10 -pady 5 -side left" );
	cmd( "pack .mm.i" );

	cmd( "frame .mm.b" );
	
	cmd( "button .mm.b.perr -width [ expr $butWid + 4 ] -text \"Previous Error\" -underline 0 -command { \
			set start \"$cerr linestart\"; \
			set errtemp [ .mm.t.t search -nocase -regexp -count errlen -backward -- $error $start 1.0];  \
			if { [ string length $errtemp ] != 0 } { \
				set cerr $errtemp; \
				.mm.t.t mark set insert $errtemp; \
				.mm.t.t tag remove sel 1.0 end; \
				.mm.t.t tag add sel \"$errtemp linestart\" \"$errtemp lineend\"; \
				.mm.t.t see $errtemp; \
				set errdat [ split [ .mm.t.t get \"$errtemp linestart\" \"$errtemp lineend\" ] : ]; \
				if { [ string length [ lindex $errdat 0 ] ] == 1 } { \
					set errfil \"[ lindex $errdat 0 ]:[ lindex $errdat 1 ]\"; \
					set idxfil 2 \
				} else { \
					set errfil \"[ lindex $errdat 0 ]\"; \
					set idxfil 1 \
				}; \
				if { $errfil != \"\" && [ llength $errdat ] > $idxfil && [ string is integer -strict [ lindex $errdat $idxfil ] ] } { \
					set errlin [ lindex $errdat $idxfil ] \
				} else { \
					set errfil \"\"; \
					set errlin  \"\" \
				}; \
				incr idxfil; \
				if { $errfil != \"\" && [ llength $errdat ] > $idxfil && [ string is integer -strict [ lindex $errdat $idxfil ] ] } { \
					set errcol [ lindex $errdat $idxfil ] \
				} else { \
					set errfil \"\"; \
					set errlin  \"\"; \
					set errcol \"\" \
				}; \
				.mm.i.f.n configure -text $errfil; \
				.mm.i.l.n configure -text $errlin; \
				.mm.i.c.n configure -text $errcol; \
			} \
		}" );
	cmd( "button .mm.b.gerr -width [ expr $butWid + 4 ] -text \"Go to Error\" -underline 0 -command { set choice 87 }" );
	cmd( "button .mm.b.ferr -width [ expr $butWid + 4 ] -text \"Next Error\" -underline 0 -command { \
			if { ! [ string equal $cerr 1.0 ] } { \
				set start \"$cerr lineend\" \
			} else { \
				set start 1.0 \
			}; \
			set errtemp [ .mm.t.t search -nocase -regexp -count errlen -- $error $start end ]; \
			if { [ string length $errtemp ] != 0 } { \
				set cerr $errtemp; \
				.mm.t.t mark set insert \"$errtemp + $errlen ch\"; \
				.mm.t.t tag remove sel 1.0 end; \
				.mm.t.t tag add sel \"$errtemp linestart\" \"$errtemp lineend\"; \
				.mm.t.t see $errtemp; \
				set errdat [ split [ .mm.t.t get \"$errtemp linestart\" \"$errtemp lineend\" ] : ]; \
				if { [ string length [ lindex $errdat 0 ] ] == 1 } { \
					set errfil \"[ lindex $errdat 0 ]:[ lindex $errdat 1 ]\"; \
					set idxfil 2 \
				} else { \
					set errfil \"[ lindex $errdat 0 ]\"; \
					set idxfil 1 \
				}; \
				if { $errfil != \"\" && [ llength $errdat ] > $idxfil && [ string is integer -strict [ lindex $errdat $idxfil ] ] } { \
					set errlin [ lindex $errdat $idxfil ] \
				} else { \
					set errfil \"\"; \
					set errlin  \"\" \
				}; \
				incr idxfil; \
				if { $errfil != \"\" && [ llength $errdat ] > $idxfil && [ string is integer -strict [ lindex $errdat $idxfil ] ] } { \
					set errcol [ lindex $errdat $idxfil ] \
				} else { \
					set errfil \"\"; \
					set errlin  \"\"; \
					set errcol \"\" \
				}; \
				.mm.i.f.n configure -text $errfil; \
				.mm.i.l.n configure -text $errlin; \
				.mm.i.c.n configure -text $errcol; \
			} \
		}" );
	cmd( "button .mm.b.close -width [ expr $butWid + 4 ] -text Done -underline 0 -command { unset -nocomplain errfil errlin errcol; destroytop .mm }" );
	cmd( "pack .mm.b.perr .mm.b.gerr .mm.b.ferr .mm.b.close -padx 10 -pady 10 -expand yes -fill x -side left" );
	cmd( "pack .mm.b -side right" );
	
	cmd( "bind .mm <p> { .mm.b.perr invoke }; bind .mm <P> { .mm.b.perr invoke }" );
	cmd( "bind .mm.t.t <Up> { .mm.b.perr invoke; break }" );
	cmd( "bind .mm.t.t <Left> { .mm.b.perr invoke; break }" );
	cmd( "bind .mm <g> { .mm.b.gerr invoke }; bind .mm <G> { .mm.b.gerr invoke }" );
	cmd( "bind .mm <n> { .mm.b.ferr invoke }; bind .mm <N> { .mm.b.ferr invoke }" );
	cmd( "bind .mm.t.t <Down> { .mm.b.ferr invoke; break }" );
	cmd( "bind .mm.t.t <Right> { .mm.b.ferr invoke; break }" );
	cmd( "bind .mm <d> { .mm.b.close invoke }; bind .mm <D> { .mm.b.close invoke }" );
	cmd( "bind .mm <Return> { .mm.b.gerr invoke }" );
	cmd( "bind .mm <Escape> { .mm.b.close invoke }" );

	cmd( "showtop .mm lefttoW no no no" );

	cmd( "if [ file exists \"$modeldir/makemessage.txt\" ] { set file [open \"$modeldir/makemessage.txt\"]; .mm.t.t insert end [read $file]; close $file } { .mm.t.t insert end \"(no compilation errors)\" }" );
	cmd( ".mm.t.t mark set insert \"1.0\"" );
	cmd( ".mm.b.ferr invoke" );
	
	cmd( ".mm.t.t configure -state disabled" );
	cmd( "wm deiconify .mm; raise .mm; focus .mm.t.t" );
	cmd( "update" );
}


/****************************************************
DISCARD_CHANGE
Ask user to discard changes in edited file, if applicable
Returns: 0: abort, 1: continue
****************************************************/
bool discard_change( void )
{
	if ( ! tosave )
		return true;					// yes: simply discard configuration
	
	// ask for confirmation
	cmd( "set answer [ tk_messageBox -parent . -type yesnocancel -default yes -icon question -title Confirmation -message \"Save current file?\" -detail \"Recent changes to file '$filename' have not been saved.\\n\\nDo you want to save before continuing?\nNot doing so will not include recent changes to subsequent actions.\n\n - Yes: save the file and continue.\n - No: do not save and continue.\n - Cancel: do not save and return to editing.\" ]" );
	cmd( "if [ string equal $answer yes ] { \
			set curfile [ file join \"$dirname\" \"$filename\" ]; \
			set file [ open \"$curfile\" w ]; \
			puts -nonewline $file [ .f.t.t get 0.0 end ]; \
			close $file; \
			set before [ .f.t.t get 0.0 end ]; \
			update_title_bar; \
			set ans 1; \
		} elseif [ string equal $answer cancel ] { \
			set ans 0; \
			set choice 0 \
		} { \
			set ans 1 \
		}" );  

	const char *ans = Tcl_GetVar( inter, "ans", 0 );
	if ( atoi( ans ) == 0 )
		return false;
	
	return true;
}


/****************************************************
TCL_DISCARD_CHANGE
Entry point function for access from the Tcl interpreter
****************************************************/
int Tcl_discard_change( ClientData cdata, Tcl_Interp *inter, int argc, const char *argv[ ] )
{
	if ( discard_change( ) == 1 )
		Tcl_SetResult( inter, ( char * ) "ok", TCL_VOLATILE );
	else
		Tcl_SetResult( inter, ( char * ) "cancel", TCL_VOLATILE );
	return TCL_OK;
}


/*********************************
 HANDLE_SIGNALS
 *********************************/
// provide support to the old 32-bit gcc compiler
#ifdef SIGSYS
#define NUM_SIG 11
int signals[ NUM_SIG ] = { SIGINT, SIGQUIT, SIGTERM, SIGWINCH, SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGBUS, SIGSYS, SIGXFSZ };
#else
#define NUM_SIG 6
int signals[ NUM_SIG ] = { SIGINT, SIGTERM, SIGABRT, SIGFPE, SIGILL, SIGSEGV };
const char *signal_names[ NUM_SIG ] = { "", "", "", "Floating-point exception", "Illegal instruction", "Segmentation violation", };
const char *strsignal( int signum ) 
{ 
	int i;
	for ( i = 0; i < NUM_SIG && signals[ i ] != signum; ++i );
	if ( i == NUM_SIG )
		return "Unknow exception";
	return signal_names[ i ];
}
#endif

void handle_signals( void )
{
	for ( int i = 0; i < NUM_SIG; ++i )
		signal( signals[ i ], signal_handler );  
}


/*********************************
 SIGNAL_HANDLER
 *********************************/
// handle critical system signals
void signal_handler( int signum )
{
	char msg2[ MAX_LINE_SIZE ];

	switch ( signum )
	{
#ifdef SIGQUIT
		case SIGQUIT:
#endif
		case SIGINT:
		case SIGTERM:
			choice = 1;				// regular quit (checking for save)
			return;
#ifdef SIGWINCH
		case SIGWINCH:
			cmd( "sizetop all" );	// readjust windows size/positions
			cmd( "update idletasks" );
			return;
#endif
		case SIGMEM:
			sprintf( msg, "Out of memory" );
			break;
			
		case SIGSTL:
			break;
			
		case SIGABRT:
		case SIGFPE:
		case SIGILL:
		case SIGSEGV:
#ifdef SIGBUS
		case SIGBUS:
		case SIGSYS:
		case SIGXFSZ:
#endif
		default:
			strncpy( msg, strsignal( signum ), 999 );
			break;			
	}
	
	sprintf( msg2, "System Signal received: %s", msg );
	log_tcl_error( "FATAL ERROR", msg2 );
	if ( tk_ok )
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"FATAL ERROR\" -detail \"System Signal received:\n\n %s\n\nLMM will close now.\"", msg );
	Tcl_Finalize( );
	exit( -signum );				// abort program
}
