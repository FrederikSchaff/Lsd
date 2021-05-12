/*************************************************************

	LSD 8.0 - March 2021
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente and Marcelo Pereira
	LSD is distributed under the GNU General Public License

	See Readme.txt for copyright information of
	third parties' code used in LSD
	
 *************************************************************/

/*************************************************************
GETSAVED.CPP
Execute the lsd_getsaved command line utility.

Lists all variables being saved in a configuration.
*************************************************************/

#include "decl.h"

#define SEP	",;\t"			// column separators to use


bool ignore_eq_file = true;	// flag to ignore equation file in configuration file
bool message_logged = false;// new message posted in log window
bool no_more_memory = false;// memory overflow when setting data save structure
bool no_ptr_chk = false;	// disable user pointer checking
bool no_saved = true;		// disable the usage of saved values as lagged ones
bool no_search;				// disable the standard variable search mechanism
bool no_zero_instance = true;// flag to allow deleting last object instance
bool parallel_mode;			// parallel mode (multithreading) status
bool running = false;		// simulation is running
bool struct_loaded = false;	// a valid configuration file is loaded
bool unsavedData = false;	// flag unsaved simulation results
bool unsavedSense = false;	// control for unsaved changes in sensitivity data
bool user_exception = false;// flag indicating exception was generated by user code
bool use_nan;				// flag to allow using Not a Number value
char *config_file = NULL;	// name of text configurations file
char *eq_file = NULL;		// equation file content
char *exec_path = NULL;		// path of executable file
char *path = NULL;			// path of current configuration
char *sens_file = NULL;		// current sensitivity analysis file
char *simul_name = NULL;	// name of current simulation configuration
char *struct_file = NULL;	// name of current configuration file
char equation_name[ MAX_PATH_LENGTH ] = "";	// equation file name
char lsd_eq_file[ MAX_FILE_SIZE + 1 ] = "";	// equations saved in configuration file
char msg[ TCL_BUFF_STR ] = "";				// auxiliary Tcl buffer
char name_rep[ MAX_PATH_LENGTH + 1 ] = "";	// documentation report file name
char path_rep[ MAX_PATH_LENGTH + 1 ] = "";	// documentation report file path
char nonavail[ ] = "NA";	// string for unavailable values (use R default)
int actual_steps = 0;		// number of executed time steps
int debug_flag = false;		// debug enable control (bool)
int fast_mode = 1;			// flag to hide LOG messages & runtime plot
int findex = 1;				// current multi configuration job
int findexSens = 0;			// index to sequential sensitivity configuration filenames
int max_step = 100;			// default number of simulation runs
int prof_aggr_time = false;	// show aggregate profiling times
int prof_min_msecs = 0;		// profile only variables taking more than X msecs.
int prof_obs_only = false;	// profile only observed variables
int quit = 0;				// simulation interruption mode (0=none)
int t;						// current time step
int series_saved = 0;		// number of series saved
int sim_num = 1;			// simulation number running
int stack;					// LSD stack call level
int when_debug;				// next debug stop time step (0 for none)
int wr_warn_cnt;			// invalid write operations warning counter
long nodesSerial = 1;		// network node's serial number global counter
unsigned seed = 1;			// random number generator initial seed
description *descr = NULL;	// model description structure
lsdstack *stacklog = NULL;	// LSD stack
object *blueprint = NULL;	// LSD blueprint (effective model in use)
object *root = NULL;		// LSD root object
object *wait_delete = NULL;	// LSD object waiting for deletion
o_setT obj_list;			// set with all existing LSD objects
sense *rsense = NULL;		// LSD sensitivity analysis structure
variable *cemetery = NULL;	// LSD saved data series (from last simulation run)
variable *last_cemetery = NULL;	// LSD last saved data from deleted objects

// constant string arrays
const char *signal_names[ REG_SIG_NUM ] = REG_SIG_NAME;
const int signals[ REG_SIG_NUM ] = REG_SIG_CODE;


char *out_file = NULL;		// output .csv file, if any


/*********************************
 LSDMAIN
 *********************************/
int lsdmain( int argn, char **argv )
{
	int i, confs;
	char *sep;
	FILE *f;
	bool all_var = false;

	path = new char[ strlen( "" ) + 1 ];
	strcpy( path, "" );

	findex = 1;

	if ( argn < 3 )
	{
		fprintf( stderr, "\nThis is LSD Saved Variable Reader.\nIt reads a LSD configuration file (.lsd) and shows the variables/parameters\nbeing saved, optionally saving them in a comma separated text file (.csv).\n\nCommand line options:\n'-a' show all variables/parameters\n'-f FILENAME.lsd' the configuration file to use\n'-o OUTPUT.csv' name for the comma separated output text file\n\n" );
		myexit( 1 );
	}
	else
	{
		for ( i = 1; i < argn; i += 2 )
		{
			// read -f parameter : original configuration file
			if ( argv[ i ][ 0 ] == '-' && argv[ i ][ 1 ] == 'f' && 1 + i < argn && strlen( argv[ 1 + i ] ) > 0 )
			{
				struct_file = new char[ strlen( argv[ 1 + i ] ) + 1 ];
				strcpy( struct_file, argv[ 1 + i ] );
				continue;
			}
			// read -o parameter : output file name
			if ( argv[ i ][ 0 ] == '-' && argv[ i ][ 1 ] == 'o' && 1 + i < argn && strlen( argv[ 1 + i ] ) > 0 )
			{
				out_file = new char[ strlen( argv[ 1 + i ] ) + 1 ];
				strcpy( out_file, argv[ 1 + i ] );
				continue;
			}
			// read -a parameter : show all variables/parameters
			if ( argv[ i ][ 0 ] == '-' && argv[ i ][ 1 ] == 'a' )
			{
				i--; 					// no parameter for this option
				all_var = true;
				continue;
			}

			fprintf( stderr, "\nOption '%c%c' not recognized.\nThis is LSD Saved Variable Reader.\n\nCommand line options:\n'-a' show all variables/parameters\n'-f FILENAME.lsd' the configuration file to use\n'-o OUTPUT.csv' name for the comma separated output text file\n\n", argv[ i ][ 0 ], argv[ i ][ 1 ] );
			myexit( 2 );
		}
	}

	if ( struct_file == NULL )
	{
		fprintf( stderr, "\nNo configuration file provided.\nThis is LSD Saved Variable Reader.\nSpecify a -f FILENAME.lsd to use for reading the saved variables (if any).\n\n" );
		myexit( 3 );
	}

	f = fopen( struct_file, "r" );
	if ( f == NULL )
	{
		fprintf( stderr, "\nFile '%s' not found.\nThis is LSD Saved Variable Reader.\nSpecify an existing -f FILENAME.lsd configuration file.\n\n", struct_file );
		myexit( 4 );
	}
	fclose( f );

	root = new object;
	root->init( NULL, "Root" );
	add_description( "Root" );
	reset_blueprint( NULL );

	if ( load_configuration( true ) != 0 )
	{
		fprintf( stderr, "\nFile '%s' is invalid.\nThis is LSD Saved Variable Reader.\nCheck if the file is a valid LSD configuration or regenerate it using the LSD Browser.\n\n", struct_file );
		myexit( 5 );
	}

	count_save( root, & i );
	if ( ! all_var && i == 0 )
	{
		printf( "\n(no variable being saved)\n" );
		return 0;
	}

	if ( out_file != NULL && strlen( out_file ) != 0 )
	{
		f = fopen( out_file, "wt" );
		if ( f == NULL )
		{
			fprintf( stderr, "\nFile '%s' cannot be saved.\nThis is LSD Saved Variable Reader.\nCheck if the drive or the file is set READ-ONLY, change file name or\nselect a drive with write permission and try again.\n\n", out_file  );
			myexit( 6 );
		}

		sep = new char [ strlen( CSV_SEP ) + 1 ];
		strcpy( sep, CSV_SEP );

		// write .csv header
		fprintf( f, "Name%sType%sObject%sDescription\n", sep, sep, sep );
		get_saved( root, f, sep, all_var );
		fclose( f );
	}
	else	// send to stdout
		get_saved( root, stdout, "\t", all_var );

	empty_blueprint( );
	empty_description( );
	root->delete_obj( );
	delete [ ] out_file;
	delete [ ] simul_name;

	return 0;
}


/*********************************
FUN
Dummy for linking
*********************************/
double variable::fun( object* r ) { return NAN; }


/*********************************
ALLOC_SAVE_VAR
*********************************/
bool alloc_save_var( variable *v ) { return true; }
