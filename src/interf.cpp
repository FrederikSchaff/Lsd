/*************************************************************

	LSD 7.1 - May 2018
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente
	LSD is distributed under the GNU General Public License
	
	Silk icon set 1.3 by Mark James
	http://www.famfamfam.com/lab/icons/silk 
	
 *************************************************************/

/*
USED CASE 93
*/

/****************************************************
INTERF.CPP Manage the main interfaces, that the browser and all the menus.
It is re-build any time the window changes. There are some 20 actions that are
commanded from the browser window, implemented as a switch in operate.

The functions contained in this file are:

-object *create( object *root)
The main cycle for the Browser, from which it exits only to run a simulation
or to quit the program. The cycle is just once call to browsw followed by
a call to operate.

- int browse( object *r, int *choice);
build the browser window and waits for an action (on the form of
values for choice or choice_g different from 0)

- object *operate( int *choice, object *r);
takes the value of choice and operate the relative command on the
object r. See the switch for the complete list of the available commands

- void clean_debug(object *n);
remove all the flags to debug from any variable in the model

- void clean_save(object *n);
remove all the flags to save from any variable in the model

- void show_save(object *n)
shows all variables to be saved in the result files

- void clean_plot(object *n);
remove all the flags to plot from any variable in the model

- void wipe_out(object *d);
Eliminate all the Object like d from the model. Cancel also the their descendants


Functions used here from other files are:

- void plog(char *m);
LSDMAIN.CPP print  message string m in the Log screen.

- void analysis(int *choice);
ANALYSIS.CPP analysis of result files

- void show_eq(char *lab, int *choice);
SHOW_EQ.CPP shows one equation for variable lab

- object *skip_next_obj(object *t, int *i);
UTIL.CPP. Counts how many types of objects equal to t are in this
group. count returns such value, and the whole function returns the next object
after the last of the series.

- int my_strcmp(char *a, char *b);
UTIL.CPP It is a normal strcmp, but it catches the possibility of both strings being
NULL

- void cmd(char *cc);
UTIL.CPP Standard routine to send the message string cc to the interp
Basically it makes a simple Tcl_Eval, but controls also that the interpreter
did not issue an error message.

- object *go_brother(object *cur);
UTIL.CPP returns: c->next, if it is of the same type of c (brother).
Returns NULL otherwise. It is safe to use even when c or c->next are NULL.

- void show_graph( object *t);
DRAW.CPP shows the grsphical representation of the model

- void set_obj_number(object *r, int *choice);
EDIT.CPP allows to edit the number of instances in the model

- void edit_data(object *root, int *choice, char *obj_name);
EDIT_DAT.CPP allows to edit the initial values

- FILE *search_str(char *name, char *str);
UTIL.CPP given a string name, returns the file corresponding to name, and the current
position of the file is just after str.

- int deb(object *r, object *c, char *lab, double *res);
Use the debugger interface to browse through the model

- void myexit(int v);
Exit function, which is customized on the operative system.

****************************************************/

#include "decl.h"

bool justAddedVar = false;			// control the selection of last added variable
bool redrawReq = false;				// flag for asynchronous window redraw request
char lastObj[MAX_ELEM_LENGTH] = "";	// to save last shown object for quick reload (choice=38)
char *res_g;
int natBat = true;					// native (Windows/Linux) batch format flag (bool)
int result_loaded;
int lcount;
object *currObj;

// list of choices that are bad with existing run data
int badChoices[ ] = { 1, 2, 3, 6, 7, 19, 21, 22, 25, 27, 28, 30, 31, 32, 33, 36, 43, 57, 58, 59, 62, 63, 64, 65, 68, 69, 71, 72, 74, 75, 76, 77, 78, 79, 80, 81, 83, 88, 90, 91, 92, 93 };
#define NUM_BAD_CHOICES ( sizeof( badChoices ) / sizeof( badChoices[ 0 ] ) )

// list of choices that are run twice (called from another choice)
int redoChoices[ ] = { 20, 33, 55, 74, 75, 76, 77, 78, 79, 83 };
#define NUM_REDO_CHOICES ( sizeof( redoChoices ) / sizeof( redoChoices[ 0 ] ) )

// comparison function for bsearch and qsort
int comp_ints ( const void *a, const void *b ) { return ( *( int * ) a - *( int * ) b ); }

 
/****************************************************
CREATE
****************************************************/
object *create( object *cr )
{
	object *cur;
	char *s;

	Tcl_LinkVar( inter, "strWindowOn", ( char * ) &strWindowOn, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "choice_g", ( char * ) &choice_g, TCL_LINK_INT );
	Tcl_LinkVar( inter, "actual_steps", ( char * ) &actual_steps, TCL_LINK_INT );

	// sort the list of choices with existing run data to use later
	qsort( badChoices, NUM_BAD_CHOICES, sizeof ( int ), comp_ints );
	qsort( redoChoices, NUM_REDO_CHOICES, sizeof ( int ), comp_ints );

	cmd( "set ignore_eq_file %d", ignore_eq_file ? 1 : 0  );
	cmd( "set listfocus 1" );
	cmd( "set itemfocus 0" );
	cmd( "set cur 0" ); 	//Set yview for vars listbox
	cmd( "set c \"\"" );
	cmd( "if $strWindowOn { set strWindowB active } { set strWindowB normal }" );


	// restore previous object and cursor position in browser, if any
	if ( strlen( lastObj ) > 0 )
	{
		for ( cur = cr; cur->up != NULL; cur = cur->up );
		cur = cur->search( lastObj );
		if ( cur != NULL )
		{
			cr = cur;
			cmd( "if [ info exists lastList ] { set listfocus $lastList }" );
			cmd( "if [ info exists lastItem ] { set itemfocus $lastItem }" );
		}
	}

	redrawRoot = true;			// browser redraw when drawing the first time

	choice_g = choice = 0;

	// Main Cycle ********************************
	while ( choice != 1 )
	{
		cmd( "wm title . \"%s%s - LSD Browser\"", unsaved_change() ? "*" : " ", simul_name  );
		cmd( "wm title .log \"%s%s - LSD Log\"", unsaved_change() ? "*" : " ", simul_name  );

		for ( cur = cr; cur->up != NULL; cur = cur->up );

		if ( cur->v == NULL && cur->b == NULL )
			struct_loaded = false;
		else
		{ 
			struct_loaded = true;
			show_graph( cr );
			if ( message_logged )
			{
				cmd( "wm deiconify .log; raise .log; focus .log; update idletasks" );
				message_logged = false;
			}    
		}    

		cmd( "bind . <KeyPress-Escape> {}" );
		cmd( "bind . <KeyPress-Return> {}" );
		cmd( "bind . <Destroy> {set choice 35}" );
		cmd( "bind .log <Destroy> {set choice 35}" );

		// browse only if not running two-cycle operations
		if ( bsearch( & choice, redoChoices, NUM_REDO_CHOICES, sizeof ( int ), comp_ints ) == NULL )
			choice = browse( cr, &choice );

		cr = operate( &choice, cr );
	}

	Tcl_UnlinkVar( inter, "strWindowOn" );
	Tcl_UnlinkVar( inter, "choice_g" );
	Tcl_UnlinkVar( inter, "actual_steps" );

	return cr;
}


/****************************************************
BROWSE
****************************************************/
int browse( object *r, int *choice )
{
	char ch[ TCL_BUFF_STR ];
	int num;
	variable *ap_v;
	object *ap_o;
	bridge *cb;

	currObj = r;			// global pointer to C Tcl routines

	// main LSD window - avoids redrawing if not required
	if ( redrawRoot ) 
	{
		cmd( "destroy .t .l" );
		cmd( "frame .l" );

		cmd( "frame .l.v" );

		cmd( "frame .l.v.c" );
		cmd( "scrollbar .l.v.c.v_scroll -command \".l.v.c.var_name yview\"" );
		cmd( "listbox .l.v.c.var_name -selectmode browse -yscroll \".l.v.c.v_scroll set\"" );

		cmd( "mouse_wheel .l.v.c.var_name" );
		cmd( "bind .l.v.c.var_name <Left> { focus .l.s.c.son_name; set listfocus 2; set itemfocus 0; ; .l.s.c.son_name selection set 0; .l.s.c.son_name activate 0; .l.s.c.son_name see 0 }" );

		// populate the variables panel
		if ( r->v == NULL )
			cmd( ".l.v.c.var_name insert end \"(none)\"; set nVar 0" );
		else
		{
			cmd( "set app 0" );
			for ( ap_v = r->v; ap_v != NULL; ap_v = ap_v->next )
			{
				// add elements to the listbox 
				if ( ap_v->param == 0 )
				{
					if ( ap_v->num_lag == 0 )
					{
						cmd( ".l.v.c.var_name insert end \"%s (V%s)\"", ap_v->label, ( ap_v->save || ap_v->savei ) ? "+" : "" );
						cmd( ".l.v.c.var_name itemconf $app -fg blue" );
					}
					else
					{
						cmd( ".l.v.c.var_name insert end \"%s (V_%d%s)\"", ap_v->label, ap_v->num_lag, ( ap_v->save || ap_v->savei ) ? "+" : "" );
						cmd( ".l.v.c.var_name itemconf $app -fg purple" );
					}
				}
				
				if ( ap_v->param == 1 )
					cmd( ".l.v.c.var_name insert end \"%s (P%s)\"", ap_v->label, ( ap_v->save || ap_v->savei ) ? "+" : "" );
				
				if ( ap_v->param == 2 )
				{
					if ( ap_v->num_lag == 0 )
					{
						cmd( " .l.v.c.var_name insert end \"%s (F%s)\"", ap_v->label, ( ap_v->save || ap_v->savei ) ? "+" : "" );
						cmd( ".l.v.c.var_name itemconf $app -fg firebrick" );
					}
					else
					{
						cmd( ".l.v.c.var_name insert end \"%s (F_%d%s)\"", ap_v->label, ap_v->num_lag, ( ap_v->save || ap_v->savei ) ? "+" : "" );
						cmd( ".l.v.c.var_name itemconf $app -fg tomato" );
					}
				}

				cmd( "incr app" );

				if ( ap_v->next == NULL && justAddedVar )	// last variable & just added a new variable?
				{
					justAddedVar = false;
					cmd( ".l.v.c.var_name selection clear 0 end; .l.v.c.var_name selection set end; set lst [ .l.v.c.var_name curselection ]; if { ! [ string equal $lst \"\" ] } { set res [ .l.v.c.var_name get $lst ]; set listfocus 1; set itemfocus $lst}" );
				}
			}
			cmd( "set nVar [ .l.v.c.var_name size ]" );
		}

		cmd( "label .l.v.lab -text \"Variables & Parameters ($nVar)\"" );

		// variables panel context menu (right mouse button)
		cmd( "menu .l.v.c.var_name.v -tearoff 0" );
		cmd( ".l.v.c.var_name.v add command -label Change -command { set choice 7 }" );	// entryconfig 0
		cmd( ".l.v.c.var_name.v add command -label Properties -command { set choice 75 }" );	// entryconfig 1
		cmd( ".l.v.c.var_name.v add separator" );	// entryconfig 2
		cmd( ".l.v.c.var_name.v add checkbutton -label Save -variable save -command { if { $actual_steps == 0 } { set_var_conf $vname save $save; set choice 70 } { set choice 7 } }" );	// entryconfig 3
		cmd( ".l.v.c.var_name.v add checkbutton -label \"Run Plot\" -variable plot -command { if { $actual_steps == 0 } { set_var_conf $vname plot $plot; set choice 70 } { set choice 7 } }" );	// entryconfig 4
		cmd( ".l.v.c.var_name.v add checkbutton -label Debug -state disabled -variable num -command { if { $actual_steps == 0 } { set_var_conf $vname debug $num; set choice 70 } { set choice 7 } }" );	// entryconfig 5
		cmd( ".l.v.c.var_name.v add checkbutton -label Parallel -state disabled -variable parallel -command { if { $actual_steps == 0 } { set_var_conf $vname parallel $parallel; set choice 70 } { set choice 7 } }" );	// entryconfig 6
		cmd( ".l.v.c.var_name.v add separator" );	// entryconfig 7
		cmd( ".l.v.c.var_name.v add command -label \"Move Up\" -state disabled -command { set listfocus 1; set itemfocus [ .l.v.c.var_name curselection ]; if { $itemfocus > 0 } { incr itemfocus -1 }; set choice 58 }" );	// entryconfig 8
		cmd( ".l.v.c.var_name.v add command -label \"Move Down\" -state disabled -command { set listfocus 1; set itemfocus [ .l.v.c.var_name curselection ]; if { $itemfocus < [ expr [ .l.v.c.var_name size ] - 1 ] } { incr itemfocus }; set choice 59 }" );	// entryconfig 9
		cmd( ".l.v.c.var_name.v add separator" );	// entryconfig 10
		cmd( ".l.v.c.var_name.v add command -label Move -command { set choice 79 }" );	// entryconfig 11
		cmd( ".l.v.c.var_name.v add command -label Delete -command { set choice 76 }" );	// entryconfig 12
		cmd( ".l.v.c.var_name.v add separator" );	// entryconfig 13
		cmd( ".l.v.c.var_name.v add command -label Equation -state disabled -command { set choice 29 }" );	// entryconfig 14
		cmd( ".l.v.c.var_name.v add command -label Using -state disabled -command { set choice 46 }" );	// entryconfig 15
		cmd( ".l.v.c.var_name.v add command -label \"Used In\" -state disabled -command { set choice 47 }" );	// entryconfig 16
		cmd( ".l.v.c.var_name.v add separator" );	// entryconfig 17
		cmd( ".l.v.c.var_name.v add command -label \"Initial Values\" -state disabled -command { set choice 77 }" );	// entryconfig 18
		cmd( ".l.v.c.var_name.v add command -label Sensitivity -state disabled -command { set choice 78 }" );	// entryconfig 19

		// variables panel bindings
		if ( r->v != NULL )
		{
			cmd( "bind .l.v.c.var_name <Return> { \
				set listfocus 1; \
				set itemfocus [ .l.v.c.var_name curselection ]; \
				if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
					set choice 7 \
				} \
			}" );
			cmd( "bind .l.v.c.var_name <Double-Button-1> { \
				after 50; \
				event generate .l.v.c.var_name <Return> \
			}" );
			cmd( "bind .l.v.c.var_name <Button-2> { \
				.l.v.c.var_name selection clear 0 end; \
				.l.v.c.var_name selection set @%%x,%%y; \
				set listfocus 1; \
				set itemfocus [ .l.v.c.var_name curselection ]; \
				set color [ lindex [ .l.v.c.var_name itemconf $itemfocus -fg ] end ]; \
				if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
					.l.v.c.var_name.v entryconfig 5 -state normal; \
					.l.v.c.var_name.v entryconfig 6 -state normal; \
					.l.v.c.var_name.v entryconfig 8 -state normal; \
					.l.v.c.var_name.v entryconfig 9 -state normal; \
					.l.v.c.var_name.v entryconfig 14 -state normal; \
					.l.v.c.var_name.v entryconfig 15 -state normal; \
					.l.v.c.var_name.v entryconfig 16 -state normal; \
					.l.v.c.var_name.v entryconfig 18 -state normal; \
					.l.v.c.var_name.v entryconfig 19 -state normal; \
					set save [ get_var_conf $vname save ]; \
					set plot [ get_var_conf $vname plot ]; \
					set num [ get_var_conf $vname debug ]; \
					set parallel [ get_var_conf $vname parallel ]; \
					switch $color { \
						purple { } \
						blue { \
							.l.v.c.var_name.v entryconfig 18 -state disabled; \
							.l.v.c.var_name.v entryconfig 19 -state disabled; \
						} \
						black { \
							.l.v.c.var_name.v entryconfig 5 -state disabled; \
							.l.v.c.var_name.v entryconfig 6 -state disabled; \
							.l.v.c.var_name.v entryconfig 14 -state disabled; \
							.l.v.c.var_name.v entryconfig 15 -state disabled \
						} \
						tomato { \
							.l.v.c.var_name.v entryconfig 6 -state disabled; \
						} \
						firebrick { \
							.l.v.c.var_name.v entryconfig 6 -state disabled; \
							.l.v.c.var_name.v entryconfig 18 -state disabled; \
							.l.v.c.var_name.v entryconfig 19 -state disabled; \
						} \
					}; \
					if { $itemfocus == 0 } { \
						.l.v.c.var_name.v entryconfig 8 -state disabled \
					}; \
					if { $itemfocus == [ expr [ .l.v.c.var_name size ] - 1 ] } { \
						.l.v.c.var_name.v entryconfig 9 -state disabled \
					}; \
					tk_popup .l.v.c.var_name.v %%X %%Y \
				} \
			}" );
			cmd( "bind .l.v.c.var_name <Button-3> { \
				event generate .l.v.c.var_name <Button-2> -x %%x -y %%y \
			}" );
			cmd( "bind .l.v.c.var_name <Control-Up> { \
				set listfocus 1; \
				set itemfocus [ .l.v.c.var_name curselection ]; \
				if { $itemfocus > 0 } { \
					incr itemfocus -1 \
				}; \
				if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
					set choice 58 \
				} \
			}" );
			cmd( "bind .l.v.c.var_name <Control-Down> { \
				set listfocus 1; \
				set itemfocus [ .l.v.c.var_name curselection ]; \
				if { $itemfocus < [ expr [ .l.v.c.var_name size ] - 1 ] } { \
					incr itemfocus \
				}; \
				if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
					set choice 59 \
				} \
			}" );
		}
		cmd( ".l.v.c.var_name yview $cur" );

		cmd( "frame .l.s" );

		cmd( "frame .l.s.c" );
		cmd( "scrollbar .l.s.c.v_scroll -command \".l.s.c.son_name yview\"" );
		cmd( "listbox .l.s.c.son_name -selectmode browse -yscroll \".l.s.c.v_scroll set\"" );

		cmd( "mouse_wheel .l.s.c.son_name" );
		cmd( "bind .l.s.c.son_name <Right> { focus .l.v.c.var_name; set listfocus 1; set itemfocus 0; .l.v.c.var_name selection set 0; .l.v.c.var_name activate 0; .l.v.c.var_name see 0 }" );
		cmd( "bind .l.s.c.son_name <BackSpace> { set choice 5 }" );

		// populate the objects panel
		if ( r->b == NULL )
			cmd( ".l.s.c.son_name insert end \"(none)\"; set nDesc 0" );
		else
		{
			cmd( "set app 0" );
			for ( cb = r->b; cb != NULL; cb = cb->next )
			{
				skip_next_obj( cb->head, &num );
				cmd( ".l.s.c.son_name insert end \"%s (#%d)\"", cb->blabel, num );
				cmd( ".l.s.c.son_name itemconf $app -fg red" );
				cmd( "incr app" );
			}
			cmd( "set nDesc [ .l.s.c.son_name size ]" );
		}	

		cmd( "label .l.s.lab -text \"Descending Objects ($nDesc)\"" );

		// objects panel context menu (right mouse button)
		cmd( "menu .l.s.c.son_name.v -tearoff 0" );
		cmd( ".l.s.c.son_name.v add command -label \"Select\" -command { set choice 4 }" );	// entryconfig 0
		cmd( ".l.s.c.son_name.v add command -label \"Parent\" -command { set choice 5 }" );	// entryconfig 1
		cmd( ".l.s.c.son_name.v add command -label \"Insert Parent\" -command { set choice 32 }" );	// entryconfig 2
		cmd( ".l.s.c.son_name.v add separator" );	// entryconfig 3
		cmd( ".l.s.c.son_name.v add command -label \"Move Up\" -state disabled -command { set listfocus 2; set itemfocus [ .l.s.c.son_name curselection ]; if { $itemfocus > 0 } { incr itemfocus -1 }; if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { set choice 60 } }" );	// entryconfig 4
		cmd( ".l.s.c.son_name.v add command -label \"Move Down\" -state disabled -command { set listfocus 2; set itemfocus [ .l.s.c.son_name curselection ]; if { $itemfocus < [ expr [ .l.s.c.son_name size ] - 1 ] } { incr itemfocus }; if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { set choice 61 } }" );	// entryconfig 5
		cmd( ".l.s.c.son_name.v add separator" );	// entryconfig 6
		cmd( ".l.s.c.son_name.v add command -label Change -command { set choice 6 }" );	// entryconfig 7
		cmd( ".l.s.c.son_name.v add command -label Rename -command { set choice 83 }" );	// entryconfig 8
		cmd( ".l.s.c.son_name.v add command -label Number -command { set choice 33 }" );	// entryconfig 9
		cmd( ".l.s.c.son_name.v add command -label Delete -command { set choice 74 }" );	// entryconfig 10
		cmd( ".l.s.c.son_name.v add separator" );	// entryconfig 11
		cmd( ".l.s.c.son_name.v add cascade -label Add -menu .l.s.c.son_name.v.a" );	// entryconfig 12
		cmd( ".l.s.c.son_name.v add separator" );	// entryconfig 13
		cmd( ".l.s.c.son_name.v add command -label \"Initial Values\" -command { set choice 21 }" );	// entryconfig 14
		cmd( ".l.s.c.son_name.v add command -label \"Browse Data\" -command { set choice 34 }" );	// entryconfig 15
		cmd( "menu .l.s.c.son_name.v.a -tearoff 0" );
		cmd( ".l.s.c.son_name.v.a add command -label Variable -command { set choice 2; set param 0 }" );
		cmd( ".l.s.c.son_name.v.a add command -label Parameter -command { set choice 2; set param 1 }" );
		cmd( ".l.s.c.son_name.v.a add command -label Function -command { set choice 2; set param 2 }" );
		cmd( ".l.s.c.son_name.v.a add command -label Object -command { set choice 3 }" );

		// objects panel bindings
		if ( r->b != NULL )
		{
			cmd( "bind .l.s.c.son_name <Return> { \
					set listfocus 2; \
					set itemfocus [ .l.s.c.son_name curselection ]; \
					if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
						set choice 4 \
					} \
				}" );
			cmd( "bind .l.s.c.son_name <Double-Button-1> { \
					after 50; \
					event generate .l.s.c.son_name <Return> \
				}" );
			cmd( "bind .l.s.c.son_name <Button-2> { \
					.l.s.c.son_name selection clear 0 end; \
					.l.s.c.son_name selection set @%%x,%%y; \
					set listfocus 2; \
					set itemfocus [ .l.s.c.son_name curselection ]; \
					if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
						set useCurrObj no; \
						if { $itemfocus == 0 } { \
							.l.s.c.son_name.v entryconfig 4 -state disabled \
						} { \
							.l.s.c.son_name.v entryconfig 4 -state normal \
						}; \
						if { $itemfocus == [ expr [ .l.s.c.son_name size ] - 1 ] } { \
							.l.s.c.son_name.v entryconfig 5 -state disabled \
						} { \
							.l.s.c.son_name.v entryconfig 5 -state normal \
						}; \
						tk_popup .l.s.c.son_name.v %%X %%Y \
					} \
				}" );
			cmd( "bind .l.s.c.son_name <Button-3> { \
					event generate .l.s.c.son_name <Button-2> -x %%x -y %%y \
				}" );
			cmd( "bind .l.s.c.son_name <Control-Up> { \
					set listfocus 2; \
					set itemfocus [ .l.s.c.son_name curselection ]; \
					if { $itemfocus > 0 } { \
						incr itemfocus -1 \
					}; \
					if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
						set choice 60 \
					} \
				}" );
			cmd( "bind .l.s.c.son_name <Control-Down> { \
					set listfocus 2; \
					set itemfocus [ .l.s.c.son_name curselection ]; \
					if { $itemfocus < [ expr [ .l.s.c.son_name size ] - 1 ] } { \
						incr itemfocus \
					}; \
					if { ! [ catch { set vname [ lindex [ split [ selection get ] ] 0 ] } ] } { \
						set choice 61 \
					} \
				}" );
		}

		// navigation (top) panel
		cmd( "frame .l.p -relief groove -bd 2" );

		cmd( "frame .l.p.up_name" );
		cmd( "label .l.p.up_name.d -text \"Parent Object:\" -width 12 -anchor w" );
		strcpy( ch, "button .l.p.up_name.n -relief $bRlf -overrelief $ovBrlf -anchor e -text \" " );
		if ( r->up == NULL )
			strcat( ch, "(none) \" -command { }" );
		else
		{
			strcat( ch, ( r->up )->label );
			strcat( ch, " \" -command { set itemfocus 0; set choice 5 } -foreground red" );
		}
		cmd( ch );

		cmd( "bind .l.p.up_name.n <Enter> {set ttip \"Select parent object\"}" );
		cmd( "bind .l.p.up_name.n <Leave> {set ttip \"\"}" );
		cmd( "bind . <KeyPress-u> {catch {.l.p.up_name.n invoke}}; bind . <KeyPress-U> {catch {.l.p.up_name.n invoke}}" );

		cmd( "pack .l.p.up_name.d .l.p.up_name.n -side left" );
		cmd( "pack .l.p.up_name -padx 9 -anchor w" );

		cmd( "frame .l.p.tit" );
		cmd( "label .l.p.tit.lab -text \"Current Object:\" -width 12 -anchor w" );
		strcpy( ch, "button .l.p.tit.but -foreground red -relief $bRlf -overrelief $ovBrlf -anchor e -text \" " );
		strcat( ch, r->label);

		if ( r->up != NULL ) 
			strcat( ch, " \" -command { set choice 6 }" );
		else
			strcat( ch, " \" -command { }" );
		cmd( ch );

		cmd( "bind .l.p.tit.but <Enter> {set ttip \"Change...\"}" );
		cmd( "bind .l.p.tit.but <Leave> {set ttip \"\"}" );

		cmd( "pack .l.p.tit.lab .l.p.tit.but -side left" );
		cmd( "pack .l.p.tit -padx 8 -anchor w" );

		// main menu - avoid redrawing the menu if it already exists and is configured
		cmd( "set existMenu [ winfo exists .m ]" );
		cmd( "set confMenu [ . cget -menu ]" );
		if ( ! strcmp( Tcl_GetVar( inter, "existMenu", 0 ), "0" ) ||
			 strcmp( Tcl_GetVar( inter, "confMenu", 0 ), ".m" ) )
		{
			cmd( "destroy .m" );
			cmd( "menu .m -tearoff 0" );

			cmd( "set w .m.file" );
			cmd( "menu $w -tearoff 0" );
			cmd( ".m add cascade -label File -menu $w -underline 0" );
			cmd( "$w add command -label \"Load...\" -underline 0 -accelerator Ctrl+L -command { set choice 17 }" );
			cmd( "$w add command -label Reload -underline 0 -accelerator Ctrl+W -command { set choice 38 }" );
			cmd( "$w add command -label Save -underline 0 -accelerator Ctrl+S -command { set choice 18 }" );
			cmd( "$w add command -label \"Save As...\" -underline 5 -command { set choice 73 }" );
			cmd( "$w add command -label Unload -underline 0 -accelerator Ctrl+E -command { set choice 20 }" );
			cmd( "$w add command -label \"Compare...\" -underline 0 -command { set choice 82 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Save Results...\" -underline 2 -accelerator Ctrl+Z -command { set choice 37 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Load Network...\" -underline 5 -command { set choice 88 }" );
			cmd( "$w add command -label \"Save Network...\" -underline 8 -command { set choice 89 }" );
			cmd( "$w add command -label \"Unload Network\" -underline 3 -command { set choice 93 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Load Sensitivity...\" -underline 3 -command { set choice 64 }" );
			cmd( "$w add command -label \"Save Sensitivity...\" -underline 6 -command { set choice 65 }" );
			cmd( "$w add command -label \"Unload Sensitivity\" -underline 11 -command { set choice 67 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Export Saved Elements...\" -underline 1 -command { set choice 91 }" );
			cmd( "$w add command -label \"Export Sensitivity Limits...\" -underline 2 -command { set choice 90 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Set Equation File...\" -underline 2 -accelerator Ctrl+U -command { set choice 28 }" );
			cmd( "$w add command -label \"Upload Equation File\" -command { set choice 51 }" );
			cmd( "$w add command -label \"Offload Equation File...\" -underline 1 -command { set choice 52 }" );
			cmd( "$w add command -label \"Compare Equation Files...\" -underline 2 -command { set choice 53 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label Quit -underline 0 -accelerator Ctrl+Q -command { set choice 11 }" );

			cmd( "set w .m.model" );
			cmd( "menu $w -tearoff 0" );
			cmd( ".m add cascade -label Model -menu $w -underline 0" );
			cmd( "$w add command -label \"Add Variable...\" -underline 4 -accelerator Ctrl+V -command { set param 0; set choice 2 }" );
			cmd( "$w add command -label \"Add Parameter...\" -underline 4 -accelerator Ctrl+P -command { set param 1; set choice 2 }" );
			cmd( "$w add command -label \"Add Function...\" -underline 5 -accelerator Ctrl+N -command { set param 2; set choice 2 }" );
			cmd( "$w add command -label \"Add Descending Object...\" -underline 4 -accelerator Ctrl+D -command { set choice 3 }" );
			cmd( "$w add command -label \"Add Parent Object...\" -underline 5 -command { set choice 32 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Change Element...\" -underline 0 -command { set choice 7 }" );
			cmd( "$w add command -label \"Change Object...\" -underline 7 -command { set choice 6 }" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Find Element...\" -underline 0 -accelerator Ctrl+F -command { set choice 50 }" );

			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Create Model Report...\" -underline 7 -command { set choice 36 }" );
			cmd( "$w add command -label \"Create LaTex Tables\" -underline 9 -command { set choice 57 }" );
			cmd( "$w add command -label \"Create LaTex References\" -underline 13 -command { set choice 92 }" );
			cmd( "$w add command -label \"Import Descriptions\" -underline 0 -command { set choice 43 }" );

			cmd( "$w add separator" );
			
			cmd( "$w add checkbutton -label \"Enable Structure Window\" -underline 7 -accelerator Ctrl+Tab -variable strWindowOn -command { set choice 70 }" );
			cmd( "$w add checkbutton -label \"Ignore Equation File\" -underline 0 -variable ignore_eq_file -command { set choice 54 }" );

			cmd( "set w .m.data" );
			cmd( "menu $w -tearoff 0" );
			cmd( ".m add cascade -label Data -menu $w -underline 0" );
			cmd( "$w add command -label \"Initial Values...\" -command {set choice 21} -underline 0 -accelerator Ctrl+I" );
			cmd( "$w add command -label \"Numbers of Objects....\" -command {set choice 19} -accelerator Ctrl+O -underline 0" );
			
			cmd( "$w add separator" );
			
			cmd( "$w add cascade -label \"Sensitivity Analysis\" -underline 0 -menu $w.setsens" );

			cmd( "$w add separator" );

			cmd( "$w add command -label \"Analysis of Results...\" -command { set choice 26 } -underline 0 -accelerator Ctrl+A" );
			cmd( "$w add command -label \"Data Browse...\" -command { set choice 34 } -underline 5 -accelerator Ctrl+B" );

			cmd( "set w .m.data.setsens" );
			cmd( "menu $w -tearoff 0" );
			cmd( "$w add command -label \"Full (online)\" -underline 0 -command { set choice 62 }" );
			cmd( "$w add command -label \"Full (batch)\" -underline 6 -command { set choice 63 }" );
			cmd( "$w add command -label \"MC Point Sampling (batch)...\" -underline 0 -command { set choice 71 }" );
			cmd( "$w add command -label \"MC Range Sampling (batch)...\" -underline 3 -command { set choice 80 }" );
			cmd( "$w add command -label \"EE Sampling (batch)...\" -underline 0 -command { set choice 81 }" );
			cmd( "$w add command -label \"NOLH Sampling (batch)...\" -underline 0 -command { set choice 72 }" );

			cmd( "set w .m.run" );
			cmd( "menu $w -tearoff 0" );
			cmd( ".m add cascade -label Run -menu $w -underline 0" );
			cmd( "$w add command -label Run -underline 0 -accelerator Ctrl+R -command { set choice 1 }" );
			cmd( "$w add command -label \"Start 'No Window' Batch...\" -underline 0 -command { set choice 69 }" );
			cmd( "$w add command -label \"Create/Start Parallel Batch...\" -underline 11 -command { set choice 68 }" );
			cmd( "$w add separator" );
			cmd( "$w add command -label \"Simulation Settings...\" -underline 2 -accelerator Ctrl+M -command { set choice 22 }" );

			cmd( "$w add separator" );
			
			cmd( "$w add cascade -label \"Show Elements to\" -underline 17 -menu $w.show" );
			cmd( "$w add cascade -label \"Remove Flags to\" -underline 15 -menu $w.rem" );

			cmd( "$w add separator" );
			
			cmd( "$w add command -label \"Close Run-time Plots\" -command { set choice 40 } -underline 0" );

			cmd( "set w .m.run.show" );
			cmd( "menu $w -tearoff 0" );
			cmd( "$w add command -label Save -underline 0 -command { set choice 39 }" );
			cmd( "$w add command -label \"Run-time Plot\" -underline 0 -command { set choice 84 }" );
			cmd( "$w add command -label Debug -underline 0 -command { set choice 85 }" );
			cmd( "$w add command -label Initialize -underline 0 -command { set choice 49 }" );
			cmd( "$w add command -label Observe -underline 0 -command { set choice 42 }" );
			cmd( "$w add command -label Parallelize -underline 0 -command { set choice 86 }" );
			cmd( "$w add command -label \"Sensitivity Analysis\" -underline 1 -command { set choice 66 }" );
			
			cmd( "set w .m.run.rem" );
			cmd( "menu $w -tearoff 0" );
			cmd( "$w add command -label Save -underline 0 -accelerator Ctrl+G -command { set choice 30 }" );
			cmd( "$w add command -label \"Run-time Plot\" -underline 0 -command { set choice 31 }" );
			cmd( "$w add command -label Debug -underline 0 -accelerator Ctrl+F -command { set choice 27 }" );
			cmd( "$w add command -label Parallelize -underline 0 -command { set choice 87 }" );
			
			cmd( "set w .m.help" );
			cmd( "menu $w -tearoff 0" );
			cmd( ".m add cascade -label Help -menu $w -underline 0" );
			cmd( "$w add command -label \"Help on Browser\" -underline 0 -accelerator F1 -command { LsdHelp browser.html }" );
			cmd( "$w add command -label \"LSD Quick Help\" -underline 4 -command { LsdHelp LSD_quickhelp.html }" );
			cmd( "$w add command -label \"LSD Documentation\" -underline 4 -command { LsdHelp LSD_documentation.html }" );
			cmd( "$w add separator" );
			cmd( "$w add command -label \"LMM Primer Tutorial\" -underline 4 -command { LsdHelp LMM_primer.html }" );
			cmd( "$w add command -label \"Using LSD Models Tutorial\" -underline 0 -command { LsdHelp model_using.html }" );
			cmd( "$w add command -label \"Writing LSD Models Tutorial\" -underline 0 -command { LsdHelp model_writing.html }" );
			cmd( "$w add separator" );
			cmd( "if { $tcl_platform(platform) == \"unix\" } { $w add command -label \"Set Browser\" -command { set choice 48 } -underline 0 }" );
			cmd( "$w add command -label \"Model Report\" -underline 0 -command { set choice 44 }" );
			cmd( "$w add separator" );
			cmd( "$w add command -label \"About LSD...\" -underline 0 -command { LsdAbout {%s} {%s} }", _LSD_VERSION_, _LSD_DATE_  );

			// set shortcuts on open windows
			set_shortcuts( ".", "browser.html" );
			set_shortcuts( ".log", "log.html" );

			// Button bar
			cmd( "destroy .bbar" );
			cmd( "frame .bbar -bd 2" );

			cmd( "button .bbar.open -image openImg -relief $bRlf -overrelief $ovBrlf -command {set choice 17}" );
			cmd( "button .bbar.reload -image reloadImg -relief $bRlf -overrelief $ovBrlf -command {set choice 38}" );
			cmd( "button .bbar.save -image saveImg -relief $bRlf -overrelief $ovBrlf -command {set choice 18}" );
			cmd( "button .bbar.struct -image structImg -relief $bRlf -overrelief $ovBrlf -command {set strWindowOn [expr ! $strWindowOn]; set choice 70} -state $strWindowB" );
			cmd( "button .bbar.find -image findImg -relief $bRlf -overrelief $ovBrlf -command {set choice 50}" );
			cmd( "button .bbar.init -image initImg -relief $bRlf -overrelief $ovBrlf -command {set choice 21}" );
			cmd( "button .bbar.number -image numberImg -relief $bRlf -overrelief $ovBrlf -command {set choice 19}" );
			cmd( "button .bbar.set -image setImg -relief $bRlf -overrelief $ovBrlf -command {set choice 22}" );
			cmd( "button .bbar.run -image runImg -relief $bRlf -overrelief $ovBrlf -command {set choice 1}" );
			cmd( "button .bbar.data -image dataImg -relief $bRlf -overrelief $ovBrlf -command {set choice 34}" );
			cmd( "button .bbar.result -image resultImg -relief $bRlf -overrelief $ovBrlf -command {set choice 26}" );
			cmd( "label .bbar.tip -textvariable ttip -font {Arial 8} -fg gray -width 17 -anchor w" );

			cmd( "bind .bbar.open <Enter> {set ttip \"Open...\"}" );
			cmd( "bind .bbar.open <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.reload <Enter> {set ttip \"Reload\"}" );
			cmd( "bind .bbar.reload <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.save <Enter> {set ttip \"Save\"}" );
			cmd( "bind .bbar.save <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.struct <Enter> { if $strWindowOn {set ttip \"Hide structure\"} {set ttip \"Show structure\"}}" );
			cmd( "bind .bbar.struct <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.find <Enter> {set ttip \"Find element...\"}" );
			cmd( "bind .bbar.find <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.init <Enter> {set ttip \"Initial values...\"}" );
			cmd( "bind .bbar.init <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.number <Enter> {set ttip \"Num. objects...\"}" );
			cmd( "bind .bbar.number <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.set <Enter> {set ttip \"Settings...\"}" );
			cmd( "bind .bbar.set <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.run <Enter> {set ttip \"Run\"}" );
			cmd( "bind .bbar.run <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.data <Enter> {set ttip \"Data browse...\"}" );
			cmd( "bind .bbar.data <Leave> {set ttip \"\"}" );
			cmd( "bind .bbar.result <Enter> {set ttip \"Analysis...\"}" );
			cmd( "bind .bbar.result <Leave> {set ttip \"\"}" );

			cmd( "pack .bbar.open .bbar.reload .bbar.save .bbar.struct .bbar.find .bbar.init .bbar.number .bbar.set .bbar.run .bbar.data .bbar.result .bbar.tip -padx 3 -side left" );
			cmd( "pack .bbar -anchor w -fill x" );
		}

		cmd( "pack .l.v.c.v_scroll -side right -fill y" );
		cmd( "pack .l.v.c.var_name -fill both -expand yes" );
		cmd( "pack .l.v.lab -fill x" );
		cmd( "pack .l.v.c -fill both -expand yes" );

		cmd( "pack .l.s.c.v_scroll -side right -fill y" );
		cmd( "pack .l.s.c.son_name -fill both -expand yes" );
		cmd( "pack .l.s.lab -fill x" );
		cmd( "pack .l.s.c -fill both -expand yes" );

		cmd( "pack .l.p.up_name .l.p.tit" );
		cmd( "pack .l.p -fill x" );

		cmd( "pack .l.s .l.v -side left -fill both -expand yes" );

		cmd( "pack .l -fill both -expand yes" );
	}

	cmd( "wm deiconify ." );
	cmd( "update" );

	main_cycle:

	cmd( "if [ info exists ModElem ] { set ModElem [ lsort -dictionary $ModElem ] }" );

	cmd( "if { $listfocus == 1 } { focus .l.v.c.var_name; .l.v.c.var_name selection clear 0 end; .l.v.c.var_name selection set $itemfocus; .l.v.c.var_name activate $itemfocus; .l.v.c.var_name see $itemfocus }" );
	cmd( "if { $listfocus == 2 } { focus .l.s.c.son_name; .l.s.c.son_name selection clear 0 end; .l.s.c.son_name selection set $itemfocus; .l.s.c.son_name activate $itemfocus }" );

	cmd( "if $strWindowOn { set strWindowB active } { set strWindowB normal }" );
	cmd( "set useCurrObj yes" );	// flag to select among the current or the clicked object

	*choice = 0;

	// main command loop
	while ( ! *choice  && ! choice_g )
	{
		try
		{
			Tcl_DoOneEvent( 0 );
		}
		catch ( bad_alloc& ) 	// raise memory problems
		{
			throw;
		}
		catch ( ... )				// ignore the rest
		{
			goto main_cycle;
		}
	}   


	if ( choice_g )		// coming from the structure window
	{
		*choice = choice_g;
		choice_g = 0;
		res_g = ( char * ) Tcl_GetVar( inter, "res_g", 0 );
		cmd( "focus .l.v.c.var_name" );
	}

	// update focus memory
	cmd( "if { [ .l.v.c.var_name curselection ] != \"\" } { set listfocus 1; set itemfocus [ .l.v.c.var_name curselection ] } { if { [ .l.s.c.son_name curselection ] != \"\" } { set listfocus 2; set itemfocus [ .l.s.c.son_name curselection ] } }" );

	if ( actual_steps > 0 )
	{ 	// search the sorted list of choices that are bad with existing run data
		if ( bsearch( choice, badChoices, NUM_BAD_CHOICES, sizeof ( int ), comp_ints ) != NULL )
		{ 	// prevent changing data if analysis is open
			cmd( "if [ winfo exists .da ] { tk_messageBox -parent . -type ok -icon warning -title Warning -message \"Analysis of Results window is open\" -detail \"Please close it before proceeding with any option that requires existing data to be removed.\"; set daOpen 1 } { set daOpen 0 }" );
			if ( ! strcmp( Tcl_GetVar( inter, "daOpen", 0 ), "1" ) )
				goto main_cycle;

			cmd( "set temp 38" );

			cmd( "set T .warn" );
			cmd( "newtop $T \"Warning\"" );

			cmd( "label $T.l1 -fg red -text \"Simulation just run\"" );
			cmd( "label $T.l2 -text \"Data loaded is the last step of a previous run.\nThe requested operation is inappropriate now.\"" );

			cmd( "frame $T.f" );
			cmd( "label $T.f.l -text \"Choose one option to proceed\"" );

			cmd( "frame $T.f.o -relief groove -bd 2" );
			cmd( "radiobutton $T.f.o.reload -variable temp -value 38 -text \"Reload the current initial configuration\"" );
			cmd( "radiobutton $T.f.o.load -variable temp -value 17 -text \"Load a new initial configuration\"" );     
			cmd( "radiobutton $T.f.o.ar -variable temp -value 26 -text \"Analyze the final results\"" );     
			cmd( "pack $T.f.o.reload $T.f.o.load $T.f.o.ar -anchor w" );

			cmd( "pack $T.f.l $T.f.o" );

			cmd( "pack $T.l1 $T.l2 $T.f -ipadx 5 -padx 5 -pady 5" );

			cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp LSD_quickhelp.html#problem } { set choice 2 }" );
			cmd( "bind $T <Return> {set choice 1}" );

			cmd( "showtop $T centerS" );
			cmd( "bell; update" );

			*choice = 0;
			while ( *choice == 0 && choice_g == 0 )
				Tcl_DoOneEvent( 0 );

			cmd( "destroytop .warn" );

			if ( *choice == 1 )
				cmd( "set choice $temp" );
			else 
				goto main_cycle;
		}
	} 
	 
	if ( *choice != 35 )
	{
		cmd( "if { [ winfo exists . ] == 1 } { bind . <Destroy> { } }" );
		cmd( "if { [ winfo exists .str ] == 1 } { bind .str <Destroy> { } }" );
		cmd( "if { [ winfo exists .list ] == 1 } { destroy .list }" );
	}

	return *choice;
}

/****************************************************
OPERATE
****************************************************/
object *operate( int *choice, object *r )
{
char observe, initial, cc, *lab1, *lab2, *lab3, *lab4, lab[ 2 * MAX_PATH_LENGTH ], lab_old[ 2 * MAX_PATH_LENGTH ], ch[ 2 * MAX_PATH_LENGTH ], out_file[ MAX_PATH_LENGTH ], out_dir[ MAX_PATH_LENGTH ], out_bat[ MAX_PATH_LENGTH ], win_dir[ MAX_PATH_LENGTH ];
int sl, done = 0, num, i, j, param, save, plot, nature, numlag, k, lag, fSeq, ffirst, fnext, temp[ 10 ];
long nLinks;
double fake = 0;
bool saveAs, delVar, renVar, reload, table;
FILE *f;
bridge *cb;
object *n, *cur, *cur1, *cur2;
variable *cur_v, *cv, *app;
result *rf;					// pointer for results files (may be zipped or not)
sense *cs;
description *cur_descr;
struct stat stExe, stMod;

if ( ! redrawReq )
	redrawRoot = false;		// assume no browser redraw
else
{
	redrawRoot = true;		// handle pending async redraw request
	redrawReq = false;
}

switch ( *choice )
{

// Add a Variable to the current or the pointed object (defined in tcl $vname)
case 2:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	Tcl_LinkVar( inter, "done", ( char * ) &done, TCL_LINK_INT );
	Tcl_LinkVar( inter, "copy_param", ( char * ) &param, TCL_LINK_INT );
	Tcl_LinkVar( inter, "num", ( char * ) &num, TCL_LINK_INT );

	cmd( "set T .addelem" );
	cmd( "newtop $T \"Add Element\" { set done 2 }" );

	cmd( "set copy_param $param" );
	cmd( "set num 0" );
	cmd( "set lab \"\"" );

	if ( param == 0 )
	{
		cmd( "frame $T.l" );
		cmd( "label $T.l.l1 -text \"New variable in object:\"" );
		cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
		cmd( "pack $T.l.l1 $T.l.l2 -side left -padx 2" );

		cmd( "frame $T.f" );
		cmd( "label $T.f.lab_ent -text \"Variable name\"" );
		cmd( "label $T.f.lab_num -text \"Maximum lags\"" );
		cmd( "label $T.f.sp -width 5" );
		cmd( "entry $T.f.ent_var -width 20 -textvariable lab -justify center" );
		cmd( "entry $T.f.ent_num -width 2 -validate focusout -vcmd { if { [ string is integer -strict %%P ] && %%P >= 0 } { set num %%P; return 1 } { %%W delete 0 end; %%W insert 0 $num; return 0 } } -invcmd { bell } -justify center" );
		cmd( "bind $T.f.ent_num <KeyPress-Return> {focus $T.b.ok}" );
		cmd( "pack $T.f.lab_ent $T.f.ent_var $T.f.sp $T.f.lab_num $T.f.ent_num -side left -padx 2" );
	}

	if ( param == 2 )
	{
		cmd( "frame $T.l" );
		cmd( "label $T.l.l1 -text \"New function in object:\"" );
		cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
		cmd( "pack $T.l.l1 $T.l.l2 -side left -padx 2" );

		cmd( "frame $T.f" );
		cmd( "label $T.f.lab_ent -text \"Function name\"" );
		cmd( "entry $T.f.ent_var -width 20 -textvariable lab -justify center" );
		cmd( "pack $T.f.lab_ent $T.f.ent_var -side left -padx 2" );
	}

	if ( param == 1 )
	{ //insert a parameter
		cmd( "frame $T.l" );
		cmd( "label $T.l.l1 -text \"New parameter in object:\"" );
		cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
		cmd( "pack $T.l.l1 $T.l.l2 -side left -padx 2" );

		cmd( "frame $T.f" );
		cmd( "label $T.f.lab_ent -text \"Parameter name\"" );
		cmd( "entry $T.f.ent_var -width 20 -textvariable lab -justify center" );
		cmd( "pack $T.f.lab_ent $T.f.ent_var -side left -padx 2" );
	}

	cmd( "bind $T.f.ent_var <KeyPress-Return> {focus $T.b.ok}" );

	cmd( "set w $T.d" );
	cmd( "frame $w" );
	cmd( "frame $w.f -bd 2 -relief groove" );
	cmd( "label $w.f.lab -text \"Description\"" );
	cmd( "scrollbar $w.f.yscroll -command \"$w.f.text yview\"" );
	cmd( "text $w.f.text -undo 1 -wrap word -width 60 -height 6 -relief sunken -yscrollcommand \"$w.f.yscroll set\" -font \"$font_small\"" );
	cmd( "pack $w.f.yscroll -side right -fill y" );
	cmd( "pack $w.f.lab $w.f.text -expand yes -fill both" );
	cmd( "pack $w.f" );

	cmd( "pack $T.l $T.f $T.d -pady 5" );
	if ( param == 0 )
		cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html#AddAVar } { set done 2 }" );
	else
		if ( param == 1 )
			cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html#AddAPar } { set done 2 }" );
		else
			cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html } { set done 2 }" );

	cmd( "showtop $T topleftW" );
	cmd( "focus $T.f.ent_var; $T.f.ent_var selection range 0 end" );

	here_newelem:

	if ( param == 0 )
		cmd( "write_any .addelem.f.ent_num $num" ); 

	while ( done == 0 )
		Tcl_DoOneEvent( 0 );

	if ( param == 0 )
		cmd( "set num [ .addelem.f.ent_num get ]" ); 

	if ( done == 1 )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "lab", 0 );
		strncpy( lab, lab1, MAX_ELEM_LENGTH - 1 );
		sl = strlen( lab );
		if ( sl != 0 )
		{
			for ( cur = r; cur->up != NULL; cur = cur->up );
			
			done = check_label( lab, cur );
			if ( done == 1 )
			{
				cmd( "tk_messageBox -parent .addelem -title Error -icon error -type ok -message \"The name already exists in the model\" -detail \"Choose a different name and try again.\"" );
				cmd( "focus .addelem.f.ent_var; .addelem.f.ent_var selection range 0 end" );
				done = 0;
				goto here_newelem;
			}
			
			if ( done == 2 )
			{
				cmd( "tk_messageBox -parent .addelem -title Error -icon error -type ok -message \"Invalid characters in name\" -detail \"Names must begin with a letter (English alphabet) or underscore ('_') and may contain letters, numbers or '_' but no spaces. Choose a different label and try again.\"" );
				cmd( "focus .addelem.f.ent_var; .addelem.f.ent_var selection range 0 end" );
				done = 0;
				goto here_newelem;
			}

			if ( done == 0 )
			{
				cmd( "set text_description [.addelem.d.f.text get 1.0 end]" );
				cmd( "if { $text_description==\"\\n\"} {set text_description \"(no description available)\"} {}" );
				lab1 = ( char * ) Tcl_GetVar( inter, "text_description", 0 );
				if ( param == 1 )
					add_description( lab, "Parameter", lab1 );
				if ( param == 0 )
					add_description( lab, "Variable", lab1 );
				if ( param == 2 )
					add_description( lab, "Function", lab1 );
				
				for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
				{ 
					cur->add_empty_var( lab );
					cv = cur->search_var( NULL, lab );
					if ( param != 0 )
						num = 0;
					cv->val = new double[ num + 1 ];
					cv->save = 0;
					cv->param = param;
					cv->num_lag = num;
					cv->debug = 'n';
					if ( ( param == 0 && num == 0 ) || param == 2 )
						cv->data_loaded = '+';
					else
						cv->data_loaded = '-';
					
					for ( i = 0; i < num + 1; ++i )
						cv->val[ i ] = 0;
					
					justAddedVar = true;	// flag variable just added (for acquiring focus)
				}
				
				unsaved_change( true );	// signal unsaved change
			}
		}
	}

	cmd( "destroytop .addelem" );
	redrawRoot = ( done == 2 ) ? false : true;

	if ( done != 2 )
		cmd( "if [ info exists ModElem ] { lappend ModElem %s }", lab );

	if ( cur2 != NULL )			// restore original current object
		r = cur2;

	Tcl_UnlinkVar( inter, "done" );
	Tcl_UnlinkVar( inter, "num" );
	Tcl_UnlinkVar( inter, "copy_param" );
	cmd( "unset done" );

break;


// Add a Descendent type to the current or the pointed object (defined in tcl $vname)
// and assigns the number of its instances.
case 3:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	Tcl_LinkVar( inter, "done", ( char * ) &done, TCL_LINK_INT );

	cmd( "set lab \"\"" );

	cmd( "set T .addobj" );
	cmd( "newtop $T \"Add Object\" { set done 2 }" );

	cmd( "frame $T.l" );
	cmd( "label $T.l.l1 -text \"New object descending from:\"" );
	cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
	cmd( "pack $T.l.l1 $T.l.l2 -side left -padx 2" );

	cmd( "frame $T.f" );
	cmd( "label $T.f.lab_ent -text \"Object name\"" );
	cmd( "entry $T.f.ent_var -width 20 -textvariable lab -justify center" );
	cmd( "pack $T.f.lab_ent $T.f.ent_var -side left -padx 2" );
	cmd( "bind $T.f.ent_var <KeyPress-Return> {focus $T.b.ok}" );

	cmd( "set w $T.d" );
	cmd( "frame $w" );
	cmd( "frame $w.f -bd 2 -relief groove" );
	cmd( "label $w.f.lab -text \"Description\"" );
	cmd( "scrollbar $w.f.yscroll -command \"$w.f.text yview\"" );
	cmd( "text $w.f.text -undo 1 -wrap word -width 60 -height 6 -relief sunken -yscrollcommand \"$w.f.yscroll set\" -font \"$font_small\"" );
	cmd( "pack $w.f.yscroll -side right -fill y" );
	cmd( "pack $w.f.lab $w.f.text -expand yes -fill both" );
	cmd( "pack $w.f" );

	cmd( "pack $T.l $T.f $w -pady 5" );
	cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html#AddADesc } { set done 2 }" );

	cmd( "showtop $T topleftW" );
	cmd( "focus $T.f.ent_var; $T.f.ent_var selection range 0 end" );

	here_newobject:
	
	while ( done == 0 )
		Tcl_DoOneEvent( 0 );

	if ( done == 1 )
	{
		lab1= ( char * ) Tcl_GetVar( inter, "lab", 0 );
		strncpy( lab, lab1, MAX_ELEM_LENGTH - 1 );
		if ( strlen( lab ) == 0 )
			goto here_endobject;
	
		for ( cur = r; cur->up != NULL; cur = cur->up );
		
		done = check_label( lab, cur ); // check that the label does not exist already
		if ( done == 1 )
		{
			cmd( "tk_messageBox -parent .addobj -title Error -icon error -type ok -message \"The name already exists in the model\" -detail \"Choose a different name and try again.\"" );
			cmd( "focus .addobj.f.ent_var; .addobj.f.ent_var selection range 0 end" );
			done = 0;
			goto here_newobject;
		}
		
		if ( done == 2 )
		{
			cmd( "tk_messageBox -parent .addobj -title Error -icon error -type ok -message \"Invalid characters in name\" -detail \"Names must begin with a letter (English alphabet) or underscore ('_') and may contain letters, numbers or '_' but no spaces. Choose a different label and try again.\"" );
			cmd( "focus .addobj.f.ent_var; .addobj.f.ent_var selection range 0 end" );
			done = 0;
			goto here_newobject;
		}
		
		r->add_obj( lab, 1, 1 );
		cmd( "set text_description [.addobj.d.f.text get 1.0 end]" );  
		cmd( "if { $text_description==\"\\n\" || $text_description==\"\"} {set text_description \"(no description available)\"} {}" );
		lab1 = ( char * ) Tcl_GetVar( inter, "text_description", 0 );
		add_description( lab, "Object", lab1 );
		
		unsaved_change( true );	// signal unsaved change
		redrawRoot = true;			// force browser redraw
	}

	here_endobject:

	if ( cur2 != NULL )			// restore original current object
		r = cur2;

	cmd( "destroytop .addobj" );
	Tcl_UnlinkVar( inter, "done" );
	cmd( "unset done" );

break;


// Insert a parent Object just above the current or pointed object (defined in tcl $vname)
case 32:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	Tcl_LinkVar( inter, "done", ( char * ) &done, TCL_LINK_INT );

	if ( r->up == NULL )
	{
		cmd( "set answer [ tk_messageBox -parent . -type okcancel -default cancel -title Error -icon error -message \"Cannot insert a parent of Root\" -detail \"Press 'OK' if you want the new object to be a descendant of Root and contain all current descendants from Root.\" ]; if [ string equal -nocase $answer ok ] { set done 1 } { set done 2 }" );
		if ( done == 2 )
			goto here_endparent;
		done = 0;
	}

	cmd( "set lab \"\"" );

	cmd( "set T .inspar" );
	cmd( "newtop $T \"Add Parent\" { set done 2 }" );

	cmd( "frame $T.l" );
	cmd( "label $T.l.l1 -text \"New parent to:\"" );
	cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
	cmd( "label $T.l.l3 -text \"descending from:\"" );
	cmd( "label $T.l.l4 -text \"%s\" -fg red", r->up == NULL ? "(none)" : r->up->label );
	cmd( "pack $T.l.l1 $T.l.l2 $T.l.l3 $T.l.l4 -side left -padx 2" );

	cmd( "frame $T.f" );
	cmd( "label $T.f.lab_ent -text \"Object name\"" );
	cmd( "entry $T.f.ent_var -width 20 -textvariable lab -justify center" );
	cmd( "pack $T.f.lab_ent $T.f.ent_var -side left -padx 2" );
	cmd( "bind $T.f.ent_var <KeyPress-Return> {focus $T.b.ok}" );

	cmd( "set w $T.d" );
	cmd( "frame $w" );
	cmd( "frame $w.f -bd 2 -relief groove" );
	cmd( "label $w.f.lab -text \"Description\"" );
	cmd( "scrollbar $w.f.yscroll -command \"$w.f.text yview\"" );
	cmd( "text $w.f.text -undo 1 -wrap word -width 60 -height 6 -relief sunken -yscrollcommand \"$w.f.yscroll set\" -font \"$font_small\"" );
	cmd( "pack $w.f.yscroll -side right -fill y" );
	cmd( "pack $w.f.lab $w.f.text -expand yes -fill both" );
	cmd( "pack $w.f" );

	cmd( "pack $T.l $T.f $w -pady 5" );
	cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html#InsertAParent } { set done 2 }" );

	cmd( "showtop $T topleftW" );
	cmd( "focus $T.f.ent_var; $T.f.ent_var selection range 0 end" );

	here_newparent:
	
	while ( done == 0 )
		Tcl_DoOneEvent( 0 );

	if ( done == 1 )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "lab", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			goto here_endparent;
	
		sscanf( lab1, "%99s", lab );
		for ( cur = r; cur->up != NULL; cur = cur->up );
		done = check_label( lab1, cur ); // check that the label does not exist already
		if ( done == 1 )
		{
			cmd( "tk_messageBox -parent .inspar -title Error -icon error -type ok -message \"The name already exists in the model\" -detail \"Choose a different name and try again.\"" );
			cmd( "focus .inspar.f.ent_var; .inspar.f.ent_var selection range 0 end" );
			done = 0;
			goto here_newparent;
		}
	   
		if ( done == 2 )
		{
			cmd( "tk_messageBox -parent .inspar -title Error -icon error -type ok -message \"Invalid characters in name\" -detail \"Names must begin with a letter (English alphabet) or underscore ('_') and may contain letters, numbers or '_' but no spaces. Choose a different label and try again.\"" );
			cmd( "focus .inspar.f.ent_var; .inspar.f.ent_var selection range 0 end" );
			done = 0;
			goto here_newparent;
		}

		if ( r->up == NULL )
		{
			cur = new object;
			cur->init( NULL, lab );
			cur->next = NULL;
			cur->up=r;
			cur->to_compute = 1;
			cur->b = r->b;
			r->b=new bridge;
			r->b->next = NULL;
			r->b->blabel=new char[ strlen( lab ) + 1 ];
			strcpy( r->b->blabel, lab );
			r->b->head = cur;
			cur->v = r->v;
			r->v = NULL;
			
			for ( cur1 = cur->b->head; cur1 != NULL; cur1 = cur1->next )
				cur1->up=cur;
		}
		else
		{
			r->insert_parent_obj_one( lab );
			r = r->up;
		}
	}

	cmd( "set text_description [.inspar.d.f.text get 1.0 end]" );  
	cmd( "if { $text_description==\"\\n\" || $text_description==\"\"} {set text_description \"(no description available)\"} {}" );
	lab1 = ( char * ) Tcl_GetVar( inter, "text_description", 0 );
	add_description( lab, "Object", lab1 );

	unsaved_change( true );	// signal unsaved change
	redrawRoot = true;			// force browser redraw

	here_endparent:

	if ( cur2 != NULL )			// restore original current object
		r = cur2;

	cmd( "destroytop .inspar" );
	Tcl_UnlinkVar( inter, "done" );
	cmd( "unset done" );

break;


// Move browser to show one of the descendant object (defined in tcl $vname)
case 4:

	*choice = 0;
	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) || ! strcmp( lab1, "(none)" ) )
		break;
	
	sscanf( lab1, "%99s", lab_old );

	n = r->search( lab_old );
	if ( n == NULL )
	{
		plog( "\nDescendant %s not found", "", lab_old );
		break;
	}

	cmd( "set cur 0; set listfocus 2; set itemfocus 0" );

	redrawRoot = true;			// force browser redraw
	return n;


// Move browser to show the parent object
case 5:

	*choice = 0;
	if ( r->up == NULL )
		return r;
	
	for ( i = 0, cb = r->up->b; cb->head != r; cb = cb->next, ++i );
	
	cmd( "set cur 0; set listfocus 2; set itemfocus %d", i ); 

	redrawRoot = true;					// force browser redraw
	return r->up;


// Edit current Object and give the option to disable the computation (defined in tcl $vname)
case 6:

	cmd( "if $useCurrObj { set lab %s } { if [ info exists vname ] { set lab $vname } { set lab \"\" } }; set useCurrObj yes ", r->label  );
	lab1 = ( char * ) Tcl_GetVar( inter, "lab", 0 );

	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	// check if current or pointed object and save current if needed
	if ( strcmp( r->label, lab_old ) )	// check if not current variable
	{
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	if ( ! strcmp( r->label, "Root" ) )	// cannot change Root
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Cannot change Root\" -detail \"Please select an existing object or insert a new one before using this option.\"" );
		break;
	}

	cur_descr = search_description( lab_old );
	if ( cur_descr == NULL )
	{
		add_description( lab_old, "Object", "(no description available)" );
		cur_descr = search_description( lab_old );
	} 
	  
	cmd( "set to_compute %d", r->to_compute );

	cmd( "set T .objprop" );
	cmd( "newtop $T \"Change Object\" { set choice 2 }" );

	cmd( "frame $T.h" );

	cmd( "label $T.h.lab_ent -text \"Object:\"" );
	cmd( "label $T.h.ent_var -fg red -text $lab" );
	cmd( "pack $T.h.lab_ent $T.h.ent_var -side left -padx 2" );

	cmd( "frame $T.b0" );
	cmd( "button $T.b0.prop -width $butWid -text Rename -command { set choice 83 } -underline 0" );
	cmd( "button $T.b0.num -width $butWid -text Number -command { set useCurrObj yes; set choice 33 } -underline 0" );
	cmd( "button $T.b0.del -width $butWid -text Delete -command { set choice 74 } -underline 0" );
	cmd( "pack $T.b0.prop $T.b0.num $T.b0.del -padx 10 -side left" );

	cmd( "frame $T.b1" );
	cmd( "checkbutton $T.b1.com -text \"Compute: force the computation of the variables in this object\" -variable to_compute -underline 0" );
	cmd( "pack $T.b1.com" );

	cmd( "set w $T.desc" );

	cmd( "frame $w" );
	cmd( "frame $w.f -bd 2 -relief groove" );
	cmd( "label $w.f.int -text \"Description\"" );
	cmd( "scrollbar $w.f.yscroll -command \"$w.f.text yview\"" );
	cmd( "text $w.f.text -undo 1 -wrap word -width 60 -height 10 -relief sunken -yscrollcommand \"$w.f.yscroll set\" -font \"$font_small\"" );
	cmd( "pack $w.f.yscroll -side right -fill y" );
	cmd( "pack $w.f.int $w.f.text -anchor w -expand yes -fill both" );

	for ( i = 0; cur_descr->text[ i ] != ( char ) NULL; ++i )
		if ( cur_descr->text[ i ] != '[' && cur_descr->text[ i ] != ']' && cur_descr->text[ i ] != '{' && cur_descr->text[ i ] != '}' && cur_descr->text[ i ] != '\"' && cur_descr->text[ i ] != '\\' )
			cmd( "$w.f.text insert end \"%c\"", cur_descr->text[ i ] );
		else
			cmd( "$w.f.text insert end \"\\%c\"", cur_descr->text[ i ] );

	cmd( "$w.f.text delete \"end - 1 char\"" );
	cmd( "pack $w.f -fill x -expand yes" );

	cmd( "pack $T.h $T.b0 $T.b1 $w -pady 5" );

	cmd( "bind $T <Control-r> \"$T.b0.prop invoke\"; bind $T <Control-R> \"$T.b0.prop invoke\"" );
	cmd( "bind $T <Control-n> \"$T.b0.num invoke\"; bind $T <Control-N> \"$T.b0.num invoke\"" );
	cmd( "bind $T <Control-d> \"$T.b0.del invoke\"; bind $T <Control-D> \"$T.b0.del invoke\"" );
	cmd( "bind $T <Control-c> \"$T.b1.com invoke\"; bind $T <Control-C> \"$T.b1.com invoke\"" );

	cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp menumodel.html#ChangeObjName } { set choice 2 }" );

	cmd( "showtop $T topleftW" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	redrawRoot = false;			// no browser redraw yet
	done = *choice;

	if ( *choice != 2 )
	{
		unsaved_change( true );		// signal unsaved change

		// save description changes
		cmd( "set text_description \"[.objprop.desc.f.text get 1.0 end]\"" );
		change_descr_text( lab_old );
		lab1 = ( char * ) Tcl_GetVar( inter, "text_description", 0 );
		add_description( lab, "Object", lab1 );

		cmd( "set choice $to_compute" );

		if ( *choice != r->to_compute )
		{
			cur = blueprint->search( r->label );
			if ( cur != NULL )
				cur->to_compute = *choice;
			for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
				cur->to_compute = *choice;
		}   

		// control for elements to save in objects to be not computed
		if ( *choice == 0 )
			control_tocompute( r, r->label );

		redrawRoot = true;			// force browser redraw
	}

	cmd( "destroytop .objprop" );

	// dispatch chosen option
	if ( done > 2 )
	{
		cmd( "set vname $lab" );
		*choice = done;
		return r;
	}

break;


// Delete object (defined in tcl $vname)
case 74:
// Rename object (defined in tcl $vname)
case 83:

	nature = *choice;

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	for ( cur = r; cur->up != NULL; cur = cur->up );
	cur = cur->search( lab_old );		// get pointer to vname
	if ( cur == NULL )
	{
		plog( "\nObject %s not found", "", lab_old );
		break;
	}

	if ( nature == 74 )		// delete
	{
		cmd( "set answer [tk_messageBox -parent . -title Confirmation -icon question -type yesno -default yes -message \"Delete object?\" -detail \"Press 'Yes' to confirm deleting '$vname'\n\nNote that all descendants will be also deleted!\"]" );
		cmd( "switch $answer {yes {set choice 1} no {set choice 2}}" );
		if ( *choice == 2 )
			break;

		r = cur->up;
		wipe_out( cur );
	}
	else					// rename
	{
		cmd( "set T .chgnam" );
		cmd( "newtop $T \"Rename\" { set choice 2 }" );

		cmd( "frame $T.l" );
		cmd( "label $T.l.l -text \"Object:\"" );
		cmd( "label $T.l.n -fg red -text \"$vname\"" );
		cmd( "pack $T.l.l $T.l.n -side left -padx 2" );

		cmd( "frame $T.e" );
		cmd( "label $T.e.l -text \"New name\"" );
		cmd( "entry $T.e.e -width 20 -textvariable vname -justify center" );
		cmd( "pack $T.e.l $T.e.e -side left -padx 2" );

		cmd( "pack $T.l $T.e -padx 5 -pady 5" );

		cmd( "okcancel $T b { set choice 1 } { set choice 2 }" );
		
		cmd( "bind $T.e.e <KeyPress-Return> { set choice 1 }" );
		
		cmd( "showtop $T" );
		cmd( "focus $T.e.e" );
		cmd( "$T.e.e selection range 0 end" );

		here_newname:

		*choice = 0;
		while ( *choice == 0 )
			Tcl_DoOneEvent( 0 );

		if ( *choice == 1 )
		{
			lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
			if ( lab1 == NULL || ! strcmp( lab1, "" ) )		
				break;
			sscanf( lab1, "%99s", lab );

			if ( strcmp( lab, r->label ) )
			{
				for ( cur1 = r; cur1->up != NULL; cur1 = cur1->up );
				done = check_label( lab, cur1 );
				if ( done == 1 )
				{
					cmd( "tk_messageBox -parent .chgnam -title Error -icon error -type ok -message \"The name already exists in the model\" -detail \"Choose a different name and try again.\"" );
					cmd( "focus .chgnam.e.e; .chgnam.e.e selection range 0 end" );
					goto here_newname;
				}
				if ( done == 2 )
				{
					cmd( "tk_messageBox -parent .chgnam -title Error -icon error -type ok -message \"Invalid characters in name\" -detail \"Names must begin with a letter (English alphabet) or underscore ('_') and may contain letters, numbers or '_' but no spaces. Choose a different label and try again.\"" );
					cmd( "focus .chgnam.e.e; .chgnam.e.e selection range 0 end" );
					goto here_newname;
				}
				
				cmd( "if [ info exists ModElem ] { set pos [ lsearch -exact $ModElem \"%s\" ]; if { $pos >= 0 } { set ModElem [ lreplace $ModElem $pos $pos ]; lappend ModElem %s } }", r->label, lab );
	   
				change_descr_lab( cur->label, lab, "", "", "" );
				cur->chg_lab( lab );
			}
			else
				break;
		}
		
		cmd( "destroytop .chgnam" );
	}

	unsaved_change( true );				// signal unsaved change
	redrawRoot = true;					// force browser redraw

break;


// Edit variable name (defined in tcl $vname) and set debug/saving/plot flags
case 7:

	redrawRoot = true;					// assume browser redraw required
	int savei, parallel;

	cmd( "if { ! [ catch { set vname [ .l.v.c.var_name get [ .l.v.c.var_name curselection ] ] } ] && ! [ string equal $vname \"\" ] } { set choice 1 } { set choice 0 }" );
	if ( *choice == 0 )
	{ 
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No element selected\" -detail \"Please select an element (variable, parameter) before using this option.\"" );
		break;
	}

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) || ! strcmp( lab1, "(none)" ) )		
		break;
	sscanf( lab1, "%99s", lab_old );
	cv = r->search_var( NULL, lab_old );

	cur_descr=search_description( lab_old );
	if ( cur_descr == NULL )
	{
		if ( cv->param == 0 )
			add_description( lab_old, "Variable", "(no description available)" );
		if ( cv->param == 1 )
			add_description( lab_old, "Parameter", "(no description available)" );  
		if ( cv->param == 2 )
			add_description( lab_old, "Function", "(no description available)" );  
		plog( "\nWarning: description for '%s' not found. New one created.", "", lab_old );
		cur_descr=search_description( lab_old );
	} 

	Tcl_LinkVar( inter, "done", ( char * ) &done, TCL_LINK_INT );
	Tcl_LinkVar( inter, "debug", ( char * ) &num, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "save", ( char * ) &save, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "savei", ( char * ) &savei, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "plot", ( char * ) &plot, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "parallel", ( char * ) &parallel, TCL_LINK_BOOLEAN );

	save = cv->save;
	num = ( cv->debug == 'd' ) ? 1 : 0;
	plot = cv->plot;
	savei = cv->savei;
	parallel = cv->parallel;

	cmd( "set observe %d", cur_descr->observe=='y'?1:0 );
	cmd( "set initial %d", cur_descr->initial=='y'?1:0 );
	cmd( "set vname %s", lab_old );

	cmd( "set T .chgelem" );
	cmd( "newtop $T \"Change Element\" { set done 2 }" );

	cmd( "frame $T.h" );

	cmd( "frame $T.h.l" );
	
	if ( cv->param == 0 )
	  cmd( "label $T.h.l.lab_ent -text \"Variable:\"" );
	if ( cv->param == 1 )
	  cmd( "label $T.h.l.lab_ent -text \"Parameter:\"" );
	if ( cv->param == 2 )
	  cmd( "label $T.h.l.lab_ent -text \"Function:\"" );
	cmd( "label $T.h.l.ent_var -fg red -text $vname" );
	cmd( "pack $T.h.l.lab_ent $T.h.l.ent_var -side left -padx 2" );

	cmd( "frame $T.h.o" );
	cmd( "label $T.h.o.l -text \"In object:\"" );
	cmd( "label $T.h.o.obj -fg red -text \"%s\"", cv->up->label );
	cmd( "pack $T.h.o.l $T.h.o.obj -side left -padx 2" );

	cmd( "pack $T.h.l $T.h.o" );

	cmd( "frame $T.b0" );
	cmd( "button $T.b0.prop -width $butWid -text Properties -command {set done 5} -underline 1" );
	cmd( "button $T.b0.mov -width $butWid -text Move -command {set done 13} -underline 0" );
	cmd( "button $T.b0.del -width $butWid -text Delete -command {set done 10} -underline 2" );
	cmd( "pack $T.b0.prop $T.b0.mov $T.b0.del -padx 10 -side left" );

	cmd( "frame $T.b1" );

	cmd( "frame $T.b1.sav" );
	cmd( "checkbutton $T.b1.sav.n -text \"Save: save the series for analysis      \" -variable save -underline 0 -command { if $save { .chgelem.b1.sav.i configure -state normal } { set savei 0; .chgelem.b1.sav.i configure -state disabled } }" );
	cmd( "checkbutton $T.b1.sav.i -text \"Save in separate files\" -variable savei -underline 17" );
	cmd( "if { ! $save } { set savei 0; .chgelem.b1.sav.i configure -state disabled }" );
	cmd( "pack $T.b1.sav.n $T.b1.sav.i -side left -anchor w" );

	cmd( "checkbutton $T.b1.plt -text \"Run-time plot: observe the series during the simulation execution\" -variable plot -underline 9" );
	cmd( "checkbutton $T.b1.deb -text \"Debug: allow interruption after this equation/function\" -variable debug -underline 0" );
	cmd( "checkbutton $T.b1.par -text \"Parallel: allow multi-object parallel updating for this equation\" -variable parallel -underline 0" );

	switch ( cv->param )
	{
		case 1:
			cmd( "pack $T.b1.sav $T.b1.plt -anchor w" );
			break;
		case 2:
			cmd( "pack $T.b1.sav $T.b1.plt $T.b1.deb -anchor w" );
			cmd( "bind $T <Control-d> \"$T.b1.deb invoke\"; bind $T <Control-D> \"$T.b1.deb invoke\"" );
			break;
		case 0:
			cmd( "pack $T.b1.sav $T.b1.plt $T.b1.deb $T.b1.par -anchor w" );
			cmd( "bind $T <Control-d> \"$T.b1.deb invoke\"; bind $T <Control-D> \"$T.b1.deb invoke\"" );
			cmd( "bind $T <Control-p> \"$T.b1.par invoke\"; bind $T <Control-P> \"$T.b1.par invoke\"" );
	}

	cmd( "pack $T.h $T.b0 $T.b1 -pady 5" );

	cmd( "set Td $T.desc" );
	cmd( "frame $Td" );

	cmd( "frame $Td.opt" );
	cmd( "label $Td.opt.l -text \"Include in documentation to be\"" );
	cmd( "checkbutton $Td.opt.ini -text \"Initialized\" -variable initial -underline 0" ); 
	cmd( "checkbutton $Td.opt.obs -text \"Observed\" -variable observe -underline 0" );
	
	if ( cv->param == 1 || cv->num_lag > 0 )
	{
		cmd( "pack $Td.opt.l $Td.opt.obs $Td.opt.ini -side left" );
		cmd( "bind $T <Control-i> \"$Td.opt.ini invoke\"; bind $T <Control-I> \"$Td.opt.ini invoke\"" );
	}
	else
		cmd( "pack $Td.opt.l $Td.opt.obs -side left" );

	cmd( "frame $Td.f -bd 2 -relief groove" );
	cmd( "label $Td.f.int -text \"Description\"" );

	cmd( "scrollbar $Td.f.yscroll -command \"$Td.f.text yview\"" );
	cmd( "text $Td.f.text -undo 1 -wrap word -width 60 -height 8 -relief sunken -yscrollcommand \"$Td.f.yscroll set\" -font \"$font_small\"" );
	cmd( "pack $Td.f.yscroll -side right -fill y" );
	cmd( "pack $Td.f.int $Td.f.text -anchor w -expand yes -fill both" );

	for ( i = 0; cur_descr->text[ i ] != '\0'; ++i )
		if ( cur_descr->text[ i ] != '[' && cur_descr->text[ i ] != ']' && cur_descr->text[ i ] != '{' && cur_descr->text[ i ] != '}' && cur_descr->text[ i ] != '\"' && cur_descr->text[ i ] != '\\')
			cmd( "$Td.f.text insert end \"%c\"", cur_descr->text[ i ] );
		else
			cmd( "$Td.f.text insert end \"\\%c\"", cur_descr->text[ i ] );

	cmd( "$Td.f.text delete \"end - 1 char\"" );

	cmd( "frame $Td.b" );
	cmd( "button $Td.b.eq -width [ expr $butWid + 3 ] -text \"View Code\" -command {set done 3} -underline 3" );
	cmd( "button $Td.b.auto_doc -width [ expr $butWid + 3 ] -text \"Auto Descr.\" -command {set done 9} -underline 0" );
	cmd( "button $Td.b.us -width [ expr $butWid + 3 ] -text \"Using Element\" -command {set done 4} -underline 0" );
	cmd( "button $Td.b.using -width [ expr $butWid + 3 ] -text \"Elements Used\" -command {set done 7} -underline 0" );
	
	if ( ! strcmp( cur_descr->type, "Parameter" ) )
		cmd( "pack $Td.b.auto_doc $Td.b.us -padx 10 -side left" );
	else
	{
		cmd( "pack $Td.b.eq $Td.b.auto_doc $Td.b.us $Td.b.using -padx 5 -side left" );
		cmd( "bind $T <Control-w> \"$Td.b.eq invoke\"; bind $T <Control-W> \"$Td.b.eq invoke\"" );
		cmd( "bind $T <Control-e> \"$Td.b.using invoke\"; bind $T <Control-E> \"$Td.b.using invoke\"" );
	}

	if ( cv->param == 1 || cv->num_lag > 0 )
	{
		cmd( "frame $Td.i -bd 2 -relief groove" );
		cmd( "label $Td.i.int -text \"Comments on initial values\"" );
		cmd( "scrollbar $Td.i.yscroll -command \"$Td.i.text yview\"" );
		cmd( "text $Td.i.text -undo 1 -wrap word -width 60 -height 3 -relief sunken -yscrollcommand \"$Td.i.yscroll set\" -font \"$font_small\"" );
		cmd( "pack $Td.i.yscroll -side right -fill y" );
		if (cur_descr->init != NULL )
		{
			for ( i = 0; cur_descr->init[ i ] != '\0'; ++i )
				if ( cur_descr->init[ i ] != '[' && cur_descr->init[ i ] != ']' && cur_descr->init[ i ] != '{' && cur_descr->init[ i ] != '}' && cur_descr->init[ i ] != '\"' && cur_descr->text[ i ] != '\\')
					cmd( "$Td.i.text insert end \"%c\"", cur_descr->init[ i ] );
				else
					cmd( "$Td.i.text insert end \"\\%c\"", cur_descr->init[ i ] );
	  
			cmd( "$Td.i.text delete \"end - 1 char\"" );
		}
		cmd( "pack $Td.i.int $Td.i.text -anchor w -expand yes -fill both" );
	  
		cmd( "frame $Td.b2" );
		cmd( "button $Td.b2.setall -width [ expr $butWid + 3 ] -text \"Initial Values\" -command {set done 11} -underline 1" );
		cmd( "button $Td.b2.sens -width [ expr $butWid + 3 ] -text \"Sensitivity\" -command {set done 12} -underline 5" );
		cmd( "pack $Td.b2.setall $Td.b2.sens -padx 10 -side left" );
		
		cmd( "pack $Td.opt $Td.f $Td.b $Td.i $Td.b2 -pady 5" );
	  
		cmd( "bind $T <Control-n> \"$Td.b2.setall invoke\"; bind $T <Control-N> \"$Td.b2.setall invoke\"" );
		cmd( "bind $T <Control-t> \"$Td.b2.sens invoke\"; bind $T <Control-T> \"$Td.b2.sens invoke\"" );

	}
	else
		cmd( "pack $Td.opt $Td.f $Td.b -pady 5" );

	cmd( "pack $Td -pady 5" );

	cmd( "okhelpcancel $T b { set done 1 } { LsdHelp menumodel.html#variables } { set done 2 }" );

	cmd( "bind $T <Control-r> \"$T.b0.prop invoke\"; bind $T <Control-R> \"$T.b0.prop invoke\"" );
	cmd( "bind $T <Control-m> \"$T.b0.mov invoke\"; bind $T <Control-M> \"$T.b0.mov invoke\"" );
	cmd( "bind $T <Control-l> \"$T.b0.del invoke\"; bind $T <Control-L> \"$T.b0.del invoke\"" );
	cmd( "bind $T <Control-s> \"$T.b1.sav.n invoke\"; bind $T <Control-S> \"$T.b1.sav.n invoke\"" );
	cmd( "bind $T <Control-f> \"$T.b1.sav.i invoke\"; bind $T <Control-F> \"$T.b1.sav.i invoke\"" );
	cmd( "bind $T <Control-p> \"$T.b1.plt invoke\"; bind $T <Control-P> \"$T.b1.plt invoke\"" );
	cmd( "bind $T <Control-o> \"$Td.opt.obs invoke\"; bind $T <Control-O> \"$Td.opt.obs invoke\"" );
	cmd( "bind $T <Control-a> \"$Td.b.auto_doc invoke\"; bind $T <Control-A> \"$Td.b.auto_doc invoke\"" );
	cmd( "bind $T <Control-u> \"$Td.b.us invoke\"; bind $T <Control-U> \"$Td.b.us invoke\"" );

	cmd( "showtop $T topleftW" );

	cycle_var:

	done = 0;
	while ( done == 0 )
		Tcl_DoOneEvent( 0 );

	*choice = 1;	// point .chgelem window as parent for the following windows
	if ( done == 3 )
		show_eq( lab_old, choice );
	if ( done == 4 )
		scan_used_lab( lab_old, choice );
	if ( done == 7 )
		scan_using_lab( lab_old, choice );
	*choice = 0;

	if ( done == 9 ) 
	{
		cmd( "set text_description \"[.chgelem.desc.f.text get 1.0 end]\"" );
		change_descr_text( lab_old );
	  
		auto_document( choice, lab_old, "ALL", true );
		cmd( ".chgelem.desc.f.text delete 1.0 end" );

		for ( i = 0; cur_descr->text[ i ] != '\0'; ++i )
			if ( cur_descr->text[ i ] != '[' && cur_descr->text[ i ] != ']' && cur_descr->text[ i ] != '{' && cur_descr->text[ i ] != '}' && cur_descr->text[ i ] != '\"' && cur_descr->text[ i ] != '\\')
				cmd( ".chgelem.desc.f.text insert end \"%c\"", cur_descr->text[ i ] );
			else
				cmd( ".chgelem.desc.f.text insert end \"\\%c\"", cur_descr->text[ i ] );
		  
		cmd( ".chgelem.desc.f.text delete \"end - 1 char\"" );
		unsaved_change( true );		// signal unsaved change
	}

	if (done == 7 || done == 4 || done == 3 || done == 9)
	  goto cycle_var;

	if ( done == 2 || done == 8 )	// esc/cancel
	{
		redrawRoot = false;			// no browser redraw
		goto here_endelem;
	}
	else
	{
	   cmd( "set choice $observe" );
	   *choice == 1 ? observe = 'y' : observe = 'n';
	   cmd( "set choice $initial" );
	   *choice == 1 ? initial = 'y' : initial = 'n';
	   cur_descr->initial = initial;
	   cur_descr->observe = observe;
	   
	   for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
	   {
		   cv = cur->search_var( NULL, lab_old );
		   cv->save=save;
		   cv->savei=savei;
		   cv->debug=num==1?'d':'n';
		   cv->plot=plot;
		   cv->parallel = parallel;
		   cv->observe = ( observe == 'y' ) ? true : false;
	   }
		  
	   cmd( "set text_description \"[.chgelem.desc.f.text get 1.0 end]\"" );
	   change_descr_text( lab_old );
	   if ( cv->param == 1 || cv->num_lag > 0 )
	   {
		 cmd( "set text_description \"[.chgelem.desc.i.text get 1.0 end]\"" );
		 change_init_text( lab_old );
	   }
	  
	   unsaved_change( true );		// signal unsaved change

	   if ( save == 1 || savei == 1 )
	   {
		  for ( cur = r; cur != NULL; cur = cur->up )
			if ( cur->to_compute == 0 )
			{
			   cmd( "tk_messageBox -parent .chgelem -type ok -title Warning -icon warning -message \"Cannot save item\" -detail \"Item\n'%s'\nset to be saved but it will not be registered for the Analysis of Results, since object\n'%s'\nis not set to be computed.\"", lab_old, cur->label );
			}
	   }
	}

	if ( done != 8 )
	  *choice = 0;
	else
	  *choice = 7;  

	here_endelem:

	cmd( "destroytop .chgelem" );
	Tcl_UnlinkVar( inter, "done" );
	Tcl_UnlinkVar( inter, "save" );
	Tcl_UnlinkVar( inter, "savei" );
	Tcl_UnlinkVar( inter, "debug" );
	Tcl_UnlinkVar( inter, "plot" );
	Tcl_UnlinkVar( inter, "parallel" );
	cmd( "unset done" );

	// options to be handled in a second run of the operate function
	switch ( done )
	{
		case 5:
			*choice = 75;			// open properties box for $vname
			break;
		case 10:
			*choice = 76;			// delete element in $vname
			break;
		case 11:
			*choice = 77;			// change initial values for $vname
			break;
		case 12:
			*choice = 78;			// change sensitivity values for $vname
			break;
		case 13:
			*choice = 79;			// move element in $vname
			break;
		default:
			*choice = 0;
			break;
	}

	if ( *choice != 0 )
	{
		redrawRoot = false;			// no browser redraw yet
		return r;					// execute command
	} 

break;


// Edit variable/parameter (defined by tcl $vname) properties
case 75:
// Delete variable/parameter (defined by tcl $vname)
case 76:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );	// get var/par name in lab_old

	if ( *choice == 76 )
	{
		delVar = renVar = true;
		cmd( "set answer [ tk_messageBox -parent . -title Confirmation -icon question -type yesno -default yes -message \"Delete element?\" -detail \"Press 'Yes' to confirm deleting '$vname'\" ]; switch $answer { yes { set choice 1 } no { set choice 2 } }" );
		if ( *choice == 1 )
			cmd( "set vname \"\"; set nature 3; set numlag 0" );	// configure to delete
		else
			goto here_endprop;
	}
	else
	{
		delVar = renVar = false;

		cv = r->search_var( NULL, lab_old );
		cmd( "set nature %d", cv->param );
		cmd( "if { $nature == 0 } { set numlag %d } { set numlag 0 }", cv->num_lag );

		cmd( "set T .prop" );
		cmd( "newtop $T \"Properties\" { set choice 2 }" );

		cmd( "frame $T.h" );
		cmd( "label $T.h.l1 -text \"Element:\"" );
		cmd( "label $T.h.l2 -text \"%s\" -fg red", cv->label );
		cmd( "pack $T.h.l1 $T.h.l2 -side left -padx 2" );
		
		cmd( "frame $T.n" );
		cmd( "label $T.n.var -text \"Name\"" );
		cmd( "entry $T.n.e -width 20 -textvariable vname -justify center" );
		cmd( "label $T.n.sp -width 2" );
		cmd( "label $T.n.l -text \"Lags\"" );
		cmd( "entry $T.n.lag -width 2 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set numlag %%P; return 1 } { %%W delete 0 end; %%W insert 0 $numlag; return 0 } } -invcmd { bell } -justify center" );
		cmd( "$T.n.lag insert 0 $numlag" ); 
		cmd( "if { $nature != 0 } { $T.n.lag configure -state disabled }" );
		cmd( "pack $T.n.var $T.n.e $T.n.sp $T.n.l $T.n.lag -side left -padx 2" );

		cmd( "frame $T.v" );
		cmd( "label $T.v.l -text \"Type\"" );
		
		cmd( "frame $T.v.o -bd 2 -relief groove" );
		cmd( "radiobutton $T.v.o.var -text Variable -variable nature -value 0 -underline 0 -command { $T.n.lag configure -state normal }" );
		cmd( "radiobutton $T.v.o.par -text Parameter -variable nature -value 1 -underline 0 -command { $T.n.lag configure -state disabled }" );
		cmd( "radiobutton $T.v.o.fun -text Function -variable nature -value 2 -underline 0 -command { $T.n.lag configure -state disabled }" );
		cmd( "pack  $T.v.o.var $T.v.o.par $T.v.o.fun -anchor w" );
		
		cmd( "pack $T.v.l $T.v.o" );

		cmd( "pack $T.h $T.n $T.v -padx 5 -pady 5" );
		
		cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp menumodel.html#change_nature } { set choice 2 }" );
		
		cmd( "bind $T.n.e <KeyPress-Return> { set choice 1 }" );
		cmd( "bind $T <Control-v> \"$T.v.o.var invoke\"; bind $T <Control-V> \"$T.v.o.var invoke\"" );
		cmd( "bind $T <Control-p> \"$T.v.o.var invoke\"; bind $T <Control-P> \"$T.v.o.var invoke\"" );
		cmd( "bind $T <Control-f> \"$T.v.o.var invoke\"; bind $T <Control-F> \"$T.v.o.var invoke\"" );
		
		cmd( "showtop $T" );
		cmd( "focus $T.n.e" );
		cmd( "$T.n.e selection range 0 end" );

		*choice = 0;
	}

	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "if [ winfo exists .prop ] { if { $nature == 0 } { set numlag [ .prop.n.lag get ] }; destroytop .prop }" );

	if ( *choice == 2 )
		goto here_endprop;

	cmd( "set choice $nature" );
	nature = *choice;

	cmd( "set choice $numlag" );
	numlag = *choice;

	if ( ! delVar && ( nature != cv->param || numlag != cv->num_lag ) )
	{
		if ( nature == 0 )
			change_descr_lab( lab_old, "", "Variable", "", "" );
		if ( nature == 1 )
			change_descr_lab( lab_old, "", "Parameter", "", "" );
		if ( nature == 2 )
			change_descr_lab( lab_old, "", "Function", "", "" );

		for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
		{ 
			cv = cur->search_var( NULL, lab_old );
			cv->num_lag = numlag;
			delete [ ] cv->val;
			cv->val=new double[numlag+1];
			for ( i = 0; i < numlag + 1; ++i )
				cv->val[ i ] = 0;
			cv->param = nature;
			if ( cv->param == 1 || cv->num_lag > 0 )
				cv->data_loaded = '-';
			if ( cv->param != 0 )
				cv->parallel = false;
		}
	}

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 != NULL && strcmp( lab1, "" ) )
	{
		sscanf( lab1, "%99s", lab );			// new name in lab (empty if delete)
		if ( strcmp( lab, lab_old ) )			// check new name if different
			renVar = true;
	}
	else
		if ( delVar )
			strcpy( lab, "" );
		else
			goto here_endprop;
		
	if ( renVar )
	{
		if ( ! delVar )
		{
			for ( cur = r; cur->up != NULL; cur = cur->up );
			*choice = check_label( lab, cur );

			if ( *choice == 1 )
			{
				cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"The name already exists in the model\" -detail \"Choose a different name and try again.\"" );
				goto here_endprop;
			}
			if ( *choice == 2 )
			{
				cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Invalid characters in name\" -detail \"Names must begin with a letter (English alphabet) or underscore ('_') and may contain letters, numbers or '_' but no spaces. Choose a different label and try again.\"" );
				goto here_endprop;
			}
		}
		
		// remove from find list
		cmd( "if [ info exists ModElem ] { set pos [ lsearch -exact $ModElem \"%s\" ]; if { $pos >= 0 } { set ModElem [ lreplace $ModElem $pos $pos ] } }", lab_old  );

		if ( ! delVar )
		{
			change_descr_lab( lab_old, lab, "", "", "" );
			cmd( "if [ info exists ModElem ] { lappend ModElem %s }", lab );		// add to find list
		}
		
		for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
		{
			if ( ! delVar )
			{
				cur->chg_var_lab( lab_old, lab );
				cv = cur->search_var( NULL, lab );
			}
			else
			{
				if ( ! strcmp( lab_old, cur->v->label ) )
				{
					app = cur->v->next;
					delete [ ] cur->v->label;
					delete [ ] cur->v->val;
					delete cur->v;
					cur->v = app;
				}
				else
				{
					for (cur_v = cur->v; cur_v->next != NULL; cur_v = cur_v->next )
					{
						if ( ! strcmp( lab_old,cur_v->next->label ) )
						{
							app = cur_v->next->next;
							delete [ ] cur_v->next->label;
							delete [ ] cur_v->next->val;
							delete cur_v->next;
							cur_v->next = app;
							break;
						}
					}
				}
			}
		}
	}

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

	here_endprop:

break;


// Move variable/parameter (defined by tcl $vname)
case 79:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );	// get var/par name in lab_old

	cmd( "set TT .objs" );
	cmd( "newtop $TT \"Move\" { set choice 2 }" );

	cmd( "frame $TT.l" );
	cmd( "label $TT.l.l -text \"Element:\"" );
	cmd( "label $TT.l.n -fg red -text \"%s\"", lab_old );
	cmd( "pack $TT.l.l $TT.l.n -side left -padx 2" );

	cmd( "frame $TT.v" );
	cmd( "label $TT.v.l -text \"Move to\"" );

	cmd( "frame $TT.v.t" );
	cmd( "scrollbar $TT.v.t.v_scroll -command \"$TT.v.t.lb yview\"" );
	cmd( "listbox $TT.v.t.lb -width 25 -selectmode single -yscroll \"$TT.v.t.v_scroll set\"" );
	cmd( "pack $TT.v.t.lb $TT.v.t.v_scroll -side left -fill y" );
	cmd( "mouse_wheel $TT.v.t.lb" );
	insert_object( "$TT.v.t.lb", root );
	cmd( "pack $TT.v.l $TT.v.t" );

	cmd( "pack $TT.l $TT.v -padx 5 -pady 5" );

	cmd( "okcancel $TT b { set choice 1 } { set choice 2 }" );	// insert ok button
	cmd( "bind $TT.v.t.lb <Double-1> { set choice 1 }" );

	cmd( "showtop $TT" );
	cmd( "$TT.v.t.lb selection set 0" );
	cmd( "focus $TT.v.t.lb" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set movelabel [ .objs.v.t.lb get [ .objs.v.t.lb curselection ] ]" );
	cmd( "destroytop .objs" );

	if ( *choice == 2 )
		break;

	lab1 = ( char * ) Tcl_GetVar( inter, "movelabel", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, r->label ) )		// same object?
		break;
		
	cv = r->search_var( NULL, lab_old );
	//if ( cv->param == 1 || cv->num_lag > 0 ) 		// force initial value request
	//	cv->data_loaded = '-';
	for ( cur = root->search( lab1 ); cur != NULL; cur = cur->hyper_next( cur->label ) )
		cur->add_var_from_example( cv );

	for ( cur = r; cur != NULL; cur = cur->hyper_next( cur->label ) )
	{
		if ( ! strcmp( lab_old, cur->v->label ) )
		{
			app = cur->v->next;
			delete [ ] cur->v->label;
			delete [ ] cur->v->val;
			delete cur->v;
			cur->v = app;
		}
		else
		{
			for ( cur_v = cur->v; cur_v->next != NULL; cur_v = cur_v->next )
			{
				if ( ! strcmp( lab_old, cur_v->next->label) )
				{
					app = cur_v->next->next;
					delete [ ] cur_v->next->label;
					delete [ ] cur_v->next->val;
					delete cur_v->next;
					cur_v->next = app;
					break;
				}
			}
		}
	}

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

break;


// Change variable/parameter (defined by tcl $vname) initial values
case 77:
// Change variable/parameter (defined by tcl $vname) sensitivity values
case 78:

	done = ( *choice == 77 ) ? 1 : 2;

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );		// get var/par name in lab_old
	cv = r->search_var( NULL, lab_old );	// get var/par pointer
	if ( cv == NULL )
		break;

	// do lag selection, if necessary, for initialization/sensitivity data entry
	lag = 0;							// lag option for the next cases (first lag)
	if ( ( cv->param == 0 || cv->param == 2 ) && cv->num_lag > 1 )
	{									// more than one lag to choose?
		cmd( "set lag \"1\"" );
		
		// confirm which lag to use
		cmd( "set T .lag" );
		cmd( "newtop $T \"Lag Selection\" { set choice 0 }" );

		cmd( "frame $T.i" );
		cmd( "label $T.i.l -text \"Use lag\"" );
		cmd( "entry $T.i.e -width 2 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set lag %%P; return 1 } { %%W delete 0 end; %%W insert 0 $lag; return 0 } } -invcmd { bell } -justify center" );
		cmd( "$T.i.e insert 0 $lag" ); 
		cmd( "pack $T.i.l $T.i.e -side left -padx 2" );
		
		cmd( "frame $T.o" );
		cmd( "label $T.o.l1 -text \"( valid values:\"" );
		cmd( "label $T.o.w1 -text 1 -fg red" );
		cmd( "label $T.o.l2 -text to" );
		cmd( "label $T.o.w2 -text %d -fg red", cv->num_lag );
		cmd( "label $T.o.l3 -text \")\"" );
		cmd( "pack $T.o.l1 $T.o.w1 $T.o.l2 $T.o.w2 $T.o.l3 -side left -padx 2" );
		
		cmd( "pack $T.i $T.o -padx 5 -pady 5" );
		
		cmd( "okcancel $T b { set choice $lag } { set choice 0 }" );
		cmd( "bind $T <KeyPress-Return> { set choice $lag }" );
		
		cmd( "showtop $T" );
		cmd( "$T.i.e selection range 0 end" );
		cmd( "focus $T.i.e" );
		
		*choice = -1;
		while ( *choice == -1 )		// wait for user action
			Tcl_DoOneEvent( 0 );
			
		cmd( "set lag [ .lag.i.e get ]" ); 
		cmd( "destroytop .lag" );
		
		if ( *choice == 0 )
			break;
		
		cmd( "set choice $lag" ); 
		lag = abs( *choice ) - 1;	// try to extract chosed lag
		
		// abort if necessary
		if ( lag < 0 || lag > ( cv->num_lag - 1 ) )
		{
			cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Invalid lag selected\" -detail \"Select a valid lag value for the variable or change the number of lagged values for this variable.\"" );
			break;
		}
	}

	// initialize
	if ( done == 1 )
	{
		*choice = 0;		// set top window as parent
		set_all( choice, r, cv->label, lag );
	}
	// edit sensitivity analysis data
	else
	{
		*choice = 0;
		bool exist = false;
		sense *cs, *ps = NULL;

		if ( rsense == NULL )		// no sensitivity analysis structure yet?
			rsense = cs = new sense;
		else
		{
			// check if sensitivity data for the variable already exists 
			for ( cs = rsense, ps = NULL; cs != NULL; ps = cs, cs = cs->next )
				if ( ! strcmp( cs->label, cv->label ) && 
					 ( cs->param == 1 || cs->lag == lag ) )
				{
					exist = true;
					break;	// get out of the inner for loop
				}
				
			if ( ! exist )	// if new variable, append at the end of the list
			{
				for ( cs = rsense; cs->next != NULL; cs = cs->next );	// pick last
				cs->next = new sense;	// create new variable
				ps = cs;	// keep previous sensitivity variable
				cs = cs->next;
			}
		}
			
		if ( ! exist )		// do only for new variables in the list
		{
			cs->label = new char[ strlen( cv->label ) + 1 ];
			strcpy( cs->label, cv->label );
			cs->next = NULL;
			cs->nvalues = 0;
			cs->v = NULL;
			cs->entryOk = false;	// no valid data yet
		}
		else
			cs->entryOk = true;		// valid data already there

		// save type and specific lag in this case
		cs->param = cv->param;
		cs->lag = lag;
		
		dataentry_sensitivity( choice, cs, 0 );
		
		if ( ! cs->entryOk )		// data entry failed?
		{
			if ( rsense == cs )		// is it the first variable?
				rsense = cs->next;	// update list root
			else
				ps->next = cs->next;// remove from sensitivity list		
			delete [ ] cs->label;	// garbage collection
			delete cs;
		}
		else
			unsavedSense = true;	// signal unsaved change
	}

break;


// Exit the browser and run the simulation
case 1:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to run the simulation.\"" );
		break;
	}

	// warn about no variable/parameter being saved
	for ( n = r; n->up != NULL; n = n->up );
	series_saved = 0;
	count_save( n, &series_saved );
	if ( series_saved == 0 )
	{
		cmd( "set answer [ tk_messageBox -parent . -type okcancel -default ok -icon warning -title Warning -message \"No variable or parameter marked to be saved\" -detail \"If you proceed, there will be no data to be analyzed after the simulation is run. If this is not the intended behavior, please mark the variables and parameters to be saved before running the simulation.\" ]; switch -- $answer { ok { set choice 1 } cancel { set choice 2 } } " );
		if ( *choice == 2 )
		{
			*choice = 0;
			break;
		}
	}

	// warn missing debugger
	if ( search_parallel( root ) && ( when_debug > 0 || stack_info > 0 || prof_aggr_time ) )
	{
		cmd( "set answer [ tk_messageBox -parent . -title Warning -icon warning -type okcancel -default ok -message \"Debugger/profiler not available\" -detail \"Debugging in parallel mode is not supported, including stack profiling.\n\nPress 'OK' to proceed and disable parallel processing settings or 'Cancel' to return to LSD Browser.\" ]; switch $answer { ok { set choice 1 } cancel { set choice 2 } }" );
		if ( *choice == 2 )
		{
			*choice = 0;
			break;
		}
		parallel_disable = true;
	}

	Tcl_LinkVar( inter, "no_res", ( char * ) & no_res, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "add_to_tot", ( char * ) & add_to_tot, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "docsv", ( char * ) & docsv, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "dozip", ( char * ) & dozip, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "overwConf", ( char * ) & overwConf, TCL_LINK_BOOLEAN );

	// Only ask to overwrite configuration if there are changes
	overwConf = unsaved_change( ) ? true : false;

	// avoid showing dialog if configuration already saved and nothing to save to disk
	if ( ! overwConf && sim_num == 1 )
	{
		*choice = 1;
		goto run;
	}

	// save the current object & cursor position for quick reload
	strcpy( lastObj, r->label );
	cmd( "if { ! [ string equal [ .l.s.c.son_name curselection ] \"\" ] } { set lastList 2 } { set lastList 1 }" );
	cmd( "if { $lastList == 1 } { set lastItem [ .l.v.c.var_name curselection ] } { set lastItem [ .l.s.c.son_name curselection ] }" );
	cmd( "if { $lastItem == \"\" } { set lastItem 0 }" );

	cmd( "set firstFile \"%s_%d\"", simul_name, seed );
	cmd( "set lastFile \"%s_%d\"", simul_name, seed + sim_num - 1 );
	cmd( "set totFile \"%s\"", simul_name );
	cmd( "set resExt %s", docsv ? "csv" : "res" );
	cmd( "set totExt %s", docsv ? "csv" : "tot" );
	cmd( "set zipExt \"%s\"", dozip ? ".gz" : "" );

	cmd( "set T .run" );
	cmd( "newtop $T \"Run Simulation\" { set choice 2 }" );

	cmd( "frame $T.f1" );
	cmd( "label $T.f1.l -text \"Model configuration\"" );
	cmd( "label $T.f1.w -text \"%s\" -fg red", simul_name );
	cmd( "pack $T.f1.l $T.f1.w" );

	cmd( "frame $T.f2" );

	cmd( "frame $T.f2.t" );
	cmd( "label $T.f2.t.l -text \"Time steps:\"" );
	cmd( "label $T.f2.t.w -text \"%d\" -fg red", max_step );
	cmd( "pack $T.f2.t.l $T.f2.t.w -side left -padx 2" );
		
	if ( sim_num > 1 )
	{
		cmd( "frame $T.f2.n" );
		cmd( "label $T.f2.n.l -text \"Number of simulations:\"" );
		cmd( "label $T.f2.n.w -text \"%d\" -fg red", sim_num );
		cmd( "pack $T.f2.n.l $T.f2.n.w -side left -padx 2" );
		
		cmd( "pack $T.f2.t $T.f2.n" );

		cmd( "frame $T.f3" );
		cmd( "label $T.f3.l -text \"Results files\"" );
		
		cmd( "frame $T.f3.w" );
		
		cmd( "frame $T.f3.w.l1" );
		cmd( "label $T.f3.w.l1.l -text \"from:\"" );
		cmd( "label $T.f3.w.l1.w -fg red -text \"$firstFile.$resExt$zipExt\"" );
		cmd( "pack $T.f3.w.l1.l $T.f3.w.l1.w -side left -padx 2" );
		
		cmd( "frame $T.f3.w.l2" );
		cmd( "label $T.f3.w.l2.l -text \"to:\"" );
		cmd( "label $T.f3.w.l2.w -fg red -text \"$lastFile.$resExt$zipExt\"" );
		cmd( "pack $T.f3.w.l2.l $T.f3.w.l2.w -side left -padx 2" );
		
		cmd( "pack $T.f3.w.l1 $T.f3.w.l2" );

		cmd( "pack $T.f3.l $T.f3.w" );

		cmd( "frame $T.f4" );
		cmd( "label $T.f4.l1 -text \"Totals file (last steps)\"" );
		cmd( "label $T.f4.l2 -fg red -text \"$totFile.$totExt$zipExt\"" );
		
		cmd( "set choice [ file exists \"%s%s$totFile.$totExt$zipExt\" ]", path, strlen( path ) > 0 ? "/" : "" );
		cmd( "label $T.f4.l3 -text \"%s\"", *choice ? "(WARNING: totals file already exists)" : "" );
		cmd( "pack $T.f4.l1 $T.f4.l2 $T.f4.l3" );

		add_to_tot = ( *choice ) ? add_to_tot : false;

		cmd( "frame $T.f5" );
		cmd( "checkbutton $T.f5.a -text \"Append to existing totals file\" -variable add_to_tot -state %s", *choice ? "normal" : "disabled" );
		cmd( "checkbutton $T.f5.b -text \"Skip generating results files\" -variable no_res" );
		cmd( "checkbutton $T.f5.c -text \"Generate zipped files\" -variable dozip -command { if $dozip { set zipExt \".gz\" } { set zipExt \"\" }; $T.f3.w.l1.w configure -text \"$firstFile.$resExt$zipExt\"; $T.f3.w.l2.w configure -text \"$lastFile.$resExt$zipExt\"; $T.f4.l2 configure -text \"$totFile.$totExt$zipExt\"; if [ file exists \"%s%s$totFile.$totExt$zipExt\" ] { $T.f4.l3 configure -text \"(WARNING: totals file already exists)\"; $T.f5.a configure -state normal } { $T.f4.l3 configure -text \"\"; $T.f5.a configure -state disabled } }", path, strlen( path ) > 0 ? "/" : "" );
		cmd( "checkbutton $T.f5.d -text \"Comma-separated text format (.csv)\" -variable docsv -command { if $docsv { set resExt csv; set totExt csv } { set resExt res; set totExt tot }; $T.f3.w.l1.w configure -text \"$firstFile.$resExt$zipExt\"; $T.f3.w.l2.w configure -text \"$lastFile.$resExt$zipExt\"; $T.f4.l2 configure -text \"$totFile.$totExt$zipExt\"; if [ file exists \"%s%s$totFile.$totExt$zipExt\" ] { $T.f4.l3 configure -text \"(WARNING: totals file already exists)\"; $T.f5.a configure -state normal } { $T.f4.l3 configure -text \"\"; $T.f5.a configure -state disabled } }", path, strlen( path ) > 0 ? "/" : "" );
		cmd( "checkbutton $T.f5.e -text \"Update configuration file\" -variable overwConf" );
		cmd( "pack $T.f5.a $T.f5.b $T.f5.c $T.f5.d %s -anchor w", overwConf ? "$T.f5.e" : "" );
		
		cmd( "pack $T.f1 $T.f2 $T.f3 $T.f3 $T.f4 $T.f5 -padx 5 -pady 5" );
	}
	else
	{
		*choice = 0;
		cmd( "pack $T.f2.t" );
		
		cmd( "label $T.f3 -text \"(results will be saved in memory only)\"" );
		
		cmd( "checkbutton $T.f6 -text \"Update configuration file\" -variable overwConf" );
		
		cmd( "pack $T.f1 $T.f2 $T.f3 %s -padx 5 -pady 5", overwConf ? "$T.f6" : "" );
	}

	cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp menurun.html#run } { set choice 2 }" );

	cmd( "showtop $T" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .run" );

	run:

	Tcl_UnlinkVar( inter, "no_res" );
	Tcl_UnlinkVar( inter, "add_to_tot" );
	Tcl_UnlinkVar( inter, "docsv" );
	Tcl_UnlinkVar( inter, "dozip" );
	Tcl_UnlinkVar( inter, "overwConf" );

	if ( *choice == 2 )
	{
		*choice = 0;
		break;
	}

	for ( n = r; n->up != NULL; n = n->up );
	blueprint->empty( );			    // update blueprint to consider last changes
	set_blueprint( blueprint, n );

	if ( overwConf )					// save if needed
		if ( ! save_configuration( r ) )
		{
			cmd( "set answer [ tk_messageBox -parent . -type okcancel -default cancel -icon warning -title Warning -message \"File '%s.lsd' cannot be saved\" -detail \"Check if the drive or the file is set READ-ONLY. Press 'OK' to run the simulation without saving the initialization file.\" ]; switch -- $answer { ok { set choice 1 } cancel { set choice 2 } } ", simul_name );
			if ( *choice == 2 )
			{
				*choice = 0;
				break;
			}
		}

	*choice = 1; 
	return n;


// Exit LSD
case 11:

	if ( discard_change( ) )	// unsaved configuration changes ?
		myexit( 0 );
	
break;


// Load a model
case 17:
case 38: //quick reload

	reload = ( *choice == 38 ) ? true : false;

	if ( reload )
		save_pos( r );

	if ( struct_loaded )
	{ 
		if ( ! discard_change( ) )		// unsaved configuration?
			break;

		cmd( "set a [split [winfo children .] ]" );  // remove old runtime plots
		cmd( "foreach i $a {if [string match .plt* $i] {destroytop $i}}" );
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );	// remove lattice
		cmd( "if { [ file exists temp.html ] } { file delete temp.html }" );

		empty_sensitivity(rsense); 	// discard sensitivity analysis data
		rsense = NULL;
		unsavedSense = false;		// nothing to save
		findexSens = 0;
		nodesSerial = 0;			// network node serial number global counter
		cmd( "unset -nocomplain ModElem" );
	}

	struct_loaded = false;
	actual_steps = 0;				//Flag that no simulation has been run
	unsavedData = false;			// no unsaved simulation results
	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	if ( ! reload )
	{
		strcpy( lastObj, "" );		// disable last object for quick reload
		cmd( "set res \"%s\"", simul_name );

		cmd( "set bah [ tk_getOpenFile -parent . -title \"Open Configuration File\"  -defaultextension \".lsd\" -initialdir \"$path\" -initialfile \"$res.lsd\" -filetypes { { {LSD model file } {.lsd} } } ]" );
		*choice = 0;
		cmd( "if { [ string length $bah ] > 0 && ! [ fn_spaces \"$bah\" . ] } { set res $bah; set path [ file dirname $res ]; set res [ file tail $res ]; set last [ expr [ string last .lsd $res ] - 1 ]; set res [ string range $res 0 $last ] } { set choice 2 }" );
		if ( *choice == 2 )
			break;

		lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );
		if ( strlen( lab1 ) == 0 )
			break;
		delete [ ] simul_name;
		simul_name = new char[ strlen( lab1 ) + 1 ];
		strcpy( simul_name, lab1 );

		lab1 = ( char * ) Tcl_GetVar( inter, "path", 0 );
		delete [ ] path;
		path = new char[ strlen( lab1 ) + 1 ];
		strcpy( path, lab1 );
		if ( strlen( lab1 ) > 0 )
			cmd( "cd $path" );
	}

	switch ( load_configuration( r, false ) )
	{
		case 1:							// file/path not found
			if ( strlen( path ) > 0 )
				cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"File not found\" -detail \"File for model '%s' not found in directory '%s'.\"", simul_name, path );
			else
				cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"File not found\" -detail \"File for model '%s' not found in current directory\"", simul_name  );
			*choice = 20;
			break;
			
		case 2:
		case 3:
			cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid or damaged file\" -detail \"Please check if a proper file was selected.\"" );
			*choice = 20;
			break;
			
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:							// problem from MODELREPORT section
		case 9:							// problem from DESCRIPTION section
			autofill_descr( r );
			
		case 10:						// problem from DOCUOBSERVE section
		case 11:
		case 12:						// problem from DOCUINITIAL section
		case 13:
			cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid or damaged file\" -detail \"Please check if a proper file was selected and if the loaded configuration is correct.\"" );

		default:						// load ok
			unsaved_change( false );	// no changes to save
			iniShowOnce = false;		// show warning on # of columns in .ini
			redrawRoot = true;			// force browser redraw
			if ( ! reload )
				cmd( "set cur 0" ); // point for first var in listbox
			*choice = 0;
	}

	// restore pointed object and variable
	n = restore_pos( r );

	if ( n != r )
		return n;

break;

	
// Save a model
case 73:
case 18:

	saveAs = ( *choice == 73 ) ? true : false;

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration to save\" -detail \"Create a configuration before saving.\"" );
		break;
	}

	Tcl_LinkVar( inter, "done", ( char * ) &done, TCL_LINK_INT );

	if ( actual_steps > 0 )
	{ 
		cmd( "set answer [ tk_messageBox -parent . -type okcancel -default cancel -icon warning -title Warning -message \"Configuration is the final state of a simulation run\" -detail \"Press 'OK' to save it anyway or 'Cancel' to abort saving.\" ]; switch -- $answer { ok { set done 1 } cancel { set done 2 } } " );

		if ( done == 2 )
		{
			Tcl_UnlinkVar( inter, "done" );
			cmd( "unset done" );
			break;
		}
		saveAs = true;	// require file name to save
	 }

	done = 0;
	cmd( "set res \"%s\"", simul_name );
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	if ( saveAs )			// only asks file name if instructed to or necessary
	{
		cmd( "set bah [ tk_getSaveFile -parent . -title \"Save Configuration File\" -defaultextension \".lsd\" -initialfile $res -initialdir \"$path\" -filetypes { { {LSD model files} {.lsd} } } ]" );

		cmd( "if { [ string length $bah ] > 0 } { set res $bah; set path [ file dirname $res ]; set res [ file tail $res ]; set last [ expr [ string last .lsd $res ] - 1 ];if { $last > 0 } { set res [ string range $res 0 $last ] } } { set done 2 }" );
		if ( done == 2 )
			goto save_end;

		lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );

		if ( strlen( lab1 ) == 0 )
			break;
		delete [ ] simul_name;
		simul_name = new char[ strlen( lab1 ) + 1 ];
		strcpy( simul_name, lab1 );
		lab1 = ( char * ) Tcl_GetVar( inter, "path", 0 );
		delete [ ] path;
		path = new char[ strlen( lab1 ) + 1 ];
		strcpy( path, lab1 );
		if ( strlen( lab1 ) > 0 )
			cmd( "cd $path" );
	}

	if ( ! save_configuration( r ) )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"File '%s.lsd' cannot be saved\" -detail \"The model is NOT saved! Check if the drive or the file is set READ-ONLY, change file name or select a drive with write permission and try again.\"", simul_name  );
	}

	save_end:
	Tcl_UnlinkVar( inter, "done" );
	cmd( "unset done" );
	
break;


// Edit Objects' numbers
case 19:

	for ( n = r; n->up != NULL; n = n->up );

	*choice = 0;
	strcpy( lab, r->label );
	set_obj_number( n, choice );
	cmd( "destroytop .ini" );
	r = n->search( lab );

	unsaved_change( true );			// signal unsaved change

break;


// Edit initial values for Objects currently selected or pointed by the browser (defined by tcl $vname)
case 21:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	for ( n = r; n->up != NULL; n = n->up );

	edit_data(n, choice, r->label);

	unsaved_change( true );			// signal unsaved change
	cmd( "destroytop .ini" );

	if ( cur2 != NULL )				// restore original current object
		r = cur2;

break;


// Unload the model
case 20:

	if ( ! discard_change( ) )	// check for unsaved configuration changes
		break;

	cmd( "destroytop .str" );
	cmd( "set a [split [winfo children .] ]" );
	cmd( "foreach i $a {if [string match .plt* $i] {destroytop $i}}" );
	cmd( "destroytop .lat" );	// remove lattice
	cmd( "if { [ file exists temp.html ] } { file delete temp.html }" );

	for ( n = r; n->up != NULL; n = n->up );
	n->empty( );
	empty_cemetery( );
	n->label = new char[ strlen( "Root" ) + 1 ];
	strcpy( n->label, "Root" );
	r = n;
	strcpy( lastObj, "" );			// disable last object for quick reload
	actual_steps = 0;
	unsavedData = false;			// no unsaved simulation results
	empty_description( );
	empty_sensitivity( rsense ); 	// discard sensitivity analysis data
	rsense = NULL;
	unsavedSense = false;			// nothing to save
	findexSens = 0;
	nodesSerial = 0;				// network node serial number global counter
	add_description( "Root", "Object", "(no description available)" );      
	cmd( "unset -nocomplain ModElem" );
	
	delete [ ] path;
	path = new char[ strlen( exec_path ) + 1 ];
	strcpy( path, exec_path );
	cmd( "set path \"%s\"; cd \"$path\"", path );
	
	delete [ ] simul_name;
	simul_name = new char[ strlen( "Sim1" ) + 1 ];
	strcpy( simul_name, "Sim1" );
	strcpy( lsd_eq_file, "" );
	sprintf( name_rep, "report_%s.html", simul_name );

	unsaved_change( false );	// signal no unsaved change
	redrawRoot = true;			// force browser redraw
	cmd( "set cur 0" ); 	// point for first var in listbox

break;


// Simulation manager: sets seeds, number of steps, number of simulations
case 22:

	// save previous values to allow canceling operation
	temp[ 1 ] = sim_num; 
	temp[ 2 ] = seed; 
	temp[ 3 ] = max_step; 
	temp[ 4 ] = when_debug;
	temp[ 5 ] = stack_info;
	temp[ 6 ] = prof_min_msecs;
	temp[ 7 ] = prof_obs_only;
	temp[ 8 ] = parallel_disable;

	Tcl_LinkVar( inter, "sim_num", ( char * ) &sim_num, TCL_LINK_INT );
	Tcl_LinkVar( inter, "seed", ( char * ) &seed, TCL_LINK_INT );
	Tcl_LinkVar( inter, "max_step", ( char * ) &max_step, TCL_LINK_INT );
	Tcl_LinkVar( inter, "stack_info", ( char * ) &stack_info, TCL_LINK_INT );
	Tcl_LinkVar( inter, "prof_min_msecs", ( char * ) &prof_min_msecs, TCL_LINK_INT );
	Tcl_LinkVar( inter, "prof_obs_only", ( char * ) &prof_obs_only, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "prof_aggr_time", ( char * ) &prof_aggr_time, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "parallel_disable", ( char * ) &parallel_disable, TCL_LINK_BOOLEAN );

	cmd( "set T .simset" );
	cmd( "newtop $T \"Simulation Settings\" { set choice 2 }" );

	cmd( "frame $T.f" );

	cmd( "frame $T.f.c" );
	cmd( "label $T.f.c.l2 -width 25 -anchor e -text \"Simulation steps\"" );
	cmd( "entry $T.f.c.e2 -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set max_step %%P; return 1 } { %%W delete 0 end; %%W insert 0 $max_step; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.c.e2 insert 0 $max_step" ); 
	cmd( "pack $T.f.c.l2 $T.f.c.e2 -side left -anchor w -padx 2 -pady 2" );

	cmd( "frame $T.f.a" );
	cmd( "label $T.f.a.l -width 25 -anchor e -text \"Number of simulation runs\"" );
	cmd( "entry $T.f.a.e -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set sim_num %%P; return 1 } { %%W delete 0 end; %%W insert 0 $sim_num; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.a.e insert 0 $sim_num" ); 
	cmd( "pack $T.f.a.l $T.f.a.e -side left -anchor w -padx 2 -pady 2" );

	cmd( "frame $T.f.b" );
	cmd( "label $T.f.b.l1 -width 25 -anchor e -text \"Random numbers initial seed\"" );
	cmd( "entry $T.f.b.e1 -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set seed %%P; return 1 } { %%W delete 0 end; %%W insert 0 $seed; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.b.e1 insert 0 $seed" ); 
	cmd( "pack $T.f.b.l1 $T.f.b.e1 -side left -anchor w -padx 2 -pady 2" );

	cmd( "frame $T.f.d" );
	cmd( "label $T.f.d.l2 -width 25 -anchor e -text \"Start debugger at step (0:none)\"" );
	cmd( "entry $T.f.d.e2 -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set when_debug %%P; return 1 } { %%W delete 0 end; %%W insert 0 $when_debug; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.d.e2 insert 0 $when_debug" ); 
	cmd( "pack $T.f.d.l2 $T.f.d.e2 -side left -anchor w -padx 2 -pady 2" );

	cmd( "frame $T.f.e" );
	cmd( "label $T.f.e.l2 -width 25 -anchor e -text \"Profile up to stack level (0:none)\"" );
	cmd( "entry $T.f.e.e2 -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set stack_info %%P; return 1 } { %%W delete 0 end; %%W insert 0 $stack_info; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.e.e2 insert 0 $stack_info" ); 
	cmd( "pack $T.f.e.l2 $T.f.e.e2 -side left -anchor w -padx 2 -pady 2" );

	cmd( "frame $T.f.f" );
	cmd( "label $T.f.f.l2 -width 25 -anchor e -text \"Profile minimum time (0:all)\"" );
	cmd( "entry $T.f.f.e2 -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set prof_min_msecs %%P; return 1 } { %%W delete 0 end; %%W insert 0 $prof_min_msecs; return 0 } } -invcmd { bell } -justify center" );
	cmd( "$T.f.f.e2 insert 0 $prof_min_msecs" ); 
	cmd( "pack $T.f.f.l2 $T.f.f.e2 -side left -anchor w -padx 2 -pady 2" );

	cmd( "pack $T.f.c $T.f.a $T.f.b $T.f.d $T.f.e $T.f.f -anchor w" );

	cmd( "checkbutton $T.f.obs -text \"Profile observed variables only\" -variable prof_obs_only" );
	cmd( "checkbutton $T.f.aggr -text \"Show aggregated profiling times\" -variable prof_aggr_time" );

#ifdef PARALLEL_MODE
	cmd( "checkbutton $T.f.npar -text \"Disable parallel computation\" -variable parallel_disable" );
	if ( ! search_parallel( root ) || max_threads < 2 )
		cmd( "$T.f.npar configure -state disabled" );
	cmd( "pack $T.f.obs $T.f.aggr $T.f.npar -anchor w" );
#else
	cmd( "pack $T.f.obs $T.f.aggr -anchor w" );
#endif

	cmd( "pack $T.f -padx 5 -pady 5" );
	cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp menurun.html#simsetting } { set choice 2 }" );
	cmd( "bind $T.f.c.e2 <KeyPress-Return> {focus $T.f.a.e; $T.f.a.e selection range 0 end}" );
	cmd( "bind $T.f.a.e <KeyPress-Return> {focus $T.f.b.e1; $T.f.b.e1 selection range 0 end}" );
	cmd( "bind $T.f.b.e1 <KeyPress-Return> {focus $T.f.d.e2; $T.f.d.e2 selection range 0 end}" );
	cmd( "bind $T.f.d.e2 <KeyPress-Return> {focus $T.f.e.e2; $T.f.e.e2 selection range 0 end}" );
	cmd( "bind $T.f.e.e2 <KeyPress-Return> {focus $T.f.f.e2; $T.f.f.e2 selection range 0 end}" );
	cmd( "bind $T.f.f.e2 <KeyPress-Return>  {focus $T.b.ok}" );

	cmd( "showtop $T" );
	cmd( "$T.f.c.e2 selection range 0 end" );
	cmd( "focus $T.f.c.e2" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set sim_num [ .simset.f.a.e get ]" ); 
	cmd( "set seed [ .simset.f.b.e1 get ]" ); 
	cmd( "set max_step [ .simset.f.c.e2 get ]" ); 
	cmd( "set when_debug [ .simset.f.d.e2 get ]" ); 
	cmd( "set stack_info [ .simset.f.e.e2 get ]" ); 
	cmd( "set prof_min_msecs [ .simset.f.f.e2 get ]" ); 

	cmd( "destroytop .simset" );

	if ( *choice == 2 )	// Escape - revert previous values
	{
		sim_num = temp[ 1 ];
		seed = temp[ 2 ];
		max_step = temp[ 3 ];
		when_debug = temp[ 4 ];
		stack_info = temp[ 5 ];
		prof_min_msecs = temp[ 6 ];
		prof_obs_only = temp[ 7 ];
		parallel_disable = temp[ 8 ];
	}
	else
		// signal unsaved change if anything to be saved
		if ( temp[ 1 ] != sim_num || temp[ 2 ] != seed || temp[ 3 ] != max_step ) 
			unsaved_change( true );

	Tcl_UnlinkVar( inter, "sim_num" );
	Tcl_UnlinkVar( inter, "seed" );
	Tcl_UnlinkVar( inter, "max_step" );
	Tcl_UnlinkVar( inter, "stack_info" );
	Tcl_UnlinkVar( inter, "prof_min_msecs" );
	Tcl_UnlinkVar( inter, "prof_obs_only" );
	Tcl_UnlinkVar( inter, "prof_aggr_time" );
	Tcl_UnlinkVar( inter, "parallel_disable" );

break;


// Move browser to Object pointed on the graphical model map
case 24:

	if ( res_g == NULL )
		break;

	for ( n = r; n->up != NULL; n = n->up );
	n = n->search( res_g );

	if ( n != r )
	{
		redrawRoot = true;	// force browser redraw
		cmd( "set cur 0; set listfocus 1; set itemfocus 0" ); // point for first var in listbox
	}

	*choice = 0;
	return n;


// Edit initial values of Objects pointed on the graphical map (NOT USED)
case 25:

	if (res_g == NULL )
	  break;

	for ( n = r; n->up != NULL; n = n->up );
	r = n->search( res_g );

	*choice = 0;
	edit_data( n, choice, r->label );
	cmd( "destroytop .ini" );

	unsaved_change( true );		// signal unsaved change
	choice_g = 0;

break;


// Enter the analysis of results module
case 26:

	analysis( choice );

break;


// Remove all the debugging flags
case 27:

	cmd( "set answer [ tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Remove debug flags?\" -detail \"Confirm the removal of all debugging information. Debugger will not stop in any variable update.\" ]; switch $answer { yes { set choice 1 } no { set choice 2 } }" );

	if ( *choice == 1 )
	{
		for ( n = r; n->up != NULL; n = n->up );
		clean_debug( n );
		unsaved_change( true );		// signal unsaved change
	}

break;


// Change Equation File from which to take the code to show
case 28:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to change the equation file.\"" );
		break;
	}

	cmd( "set res %s", equation_name );

	cmd( "set res1 [ file tail [ tk_getOpenFile -parent . -title \"Select New Equation File\" -initialfile \"$res\" -initialdir \"%s\" -filetypes { { {LSD equation files} {.cpp} } { {All files} {*} } } ] ]", exec_path );
	cmd( "if [ fn_spaces \"$res1\" . ] { set res1 \"\" } { set res1 [ file tail $res1 ] }" );

	lab1 = ( char * ) Tcl_GetVar( inter, "res1", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%499s", lab );
	strncpy( equation_name, lab, MAX_PATH_LENGTH - 1 );
	unsaved_change( true );		// signal unsaved change

break;


// Shortcut to show equation window
case 29:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	*choice = 0;	// point . window as parent for the following window
	show_eq( lab_old, choice );

break;


// Remove all the save flags
case 30:

	cmd( "set answer [ tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Remove save flags?\" -detail \"Confirm the removal of all saving information. No data will be saved.\" ]; switch $answer { yes { set choice 1 } no { set choice 2 } }" );

	if ( *choice == 1 )
	{
		for ( n = r; n->up != NULL; n = n->up );
		clean_save( n );
		unsaved_change( true );		// signal unsaved change
	}

break;


// Show variables to be saved
case 39:

	for ( n = r; n->up != NULL; n = n->up );
	i = 0;
	count_save( n, &i );
	if ( i == 0 )
		plog( " \nNo variable or parameter saved." );
	else
	{
		plog( "\n\nVariables and parameters saved (%d):\n", "", i );
		show_save( n );
	}

break;


// Show variables to be observed
case 42:

	for ( n = r; n->up != NULL; n = n->up );
	plog( "\n\nVariables and parameters containing results:\n" );
	lcount = 0;
	show_observe( n );
	if ( lcount == 0 )
		plog( "(none)\n" );

break;


// Show variables to be initialized
case 49:

	for ( n = r; n->up != NULL; n = n->up );
	plog( "\n\nVariables and parameters relevant to initialize:\n" );
	lcount = 0;
	show_initial( n );
	if ( lcount == 0 )
		plog( "(none)\n" );

break;


// Show variables to be plot
case 84:

	for ( n = r; n->up != NULL; n = n->up );
	plog( "\n\nVariables and parameters to plot in run time:\n" );
	lcount = 0;
	show_plot( n );
	if ( lcount == 0 )
		plog( "(none)\n" );

break;


// Show variables to debug
case 85:

	for ( n = r; n->up != NULL; n = n->up );
	plog( "\n\nVariables to debug:\n" );
	lcount = 0;
	show_debug( n );
	if ( lcount == 0 )
		plog( "(none)\n" );

break;


// Show variables to parallelize
case 86:

	for ( n = r; n->up != NULL; n = n->up );
	plog( "\n\nMulti-object variables to run in parallel:\n" );
	lcount = 0;
	show_parallel( n );
	if ( lcount == 0 )
		plog( "(none)\n" );

break;


// Close all Runtime Plots
case 40:

	cmd( "set a [split [winfo children .] ]" );
	cmd( " foreach i $a {if [string match .plt* $i] {destroytop $i}}" );

break;


// Remove all the plot flags
case 31:

	cmd( "set answer [ tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Remove plot flags?\" -detail \"Confirm the removal of all run-time plot information. No data will be plotted during run time.\" ]; switch $answer { yes { set choice 1 } no { set choice 2 } }" );

	if ( *choice == 1 )
	{
		for ( n = r; n->up != NULL; n = n->up );
		clean_plot( n );
		unsaved_change( true );		// signal unsaved change
	}

break;


// Remove all the parallel flags
case 87:

	cmd( "set answer [ tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Remove parallel flags?\" -detail \"Confirm the removal of all parallel processing information. No parallelization will be performed.\" ]; switch $answer { yes { set choice 1 } no { set choice 2 } }" );

	if ( *choice == 1 )
	{
		for ( n = r; n->up != NULL; n = n->up );
		clean_parallel(n);
		unsaved_change( true );		// signal unsaved change
	}

break;


// Changes the number of instances of only the Object type shown
// in the browser or the pointed object (defined in tcl $vname)
case 33:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	if ( r->up == NULL )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Cannot create copies of 'Root' object\" -detail \"Consider, if necessary, to add a new parent object here: all the elements will be moved in the newly created object, which can be multiplied in many copies.\"" );
		goto here_endinst;
	}

	skip_next_obj( r, &num );
	Tcl_LinkVar( inter, "num", ( char * ) &num, TCL_LINK_INT );

	cmd( "set cfrom 1" );

	cmd( "set T .numinst" );
	cmd( "newtop $T \"Number of Instances\" { set choice 2 }" );

	cmd( "frame $T.l" );
	cmd( "label $T.l.l1 -text \"Object:\"" );
	cmd( "label $T.l.l2 -text \"%s\" -fg red", r->label );
	cmd( "pack $T.l.l1 $T.l.l2 -side left" );

	cmd( "frame $T.e" );

	cmd( "frame $T.e.e" );
	cmd( "label $T.e.e.l -text \"Number of instances\"" );
	cmd( "entry $T.e.e.ent -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set num %%P; return 1 } { %%W delete 0 end; %%W insert 0 $num; return 0 } } -invcmd { bell } -justify center" );
	cmd( "pack $T.e.e.l $T.e.e.ent -side left -padx 2" );

	cmd( "label $T.e.l -text \"(all groups of this object will be affected)\"" );
	cmd( "pack $T.e.e $T.e.l" );

	cmd( "frame $T.cp" );
	cmd( "label $T.cp.l -text \"Copy from instance\"" );
	cmd( "entry $T.cp.e -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set cfrom %%P; return 1 } { %%W delete 0 end; %%W insert 0 $cfrom; return 0 } } -invcmd { bell } -justify center" );
	cmd( "button $T.cp.compute -width 7 -text Compute -command { set choice 3 }" );
	cmd( "pack $T.cp.l $T.cp.e $T.cp.compute -side left -padx 2" );

	cmd( "pack $T.l $T.e $T.cp -pady 5 -padx 5" );

	cmd( "okhelpcancel $T b { set choice 1 } { LsdHelp menudata_objn.html#this } { set choice 2 }" );
	cmd( "bind $T.e.e.ent <KeyPress-Return> {set choice 1}" );

	cmd( "showtop $T" );

	i = 1;
	*choice = 0;

	here_objec_num:

	cmd( "write_any .numinst.e.e.ent $num" ); 
	cmd( "write_any .numinst.cp.e $cfrom" ); 

	if ( i == 1 )
	{
		cmd( ".numinst.e.e.ent selection range 0 end" );
		cmd( "focus .numinst.e.e.ent" );
		i = 0;
	}

	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set num [ .numinst.e.e.ent get ]" ); 
	cmd( "set cfrom [ .numinst.cp.e get ]" ); 

	if ( *choice == 3 )
	{
		*choice = 0;
		k = compute_copyfrom( r, choice );
		if ( k > 0 )
			cmd( "set cfrom %d", k );
		*choice = 0;
		goto here_objec_num;
	} 

	cmd( "destroytop .numinst" );
	Tcl_UnlinkVar( inter, "num" );

	if ( *choice == 2 )
		goto here_endinst;

	cmd( "set choice $cfrom" );
	k = *choice;
	*choice = 0;

	for ( i = 0, cur = r->up; cur != NULL; ++i, cur = cur->up ); 
	chg_obj_num( &r, num, i, NULL, choice, k );

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// update list boxes

	here_endinst:
	if ( cur2 != NULL )			// restore original current object
		r = cur2;

break;


// Browse through the model instance by instance
case 34:

	// check if current or pointed object and save current if needed
	lab1 = ( char * ) Tcl_GetVar( inter, "useCurrObj", 0 );
	if ( lab1 != NULL && ! strcmp( lab1, "no" ) )
	{
		lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
		if ( lab1 == NULL || ! strcmp( lab1, "" ) )
			break;
		sscanf( lab1, "%99s", lab_old );
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lab_old );		// set pointer to $vname
		if ( n == NULL )
			break;
		cur2 = r;
		r = n;
	}
	else
		cur2 = NULL;

	deb( r, NULL, NULL, &fake );

	if ( cur2 != NULL )			// restore original current object
		r = cur2;

break;


// Window destroyed
case 35:

	if ( discard_change( ) )	// check for unsaved configuration changes
		myexit( 0 );

break;


// Create model report
case 36:

	for ( n = r; n->up != NULL; n = n->up );

	report( choice, n );

break;


// Save result
case 37:

	*choice = 0;
	if ( actual_steps == 0 )
	{
		cmd( "tk_messageBox -parent . -title Error -icon error -type ok -message \"Simulation not run, nothing to save\" -detail \"Please select in the menu Run the option Run before using this option.\"" );
		break;
	}

	Tcl_LinkVar( inter, "docsv", ( char * ) & docsv, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "dozip", ( char * ) & dozip, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "saveConf", ( char * ) & saveConf, TCL_LINK_BOOLEAN );

	time_t rawtime;
	time( &rawtime );
	struct tm *timeinfo;
	char ftime[80];
	timeinfo = localtime( &rawtime );
	strftime ( ftime, 80, "%Y%m%d-%H%M%S", timeinfo );

	cmd( "set lab \"result_%s_%s\"", simul_name, ftime );
	  
	// choose a name
	cmd( "newtop .n \"Save Results\" { set choice 2 }" );

	cmd( "frame .n.n" ); 
	cmd( "label .n.n.l -text \"Base name for file(s)\"" );
	cmd( "entry .n.n.e -width 30 -textvariable lab -justify center" );
	cmd( "pack .n.n.l .n.n.e" );

	cmd( "frame .n.do" );
	cmd( "checkbutton .n.do.zip -text \"Generate zipped results file\" -variable dozip" );
	cmd( "checkbutton .n.do.csv -text \"Comma-separated text format (.csv)\" -variable docsv" );
	cmd( "checkbutton .n.do.conf -text \"Save associated configuration\" -variable saveConf" );
	cmd( "pack .n.do.zip .n.do.csv .n.do.conf -anchor w" );

	cmd( "pack .n.n .n.do -padx 5 -pady 5" );

	cmd( "okcancel .n b { set choice 1 } { set choice 2 }" );
	cmd( "bind .n <KeyPress-Return> {set choice 1}" );

	cmd( "showtop .n" );
	cmd( "focus .n.n.e" );
	cmd( ".n.n.e selection range 0 end" );  

	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "if { [ string length lab ] == 0 } { set choice 2 }" );

	cmd( "destroytop .n" );

	Tcl_UnlinkVar( inter, "docsv" );
	Tcl_UnlinkVar( inter, "dozip" );
	Tcl_UnlinkVar( inter, "saveConf" );

	if ( *choice == 2 )
		break;

	cmd( "wm deiconify .log; raise .log; focus .log" );

	lab1 = ( char * ) Tcl_GetVar( inter, "lab", 0 );
	strncpy( lab, lab1, MAX_PATH_LENGTH - 1 );
	if ( saveConf )
	{
		plog( "\nLSD configuration file: %s.lsd", "", lab );
		cmd( "file copy -force %s.lsd %s.lsd", simul_name, lab );
	}

	plog( "\nLSD results file: %s.%s%s\nSaving data...", "", lab, docsv ? "csv" : "res", dozip ? ".gz" : "" );

	if ( strlen( path ) == 0 )
		sprintf( msg, "%s.%s", lab, docsv ? "csv" : "res" );
	else
		sprintf( msg, "%s/%s.%s", path, lab, docsv ? "csv" : "res" );
		
	rf = new result( msg, "wt", dozip, docsv );	// create results file object
	for ( n = r; n->up != NULL; n = n->up );		// get root object
	rf->title( n, 1 );							// write header
	rf->data( n, 0, actual_steps );				// write all data
	delete rf;									// close file and delete object
	plog( " Done\n" );

	unsavedData = false;						// no unsaved simulation results

break;


// Help on lsd
case 41:

	cmd( "LsdHelp LSD_quickhelp.html" );

break;


// Create automatically the elements descriptions
case 43:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to update descriptions.\"" );
		break;
	}

	cmd( "set answer [tk_messageBox -parent . -message \"Replace existing descriptions?\" -detail \"Automatic data will replace any previous entered descriptions. Proceed?\" -type yesno -title Confirmation -icon question -default yes]" );
	cmd( "if { [ string compare $answer yes ] == 0 } { set choice 0 } { set choice 1 }" );

	if ( *choice == 1 )
		break;

	cmd( "set x 1" );

	cmd( "newtop .warn \"Generate Descriptions\" { set choice 2 }" );

	cmd( "frame .warn.m" );
	cmd( "label .warn.m.l -text \"Elements to update\"" );

	cmd( "frame .warn.m.o -relief groove -bd 2" );
	cmd( "radiobutton .warn.m.o.var -text \"Only variables\" -variable x -value 1" );
	cmd( "radiobutton .warn.m.o.all -text \"All elements\" -variable x -value 2" );
	cmd( "pack .warn.m.o.var .warn.m.o.all -anchor w" );

	cmd( "pack .warn.m.l .warn.m.o" );

	cmd( "pack .warn.m -padx 5 -pady 5" );

	cmd( "okhelpcancel .warn f { set choice 1 } { LsdHelp menumodel.html#auto_docu } { set choice 2 }" );

	cmd( "showtop .warn" );

	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .warn" );

	if ( *choice == 2 )
		break;

	cmd( "set choice $x" );
	if ( *choice == 1 )
		auto_document( choice, NULL, "" );
	else
		auto_document( choice, NULL, "ALL" ); 

	unsaved_change( true );		// signal unsaved change

break;


// See model report
case 44:

	sprintf( name_rep, "report_%s.html", simul_name );
	cmd( "set choice [file exists %s]", name_rep );
	if ( *choice == 0 )
	{
		cmd( "set answer [tk_messageBox -parent . -message \"Model report not found\" -detail \"You may create a model report file from menu Model or press 'OK' to look for another HTML file.\" -type okcancel -title Warning -icon warning -default cancel]" );
		cmd( "if { [ string compare $answer ok ] == 0 } { set choice 0 } { set choice 1 }" );
		if ( *choice == 1 )
			break;

		cmd( "set fname [ tk_getOpenFile -parent . -title \"Load Report File\" -defaultextension \".html\" -initialdir \"%s\" -filetypes { { {HTML files} {.html} } { {All files} {*} } } ]", exec_path );
		cmd( "if { $fname == \"\" || [ fn_spaces \"$fname\" . ] } { set choice 0 } { set fname [ file tail $fname ]; set choice 1 }" );
		if ( *choice == 0 )
			break;
	}
	else
		cmd( "set fname %s", name_rep );

	lab1 = ( char * ) Tcl_GetVar( inter, "app", 0 );
	cmd( "set app $tcl_platform(osVersion)" );

	lab1 = ( char * ) Tcl_GetVar( inter, "app", 0 );

	if ( *choice == 1 ) 		// model report exists
		cmd( "LsdHtml $fname" );
  
break;


// Save descriptions
case 45:

	lab1= ( char * ) Tcl_GetVar( inter, "vname", 0 );
	strncpy( lab, lab1, MAX_PATH_LENGTH - 1);

	change_descr_text( lab );

	unsaved_change( true );		// signal unsaved change

break;


// Show the vars./pars. vname is using
case 46:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab );

	*choice = 0;				// make . the parent window
	scan_using_lab( lab, choice);

break;


// Show the equations where vname is used 
case 47:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab );

	*choice = 0;				// make . the parent window
	scan_used_lab( lab, choice);

break;


// Set the Html browser for Unix systems
case 48:

	cmd( "set temp_var $HtmlBrowser" );

	cmd( "newtop .a \"Set Browser\" { set choice 2 }" );

	cmd( "label .a.l2 -text \"HTML browser for help pages\"" );
	cmd( "entry .a.v_num2 -width 20 -textvariable temp_var -justify center" );
	cmd( "bind .a.v_num2 <Return> { set choice 1 }" );
	cmd( "pack .a.l2 .a.v_num2 -padx 5" );

	cmd( "okXhelpcancel .a f Default { set temp_var mozilla } { set choice 1 } { LsdHelp LSD_macros.html#V } { set choice 2 }" );

	cmd( "showtop .a" );
	cmd( "focus .a.v_num2" );
	cmd( ".a.v_num2 selection range 0 end" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	if ( *choice == 1 )
		cmd( "set HtmlBrowser $temp_var" );

	cmd( "destroytop .a" );

break;


// Find an element of the model
case 50: 

	cmd( "set bidi \"\"" );

	cmd( "newtop .srch \"Find Element\" { set choice 2 }" );

	cmd( "frame .srch.i" );
	cmd( "label .srch.i.l -text \"Element name\"" );
	cmd( "entry .srch.i.e -width 20 -textvariable bidi -justify center" );
	cmd( "pack .srch.i.l .srch.i.e" );

	cmd( "label .srch.o -text \"(type the initial letters of the\nname, LSD will complete it)\"" );
	cmd( "pack .srch.i .srch.o -padx 5 -pady 5" );
	cmd( "pack .srch.i" );

	cmd( "okcancel .srch b { set choice 1 } { set choice 2 }" );

	cmd( "bind .srch.i.e <KeyPress-Return> { set choice 1 }" );
	cmd( "bind .srch.i.e <KeyRelease> {if { %%N < 256 && [ info exists ModElem ] } { set b [.srch.i.e index insert]; set s [.srch.i.e get]; set f [lsearch -glob $ModElem $s*]; if { $f !=-1 } {set d [lindex $ModElem $f]; .srch.i.e delete 0 end; .srch.i.e insert 0 $d; .srch.i.e index $b; .srch.i.e selection range $b end } } }" );

	cmd( "showtop .srch" );
	cmd( "focus .srch.i.e" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "destroytop .srch" );

	if ( *choice == 2 )
		break;


// Arrive here from the list of vars used (keep together with case 50!)
case 55:

	*choice = 0;
	lab1 = ( char * ) Tcl_GetVar( inter, "bidi", 0 );
	strncpy( msg, lab1, MAX_ELEM_LENGTH - 1 );
	cv = r->search_var( r, msg, true );
	if ( cv != NULL )
	{
		for ( i = 0, cur_v = cv->up->v; cur_v != cv; cur_v = cur_v->next, ++i );
		cmd( "set cur %d; set listfocus 1; set itemfocus $cur", i );
		redrawRoot = true;			// request browser redraw
		return cv->up;
	}
	else
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Element not found\" -detail \"Check the spelling of the element name.\"" );

break;


// Upload in memory current equation file
case 51:
	/*
	Replace lsd_eq_file with the eq_file. That is, make appear actually used equation file as the one used for the current configuration
	*/

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to upload an equation file.\"" );
		break;
	}

	if ( !strcmp( eq_file, lsd_eq_file ) )
	{
		cmd( "tk_messageBox -parent . -title \"Upload Equations\" -icon info -message \"Nothing to do\" -detail \"There are no equations to be uploaded differing from the current configuration file.\" -type ok" );
		break;
	}

	cmd( "set answer [tk_messageBox -parent . -title Confirmation -icon question -message \"Replace equations?\" -detail \"The equations associated to the configuration file are going to be replaced with the equations used for the LSD model program. Press 'OK' to confirm.\" -type okcancel -default ok]" );
	cmd( "if {[string compare $answer ok] == 0} {set choice 1} {set choice 0}" );
	if ( *choice == 0 )
		break;

	lsd_eq_file[ MAX_FILE_SIZE ] = '\0';
	strncpy( lsd_eq_file, eq_file, MAX_FILE_SIZE );
	unsaved_change( true );		// signal unsaved change

break;


// Offload configuration's equations in a new equation file
case 52: 
	/*
	Used to re-generate the equations used for the current configuration file
	*/

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to offload an equation file.\"" );
		break;
	}

	if ( !strcmp( eq_file, lsd_eq_file ) )
	 {cmd( "tk_messageBox -parent . -title \"Offload Equations\" -icon info -message \"Nothing to do\" -detail \"There are no equations to be offloaded differing from the current equation file.\" -type ok" );
	 break;
	 }

	cmd( "set res1 fun_%s.cpp", simul_name );
	cmd( "set bah [ tk_getSaveFile -parent . -title \"Save Equation File\" -defaultextension \".cpp\" -initialfile $res1 -initialdir \"%s\" -filetypes { { {LSD equation files} {.cpp} } { {All files} {*} } } ]", exec_path );

	cmd( "if {[string length $bah] > 0} { set choice 1; set res1 [file tail $bah]} {set choice 0}" );
	if ( *choice == 0 )
	  break;

	lab1 = ( char * ) Tcl_GetVar( inter, "res1", 0 );
	strncpy( lab, lab1, MAX_PATH_LENGTH - 1 );

	if ( strlen( lab ) == 0 )
		break;
	
	f = fopen( lab, "wb" );
	fprintf( f, "%s", lsd_eq_file );
	fclose( f );
	cmd( "tk_messageBox -parent . -title \"Offload Equations\" -icon info -message \"Equation file '$res1' created\" -detail \"You need to create a new LSD model to use these equations, replacing the name of the equation file in LMM with the command 'Model Compilation Options' (menu Model).\" -type ok" );

break;


// Compare equation files
case 53: 

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to compare equation files.\"" );
		break;
	}

	if ( strlen( lsd_eq_file ) == 0 )
	{
		cmd( "tk_messageBox -parent . -type ok -icon Warning -title Warning -message \"No equations loaded\" -detail \"Please upload an equation file before trying to compare equation files.\"" );
		break;
	}

	sprintf( ch, "orig-eq_%s.tmp", simul_name);
	f = fopen( ch, "wb" );
	fprintf( f, "%s", lsd_eq_file );
	fclose( f );

	read_eq_filename( lab );
	cmd( "LsdTkDiff %s %s \"Equations on '%s'\" \"Equations on '%s.lsd'\"", lab, ch, equation_name, simul_name  );

break;


// Compare configuration files
case 82: 

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to compare configuration files.\"" );
		break;
	}

	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	cmd( "set res1 [ tk_getOpenFile -parent . -title \"Select Configuration File to Compare to\" -initialdir \"$path\" -filetypes { { {LSD configuration files} {.lsd} } } ]" );
	cmd( "set res2 [ file tail $res1 ]" );
	cmd( "if [ fn_spaces \"$res1\" . ] { set res1 \"\"; set res2 \"\" }" );

	lab1 = ( char * ) Tcl_GetVar( inter, "res1", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "res2", 0 );
	if ( lab1 == NULL || lab2 == NULL || strlen ( lab1 ) == 0 || strlen ( lab2 ) == 0 )
		break;

	f = fopen( lab1, "r" );
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Cannot open file\" -detail \"Error opening file '%s'.\"", lab2 );
		break;
	}
	fclose( f );

	cmd( "file copy -force -- $res1 ext-cfg.tmp" );
	cmd( "file copy -force -- %s int-cfg.tmp", struct_file );
	cmd( "LsdTkDiff ext-cfg.tmp int-cfg.tmp \"Configuration on '%s'\" \"Configuration on '%s.lsd' (LOADED)\"", lab2, simul_name );

break;


// Toggle ignore eq. file controls
case 54:

	cmd( "set choice $ignore_eq_file" );
	ignore_eq_file = *choice;
	plog( "\n%s equation file\n", "", ignore_eq_file ? "Ignoring" : "Not ignoring" );

break; 


// Generate Latex code
case 57:
case 92:

	table = ( *choice == 57 ) ? true : false;
	
	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to create LaTex code.\"" );
		break;
	}

	sprintf( ch, "%s%s%s_%s.tex", strlen( path ) > 0 ? path : "", strlen( path ) > 0 ? "/" : "", table ? "table" : "href", simul_name );
	cmd( "set choice [ file exists %s ]", ch );
	if ( *choice == 1 )
	{
		cmd( "set answer [ tk_messageBox -parent . -message \"File '%s' already exists\" -detail \"Please confirm overwriting it.\" -type okcancel -title Warning -icon warning -default ok ]", ch );
		cmd( "if [ string equal $answer ok ] { set choice 0 } { set choice 1 }" );
		if ( *choice == 1 )
			break;
	}

	cmd( "wm deiconify .log; raise .log; focus .log" );
	plog("\nWriting LaTex code. Please wait... " );

	f = fopen( ch, "wt" );
	tex_report_head( f, table );
	tex_report_struct( root, f, table );
	tex_report_observe( root, f, table );
	tex_report_init( root, f, table );
	tex_report_initall( root, f, table );
	tex_report_end( f );
	fclose( f );

	plog( "Done\nLaTex code saved in file: %s\n", "", ch );

break;


// Move variable up in the list box
case 58:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	shift_var( -1, lab_old, r );

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

break;


// Move variable down in the list box
case 59:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	shift_var( 1, lab_old, r );

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

break;


// Move object up in the list box
case 60:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	shift_desc( -1, lab_old, r );

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

break;


// Move object down in the list box
case 61:

	lab1 = ( char * ) Tcl_GetVar( inter, "vname", 0 );
	if ( lab1 == NULL || ! strcmp( lab1, "" ) )
		break;
	sscanf( lab1, "%99s", lab_old );

	shift_desc( 1, lab_old, r );

	unsaved_change( true );		// signal unsaved change
	redrawRoot = true;			// request browser redraw

break;


// Create parallel sensitivity analysis configuration
case 62:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables(rsense);	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );
		long ptsSa = num_sensitivity_points(rsense);	// total number of points in sensitivity space
		plog( "\nSensitivity analysis space size: %ld", "", ptsSa );
		
		// Prevent running into too big sensitivity spaces (high computation times)
		if ( ptsSa > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sensitivity analysis space size is too big!" );
			sensitivity_too_large( );		// ask user before proceeding
			if (*choice == 0)
				break;
		}
		
		for ( i = 1, cs = rsense; cs!=NULL; cs = cs->next )
			i*=cs->nvalues;
		cur = root->b->head;
		root->add_n_objects2( cur->label, i-1, cur );
		
		plog( "\nUpdating configuration, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		sensitivity_parallel( cur, rsense );
		
		unsaved_change( true );				// signal unsaved change
		cmd( "tk_messageBox -parent . -type ok -icon warning -title Warning -message \"Structure changed\" -detail \"LSD has changed your model structure, replicating the entire model for each sensitivity configuration. If you want to preserve your original configuration file, save your new configuration using a different name BEFORE running the model.\"" );
	}
	else
		sensitivity_undefined( );			// throw error

break;


// Create sequential sensitivity analysis configuration
case 63:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables(rsense);	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );
		long ptsSa = num_sensitivity_points(rsense);	// total number of points in sensitivity space
		plog( "\nSensitivity analysis space size: %ld", "", ptsSa );
		
		// Prevent running into too big sensitivity spaces (high computation times)
		if ( ptsSa > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sensitivity analysis space size is too big!" );
			sensitivity_too_large( );		// ask user before proceeding
			if ( *choice == 0 )
				break;
		}
		
		// save the current object & cursor position for quick reload
		save_pos( r );
		findexSens = 1;
		
		// create a design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiment, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		sensitivity_sequential( &findexSens, rsense );
		sensitivity_created( );				// explain user how to proceed
		
		// now reload the previously existing configuration
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );
		if ( load_configuration( r ) != 0 )
		{
			load_configuration_failed( );	// throw error message
			*choice = 20;
			break;
		}
			
		// restore pointed object and variable
		n = restore_pos( r );
		if ( n != r )
		{
			*choice = 0;
			return n;
		}
	}
	else
		sensitivity_undefined( );			// throw error

break;


// Create Monte Carlo (MC) random sensitivity analysis sampling configuration (over user selected point values)
case 71:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables(rsense);	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );
		long maxMC = num_sensitivity_points(rsense);	// total number of points in sensitivity space
		plog( "\nSensitivity analysis space size: %ld", "", maxMC );

		// get the number of Monte Carlo samples to produce
		double sizMC = 10;
		Tcl_LinkVar( inter, "sizMC", ( char * )&sizMC, TCL_LINK_DOUBLE );
		
		cmd( "newtop .s \"MC Point Sampling\" { set choice 2 }" );
		
		cmd( "frame .s.i" );
		cmd( "label .s.i.l -text \"Monte Carlo sample size as\n%% of sensitivity space size\n(0 to 100)\"" );
		cmd( "entry .s.i.e -width 5 -validate focusout -vcmd { if [ string is double -strict %%P ] { set sizMC %%P; return 1 } { %%W delete 0 end; %%W insert 0 $sizMC; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.i.e insert 0 $sizMC" ); 
		cmd( "pack .s.i.l .s.i.e" );
		
		cmd( "label .s.w -text \"(large samples are not recommended)\"" );
		
		cmd( "pack .s.i .s.w -padx 5 -pady 5" );

		cmd( "okhelpcancel .s b { set choice 1 } { LsdHelp menudata_sa.html#mcpoint } { set choice 2 }" );

		cmd( "showtop .s" );
		cmd( "focus .s.i.e" );
		cmd( ".s.i.e selection range 0 end" );

		*choice = 0;
		while ( *choice == 0 )
			Tcl_DoOneEvent( 0 );

		cmd( "set sizMC [ .s.i.e get ]" ); 
		cmd( "destroytop .s" );
		Tcl_UnlinkVar( inter, "sizMC" );

		if ( *choice == 2 )
			break;
		
		// Check if number is valid
		sizMC /= 100.0;
		if ( ( sizMC * maxMC ) < 1 || sizMC > 1.0 )
		{
			cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Invalid sample size\" -detail \"Invalid Monte Carlo sample size to perform the sensitivity analysis. Select a number between 0%% and 100%% that produces at least one sample (in average).\"" );
			*choice = 0;
			break;
		}

		// Prevent running into too big sensitivity space samples (high computation times)
		if ((sizMC * maxMC) > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sampled sensitivity analysis space size (%ld) is too big!", "", (long)(sizMC * maxMC) );
			sensitivity_too_large( );		// ask user before proceeding
			if (*choice == 0)
				break;
		}
		
		// save the current object & cursor position for quick reload
		save_pos( r );

		plog( "\nTarget sensitivity analysis sample size: %ld (%.1f%%)", "", (long)(sizMC * maxMC), 100 * sizMC );
		findexSens = 1;
		
		// create a design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiment, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		sensitivity_sequential( &findexSens, rsense, sizMC );
		sensitivity_created( );				// explain user how to proceed
		
		// now reload the previously existing configuration
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );
		if ( load_configuration( r ) != 0 )
		{
			load_configuration_failed( );	// throw error message
			*choice = 20;
			break;
		}

		// restore pointed object and variable
		n = restore_pos( r );
		if ( n != r )
		{
			*choice = 0;
			return n;
		}
	}
	else
		sensitivity_undefined( );			// throw error

break;


// Create Near Orthogonal Latin Hypercube (NOLH) sensitivity analysis sampling configuration
case 72:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables( rsense );	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );
		lab1 = NOLH_valid_tables( varSA, ch );

		cmd( "set extdoe 0" );	// flag for using external DoE file
		cmd( "set NOLHfile \"NOLH.csv\"" );
		cmd( "set doeList [list %s]", lab1 );
		cmd( "set doesize [ lindex $doeList 0 ]" );	// minimum Doe size
		cmd( "set doeext 0" );	// flag for using extended number of samples
		
		cmd( "newtop .s \"NOLH Sampling\" { set choice 2 }" );
		
		cmd( "frame .s.o" );
		cmd( "label .s.o.l1 -text \"NOLH table\"" );
		cmd( "ttk::combobox .s.o.c -width 15 -textvariable doesize -values $doeList -justify center" );
		cmd( "label .s.o.l2 -text \"(factors \u00D7 samples \u00D7 ext. samples)\"" );
		cmd( "pack .s.o.l1 .s.o.c .s.o.l2" );
		
		cmd( "checkbutton .s.e -text \"Extended number of samples\" -variable doeext" );
		if( varSA > 22 )
			cmd( ".s.e configure -state disabled" );
	
		cmd( "checkbutton .s.d -text \"External design file\" -variable extdoe -command { if { $extdoe == 1 } { .s.o.c configure -state disabled; .s.e configure -state disabled; .s.i.e configure -state normal; .s.i.e selection range 0 end; focus .s.i.e } { .s.o.c configure -state normal; .s.e configure -state normal; .s.i.e configure -state disabled } }" );
		
		cmd( "frame .s.i" );
		cmd( "label .s.i.l -text \"Design file name\"" );
		cmd( "entry .s.i.e -width 20 -justify center -textvariable NOLHfile -state disabled" );
		cmd( "label .s.i.w -text \"(file must be in the same folder\nas the configuration file; CSV\nformat with NO empty lines)\"" );
		cmd( "pack .s.i.l .s.i.e .s.i.w" );
		
		cmd( "pack .s.o .s.e .s.d .s.i -padx 5 -pady 5" );
		
		cmd( "okhelpcancel .s b { set choice 1 } { LsdHelp menudata_sa.html#nolh } { set choice 2 }" );
		
		cmd( "showtop .s" );
		
		*choice = 0;
		while ( *choice == 0 )
			Tcl_DoOneEvent( 0 );
		
		cmd( "destroytop .s" );
		
		if ( *choice == 2 )
			break;
		
		char NOLHfile[ MAX_PATH_LENGTH ];
		char const *extdoe = Tcl_GetVar( inter, "extdoe", 0 );
		char const *doesize = Tcl_GetVar( inter, "doesize", 0 );
		char const *doeext = Tcl_GetVar( inter, "doeext", 0 );
		
		if ( *extdoe == '0' )
			strcpy( NOLHfile, "" );
		else
		{
			char const *fname = Tcl_GetVar( inter, "NOLHfile", 0 );
			NOLHfile[ MAX_PATH_LENGTH - 1 ] = '\0';
			strncpy( NOLHfile, fname, MAX_PATH_LENGTH - 1 );
		}
		
		i = sscanf( doesize, "%d\u00D7", &j );
		int doesz = ( i > 0 ) ?  j : 0;
		int samples = ( *doeext == '0') ? 0 : -1;

		// adjust an NOLH design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiments, it may take a while, please wait... " );
		design *NOLHdoe = new design( rsense, 1, NOLHfile, 1, samples, doesz );
		
		if ( NOLHdoe -> n == 0 )					// DoE configuration is not ok?
		{
			cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Configuration error\" -detail \"It was not possible to create a Non Orthogonal Latin Hypercube (NOLH) Design of Experiment (DoE) for the current sensitivity configuration. If the number of variables (factors) is large than 29, an external NOLH has to be provided in the file NOLH.csv (empty lines not allowed).\"" );
			delete NOLHdoe;
			break;
		}

		// Prevent running into too big sensitivity space samples (high computation times)
		if ( NOLHdoe -> n > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sampled sensitivity analysis space size (%d) is too big!", "", NOLHdoe -> n );
			sensitivity_too_large( );		// ask user before proceeding
			if ( *choice == 0 )
			{
				delete NOLHdoe;
				break;
			}
		}
		
		// save the current object & cursor position for quick reload
		save_pos( r );
		findexSens = 1;
		
		// create a design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiment, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		sensitivity_doe( &findexSens, NOLHdoe );
		sensitivity_created( );				// explain user how to proceed
		
		delete NOLHdoe;

		// now reload the previously existing configuration
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );
		if ( load_configuration( r ) != 0 )
		{
			load_configuration_failed( );	// throw error message
			*choice = 20;
			break;
		}

		// restore pointed object and variable
		n = restore_pos( r );
		if ( n != r )
		{
			*choice = 0;
			return n;
		}
	}
	else
		sensitivity_undefined( );			// throw error

break;


// Create Monte Carlo (MC) random sensitivity analysis sampling configuration (over selected range values)
case 80:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables( rsense );	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );

		// get the number of Monte Carlo samples to produce
		int sizMC = 10;
		Tcl_LinkVar( inter, "sizMC", ( char * )&sizMC, TCL_LINK_INT );
		
		cmd( "set applst 1" );	// flag for appending to existing configuration files
		
		cmd( "newtop .s \"MC Range Sampling\" { set choice 2 }" );
		
		cmd( "frame .s.i" );
		cmd( "label .s.i.l -text \"Monte Carlo sample size\nas number of samples\"" );
		cmd( "entry .s.i.e -width 5 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set sizMC %%P; return 1 } { %%W delete 0 end; %%W insert 0 $sizMC; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.i.e insert 0 $sizMC" ); 
		cmd( "pack .s.i.l .s.i.e" );
		
		if ( findexSens > 1 )			// there are previously saved sensitivity files?
		{
			cmd( "checkbutton .s.c -text \"Append to existing configuration files\" -variable applst" );
			cmd( "pack .s.i .s.c -padx 5 -pady 5" );
		}
		else
			cmd( "pack .s.i -padx 5 -pady 5" );
		
		cmd( "okhelpcancel .s b { set choice 1 } { LsdHelp menudata_sa.html#mcrange } { set choice 2 }" );
		
		cmd( "showtop .s" );
		cmd( "focus .s.i.e" );
		cmd( ".s.i.e selection range 0 end" );
		
		*choice = 0;
		while ( *choice == 0 )
			Tcl_DoOneEvent( 0 );
		
		cmd( "set sizMC [ .s.i.e get ]" ); 
		cmd( "destroytop .s" );
		Tcl_UnlinkVar( inter, "sizMC" );
		
		if ( *choice == 2 )
			break;
		
		// Check if number is valid
		if ( sizMC < 1 )
		{
			cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Invalid sample size\" -detail \"Invalid Monte Carlo sample size to perform the sensitivity analysis. Select at least one sample.\"" );
			*choice = 0;
			break;
		}

		// Prevent running into too big sensitivity space samples (high computation times)
		if ( sizMC  > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sampled sensitivity analysis space size (%ld) is too big!", "", ( long )sizMC );
			sensitivity_too_large( );		// ask user before proceeding
			if ( *choice == 0 )
				break;
		}
		
		// save the current object & cursor position for quick reload
		save_pos( r );

		// check if design file numberig should pick-up from previously generated files
		if ( findexSens > 1 )
		{
			const char *applst = Tcl_GetVar( inter, "applst", 0 );
			if ( *applst == '0' )
				findexSens = 1;
		}
		else
			findexSens = 1;
		
		// adjust a design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiment, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		design *rand_doe = new design( rsense, 2, "", findexSens, sizMC );
		sensitivity_doe( &findexSens, rand_doe );
		sensitivity_created( );				// explain user how to proceed

		delete rand_doe;
		
		// now reload the previously existing configuration
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );
		if ( load_configuration( r ) != 0 )
		{
			load_configuration_failed( );	// throw error message
			*choice = 20;
			break;
		}

		// restore pointed object and variable
		n = restore_pos( r );
		if ( n != r )
		{
			*choice = 0;
			return n;
		}
	}
	else
		sensitivity_undefined( );			// throw error

break;


// Create Elementary Effects (EE) sensitivity analysis sampling configuration (over selected range values)
case 81:

	if ( rsense != NULL ) 
	{
		if ( ! discard_change( false ) )	// unsaved configuration?
			break;

		int varSA = num_sensitivity_variables(rsense);	// number of variables to test
		plog( "\nNumber of variables for sensitivity analysis: %d", "", varSA );

		// get the number of Monte Carlo samples to produce
		int nLevels = 4, jumpSz = 2, nTraj = 10, nSampl = 100;
		Tcl_LinkVar( inter, "varSA", ( char * )&varSA, TCL_LINK_INT );
		Tcl_LinkVar( inter, "nLevels", ( char * )&nLevels, TCL_LINK_INT );
		Tcl_LinkVar( inter, "jumpSz", ( char * )&jumpSz, TCL_LINK_INT );
		Tcl_LinkVar( inter, "nTraj", ( char * )&nTraj, TCL_LINK_INT );
		Tcl_LinkVar( inter, "nSampl", ( char * )&nSampl, TCL_LINK_INT );
		
		cmd( "newtop .s \"Elementary Effects Sampling\" { set choice 2 }" );
		
		cmd( "frame .s.i" );
		cmd( "label .s.i.l1 -text \"Number of trajectories (r)\"" );
		cmd( "entry .s.i.e1 -width 10 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set nTraj %%P; return 1 } { %%W delete 0 end; %%W insert 0 $nTraj; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.i.e1 insert 0 $nTraj" ); 
		cmd( "label .s.i.l2 -text \"([expr $varSA + 1]\u00D7r samples to create)\"" );
		cmd( "pack .s.i.l1 .s.i.e1 .s.i.l2" );	
		
		cmd( "frame .s.p" );
		cmd( "label .s.p.l1 -text \"Trajectories pool size (M)\"" );
		cmd( "entry .s.p.e2 -width 10 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set nSampl %%P; return 1 } { %%W delete 0 end; %%W insert 0 $nSampl; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.p.e2 insert 0 $nSampl" ); 
		cmd( "label .s.p.l2 -text \"(M > r enables optimization)\"" );
		cmd( "pack .s.p.l1 .s.p.e2 .s.p.l2" );	
		
		cmd( "frame .s.l" );
		cmd( "label .s.l.l1 -text \"Number of levels (p)\"" );
		cmd( "entry .s.l.e3 -width 10 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set nLevels %%P; return 1 } { %%W delete 0 end; %%W insert 0 $nLevels; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.l.e3 insert 0 $nLevels" ); 
		cmd( "label .s.l.l2 -text \"(must be even)\"" );
		cmd( "pack .s.l.l1 .s.l.e3 .s.l.l2" );	
		
		cmd( "frame .s.j" );
		cmd( "label .s.j.l1 -text \"Jump size\"" );
		cmd( "entry .s.j.e4 -width 10 -validate focusout -vcmd { if [ string is integer -strict %%P ] { set jumpSz %%P; return 1 } { %%W delete 0 end; %%W insert 0 $jumpSz; return 0 } } -invcmd { bell } -justify center" );
		cmd( ".s.j.e4 insert 0 $jumpSz" ); 
		cmd( "label .s.j.l2 -text \"( \u0394\u00D7(p - 1) )\"" );
		cmd( "pack .s.j.l1 .s.j.e4 .s.j.l2" );	
		
		cmd( "label .s.t -text \"(for details on setting Elementary Effects\nsampling parameters see Morris (1991),\nCampolongo et al. (2007) and Ruano et al. (2012))\"" );
		
		cmd( "pack .s.i .s.p .s.l .s.j .s.t -padx 5 -pady 5" );
		
		cmd( "okhelpcancel .s b { set choice 1 } { LsdHelp menudata_sa.html#ee } { set choice 2 }" );
		
		cmd( "showtop .s" );
		cmd( ".s.i.e1 selection range 0 end" );
		cmd( "focus .s.i.e1" );
		
		*choice = 0;
		while ( *choice == 0 )
			Tcl_DoOneEvent( 0 );
		
		cmd( "set nTraj [ .s.i.e1 get ]" ); 
		cmd( "set nSampl [ .s.p.e2 get ]" ); 
		cmd( "set nLevels [ .s.l.e3 get ]" ); 
		cmd( "set jumpSz [ .s.j.e4 get ]" ); 
		cmd( "destroytop .s" );
		Tcl_UnlinkVar( inter, "varSA" );
		Tcl_UnlinkVar( inter, "nLevels" );
		Tcl_UnlinkVar( inter, "jumpSz" );
		Tcl_UnlinkVar( inter, "nTraj" );
		Tcl_UnlinkVar( inter, "nSampl" );
		
		if ( *choice == 2 )
			break;
		
		// Check if numbers are valid
		if ( nLevels < 2 || nLevels % 2 != 0 || nTraj < 2 || nSampl < nTraj || jumpSz < 1 )
		{
			cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Invalid configuration\" -detail \"Invalid Elementary Effects configuration to perform the sensitivity analysis. Check Morris (1991) and Campolongo et al. (2007) for details.\"" );
			*choice = 0;
			break;
		}
		
		// Prevent running into too big sensitivity space samples (high computation times)
		if ( nTraj * ( varSA + 1 )  > MAX_SENS_POINTS )
		{
			plog( "\nWarning: sampled sensitivity analysis space size (%ld) is too big!", "", (long)( nTraj * ( varSA + 1 ) ) );
			sensitivity_too_large( );		// ask user before proceeding
			if (*choice == 0)
				break;
		}
		
		// save the current object & cursor position for quick reload
		save_pos( r );
		findexSens = 1;
		
		// adjust a design of experiment (DoE) for the sensitivity data
		plog( "\nCreating design of experiment, it may take a while, please wait... " );
		cmd( "wm deiconify .log; raise .log; focus .log" );
		design *rand_doe = new design( rsense, 3, "", findexSens, nSampl, nLevels, jumpSz, nTraj );
		sensitivity_doe( &findexSens, rand_doe );
		sensitivity_created( );				// explain user how to proceed

		delete rand_doe;
		
		// now reload the previously existing configuration
		for ( n = r; n->up != NULL; n = n->up );
		r = n;
		cmd( "destroytop .str" );
		cmd( "destroytop .lat" );
		if ( load_configuration( r ) != 0 )
		{
			load_configuration_failed( );	// throw error message
			*choice = 20;
			break;
		}

		// restore pointed object and variable
		n = restore_pos( r );
		if ( n != r )
		{
			*choice = 0;
			return n;
		}
	}
	else
		sensitivity_undefined( );				// throw error

break;


// Load a sensitivity analysis configuration
case 64:
	
	// check a model is already loaded
	if ( ! struct_loaded )
	{ 
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load one before trying to load a sensitivity analysis configuration.\"" );
		break;
    } 
	// check for existing sensitivity data loaded
	if ( rsense != NULL ) 
	{
		cmd( "set answer [tk_messageBox -parent . -type okcancel -icon warning -default ok -title Warning -message \"Sensitivity data already loaded\" -detail \"Press 'OK' if you want to discard the existing data before loading a new sensitivity configuration.\"]; switch -- $answer {ok {set choice 1} cancel {set choice 2}}" );
		if ( *choice == 2 )
			break;
		
		// empty sensitivity data
		empty_sensitivity( rsense ); 			// discard read data
		rsense = NULL;
		unsavedSense = false;					// nothing to save
		findexSens = 0;
	}
	// set default name and path to conf. file folder
	cmd( "set res \"%s\"", simul_name );
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	// open dialog box to get file name & folder
	cmd( " set bah [ tk_getOpenFile -parent . -title \"Load Sensitivity Analysis File\" -defaultextension \".sa\" -initialfile \"$res\" -initialdir \"$path\"  -filetypes { { {Sensitivity analysis files} {.sa} } } ]" );
	cmd( "if { [string length $bah] > 0 && ! [ fn_spaces \"$bah\" . ] } {set res $bah; set path [file dirname $res]; set res [file tail $res];set last [expr [string last .sa $res] -1];set res [string range $res 0 $last]} {set choice 2}" );
	if ( *choice == 2 )
		break;
	
	lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "path", 0 );
	
	// form full name
	if ( sens_file != NULL )
		delete sens_file;
	sens_file = new char[ strlen( lab1 ) + strlen( lab2 ) + 7 ];
	if ( strlen( lab1 ) > 0 )
		sprintf( sens_file, "%s/%s.sa", lab2, lab1 );
	else
		sprintf( sens_file, "%s.sa", lab1 );
	
	// read sensitivity file (text mode)
	f = fopen( sens_file, "rt" );
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Sensitivity Analysis file not found\"" );
		break;
	}
	
	if ( load_sensitivity( r, f ) != 0 )
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Invalid sensitivity analysis file\" -detail \"Please check if you select a valid file or recreate your sensitivity configuration.\"" );

	fclose( f );
	
break;


// Save a sensitivity analysis configuration
case 65:

	// check for existing sensitivity data loaded
	if ( rsense == NULL ) 
	{
		sensitivity_undefined( );			// throw error
		break;
	}
	
	// default file name and path
	cmd( "set res %s", simul_name );
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	// open dialog box to get file name & folder
	*choice = 0;
	cmd( "set bah [ tk_getSaveFile -parent . -title \"Save Sensitivity Analysis File\" -defaultextension \".sa\" -initialfile $res -initialdir \"$path\" -filetypes { { {Sensitivity analysis files} {.sa} } } ]" );
	cmd( "if { [ string length $bah ] > 0 } { set path [ file dirname $bah ]; set res [ file tail $bah ]; set last [ expr [ string last .sa $res ] - 1 ]; set res [ string range $res 0 $last ] } { set choice 2 }" );
	if ( *choice == 2 )
		break;
	lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "path", 0 );
	
	// form full name
	if ( sens_file != NULL )
		delete sens_file;
	sens_file = new char[ strlen( lab2 ) + strlen( lab1 ) + 7 ];
	sprintf( sens_file,"%s%s%s.sa", lab2, strlen( lab2 ) > 0 ? "/" : "", lab1 );

	// write sensitivity file (text mode)
	f = fopen( sens_file, "wt" );  // use text mode for Windows better compatibility
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Sensitivity analysis file not saved\" -detail \"Please check if the file name and path are valid.\"" );
		break;
	}
	
	if ( ! save_sensitivity( f ) )
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Sensitivity analysis file cannot be saved\" -detail \"Check if the drive or the file is set READ-ONLY.\"" );

	fclose( f );
	unsavedSense = false;			// nothing to save
	
break;

	
// export saved elements details
case 91:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration to export\" -detail \"Please load or create a configuration before trying to export the details on the elements to save.\"" );
		break;
	}

	// warn about no variable being saved
	for ( n = r; n->up != NULL; n = n->up );
	i = 0;
	count_save( n, &i );
	if ( i == 0 )
	{
		cmd( "tk_messageBox -parent . -type ok -icon warning -title Warning -message \"No variable or parameter marked to be saved\" -detail \"Please mark the variables and parameters to be saved before trying to export the details on the elements to save.\"" );
		break;
	}
	
	// default file name
	cmd( "set res %s-saved", simul_name );
	
	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	// open dialog box to get file name & folder
	*choice = 0;
	cmd( "set bah [ tk_getSaveFile -parent . -title \"Export Saved Elements Configuration as Comma-separated Text File\" -defaultextension \".csv\" -initialfile $res -initialdir \"$path\" -filetypes { { {Comma-separated files} {.csv} } } ]" );
	cmd( "if { [ string length $bah ] > 0 } { set path [ file dirname $bah ]; set res [ file tail $bah ] } { set choice 2 }" );
	if ( *choice == 2 )
		break;
	
	// form full name
	lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "path", 0 );
	sprintf( lab,"%s%s%s", lab2, strlen( lab2 ) > 0 ? "/" : "", lab1 );

	// write export file (text mode)
	f = fopen( lab, "wt" );  // use text mode for Windows better compatibility
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Saved elements configuration file not saved\" -detail \"Please check if the file name and path are valid.\"" );
		break;
	}
	
	strcpy( ch, CSV_SEP );
	
	// write .csv header & content
	fprintf( f, "Name%sType%sObject%sDescription\n", ch, ch, ch );
	get_saved( root, f, ch );
	fclose( f );
	
break;
	

// export sensitivity configuration as a .csv file
case 90:

	// check for existing sensitivity data loaded
	if ( rsense == NULL ) 
	{
		sensitivity_undefined( );			// throw error
		break;
	}
	
	// default file name
	cmd( "set res %s-limits", simul_name );
	
	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	// open dialog box to get file name & folder
	*choice = 0;
	cmd( "set bah [ tk_getSaveFile -parent . -title \"Export Sensitivity Limits as Comma-separated Text File\" -defaultextension \".csv\" -initialfile $res -initialdir \"$path\" -filetypes { { {Comma-separated files} {.csv} } } ]" );
	cmd( "if { [ string length $bah ] > 0 } { set path [ file dirname $bah ]; set res [ file tail $bah ] } { set choice 2 }" );
	if ( *choice == 2 )
		break;
	
	// form full name
	lab1 = ( char * ) Tcl_GetVar( inter, "res", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "path", 0 );
	sprintf( lab,"%s%s%s", lab2, strlen( lab2 ) > 0 ? "/" : "", lab1 );

	// write export file (text mode)
	f = fopen( lab, "wt" );  // use text mode for Windows better compatibility
	if ( f == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Sensitivity limits file not saved\" -detail \"Please check if the file name and path are valid.\"" );
		break;
	}
	
	// write .csv header
	strcpy( ch, CSV_SEP );
	fprintf( f, "Name%sType%sLag%sFormat%sValue%sMinimum%sMaximum%sDescription\n", ch, ch, ch, ch, ch, ch, ch );
	
	// write data
	get_sa_limits( r, f, ch );
	
	fclose( f );
	
break;


// Show sensitivity analysis configuration
case 66:

	*choice = 50;

	// check for existing sensitivity data loaded
	if ( rsense == NULL ) 
	{
		cmd( "tk_messageBox -parent . -type ok -icon warning -title Warning -message \"There is no sensitivity data to show\"" );
		break;
	}
	
	// print data to log window
	plog( "\n\nVariables and parameters set for sensitivity analysis :\n" );
	for ( cs = rsense; cs != NULL; cs = cs->next )
	{
		if ( cs->param == 1 )
			plog( "Param: %s\\[%s\\]\t#%d:\t", "", cs->label, cs->integer ? "int" : "flt", cs->nvalues );
		else
			plog( "Var: %s(-%d)\\[%s\\]\t#%d:\t", "", cs->label, cs->lag+1, cs->integer ? "int" : "flt", cs->nvalues );

		for ( i = 0; i < cs->nvalues; ++i )
			plog( "%g\t", "highlight", cs->v[ i ] );
		plog( "\n" );
	}
	
break;


// Remove sensitivity analysis configuration
case 67:

	*choice = 0;

	// check for existing sensitivity data loaded
	if ( rsense == NULL ) 
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No sensitivity data to remove\"" );
		break;
	}
	
	if ( ! discard_change( true, true ) )	// unsaved configuration?
		break;

	// empty sensitivity data
	empty_sensitivity( rsense ); 			// discard read data
	plog( "\nSensitivity data removed.\n" );
	rsense = NULL;
	unsavedSense = false;					// nothing to save
	findexSens = 0;
	
break;


// Create batch for multi-runs jobs and optionally run it
case 68:

	// check a model is already loaded
	if ( ! struct_loaded )
		findexSens = 0;									// no sensitivity created
	else
		if ( ! discard_change( false ) )				// unsaved configuration?
			break;

	// check for existing NW executable
	sprintf( ch, "%s/lsd_gnuNW", exec_path );			// form full executable name
	cmd( "if { $tcl_platform(platform) == \"windows\" } { set choice 1 } { set choice 0 }" );
	if ( *choice == 1 )
		strcat( ch, ".exe" );							// add Windows ending

	if ( ( f = fopen( ch, "rb" ) ) == NULL ) 
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Executable file 'lsd_gnuNW' not found\" -detail \"Please create the required executable file using the option 'Model'/'Generate 'No Window' Version' in LMM menu.\"" );
		break;
	}
	fclose( f );
	
	// check if NW executable file is older than running executable file
	sprintf( lab_old, "%s/%s", exec_path, exec_file );	// form full exec name

	// get OS info for files
	if ( stat( ch, &stExe ) == 0 && stat( lab_old, &stMod ) == 0 )
	{
		if ( difftime( stExe.st_mtime, stMod.st_mtime ) < 0 )
		{
			cmd( "set answer [tk_messageBox -parent . -title Warning -icon warning -type okcancel -default cancel -message \"Old executable file\" -detail \"The existing No Window executable file is older than the current version of the current executable.\n\nPress 'OK' to continue anyway or 'Cancel' to abort. Please recompile the model using the option 'Model'/'Generate 'No Window' Version' in LMM menu.\"]; if [ string equal $answer ok ] { set choice 1 } { set choice 2 }" );
			if ( *choice == 2 )
				break;
		}
	}
	
	// check if serial sensitivity configuration was just created
	*choice = 0;
	if ( findexSens > 0 )
		cmd( "set answer [ tk_messageBox -parent . -type yesnocancel -icon question -default yes -title \"Create Batch\" -message \"Script/batch created\" -detail \"A sequential sensitivity set of configuration files was just created and can be used to create the script/batch.\n\nPress 'Yes' to confirm or 'No' to select a different set of files.\" ]; switch -- $answer { yes { set choice 1 } no { set choice 0 } cancel { set choice 2 } }" ); 
	if ( *choice == 2 )
		break;
	
	// get configuration files to use
	if ( *choice == 1 )							// use current configuration files
	{
		if ( strlen( path ) == 0 || strlen( simul_name ) == 0 )
		{
			cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Invalid simulation folder or name\" -detail \"Please try again.\"" );
			findexSens = 0;						// no sensitivity created
			break;
		}
		
		ffirst = fSeq = 1;
		fnext = findexSens;
		findexSens = 0;
		strncpy( out_file, simul_name, MAX_PATH_LENGTH - 1 );
		strncpy( out_dir, path, MAX_PATH_LENGTH - 1 );
		Tcl_SetVar( inter, "res", simul_name, 0 );
		Tcl_SetVar( inter, "path", path, 0 );
	}
	else										// ask for first configuration file
	{
		cmd( "set answer [ tk_messageBox -parent . -type yesnocancel -icon question -default yes -title \"Create Batch\" -message \"Select sequence of configuration files?\" -detail \"Press 'Yes' to choose the first file of the continuous sequence (format: 'name_NNN.lsd') or 'No' to select a different set of files (use 'Ctrl' to pick multiple files).\" ]; switch -- $answer { yes { set choice 1 } no { set choice 0 } cancel { set choice 2 } }" ); 
		if ( *choice == 2 )
			break;
		else
			fSeq = *choice;
		
		if ( fSeq && strlen( simul_name ) > 0 )	// default name
			cmd( "set res \"%s_1.lsd\"", simul_name );
		else
			cmd( "set res \"\"" );
		
		cmd( "set path \"%s\"", path );
		if ( strlen( path ) > 0 )
			cmd( "cd \"$path\"" );
		// open dialog box to get file name & folder
		if ( fSeq )								// file sequence?
		{
			cmd( "set bah [ tk_getOpenFile -parent . -title \"Load First Configuration File\" -defaultextension \".lsd\" -initialfile $res -initialdir \"$path\" -filetypes { { {LSD model files} {.lsd} } } -multiple no ]" );
			cmd( "if { [ string length $bah ] > 0 && ! [ fn_spaces \"$bah\" . ] } { set res $bah; set path [ file dirname $res ]; set res [ file tail $res ]; set last [ expr [ string last .lsd $res ] - 1 ]; set res [ string range $res 0 $last ]; set numpos [ expr [ string last _ $res ] + 1 ]; if { $numpos > 0 } { set choice [ expr [ string range $res $numpos end ] ]; set res [ string range $res 0 [ expr $numpos - 2 ] ] } { plog \"\nInvalid file name for sequential set: $res\n\"; set choice 0 } } { set choice 0 }" );
			if ( *choice == 0 )
				break;
			ffirst = *choice;
			strncpy( out_file, ( char * ) Tcl_GetVar( inter, "res", 0 ), MAX_PATH_LENGTH - 1 );
			strncpy( out_dir, ( char * ) Tcl_GetVar( inter, "path", 0 ), MAX_PATH_LENGTH - 1 );
			f = NULL;
			do									// search for all sequential files
			{
				if ( strlen( out_dir ) == 0 )			// default path
					sprintf( lab, "%s_%d.lsd", out_file, ( *choice )++ );
				else
					sprintf( lab, "%s/%s_%d.lsd", out_dir, out_file, ( *choice )++ );
				if ( f != NULL ) 
					fclose( f );
				f = fopen( lab, "r" );
			}
			while ( f != NULL );
			fnext = *choice - 1;
		}
		else									// bunch of files?
		{
			cmd( "set bah [ tk_getOpenFile -parent . -title \"Load Configuration Files\" -defaultextension \".lsd\" -initialdir \"$path\" -filetypes { { {LSD model files} {.lsd} } } -multiple yes ]" );
			cmd( "set choice [ llength $bah ]; if { $choice > 0 && ! [ fn_spaces [ lindex $bah 0 ] . 1 ] } { set res [ lindex $bah 0 ]; set path [ file dirname $res ]; set res [ file tail $res ]; set last [ expr [ string last .lsd $res ] - 1 ]; set res [ string range $res 0 $last ]; set numpos [ expr [ string last _ $res ] + 1 ]; if { $numpos > 0 } { set res [ string range $res 0 [ expr $numpos - 2 ] ] } }" );
			if ( *choice == 0 )
				break;
			ffirst = 1;
			fnext = *choice + 1;
			strncpy( out_dir, ( char * ) Tcl_GetVar( inter, "path", 0 ), MAX_PATH_LENGTH - 1 );
		}
	}

	Tcl_LinkVar( inter, "no_res", ( char * ) & no_res, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "natBat", ( char * ) & natBat, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "docsv", ( char * ) & docsv, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "dozip", ( char * ) & dozip, TCL_LINK_BOOLEAN );
	cmd( "set cores %d", max_threads );
	cmd( "set threads 1" );
	
	// confirm number of cores to use
	cmd( "set res2 $res" );
	cmd( "newtop .s \"Parallel Batch\" { set choice 2 }" );

	cmd( "frame .s.t" );
	cmd( "label .s.t.l -text \"Batch file base name\"" );
	cmd( "entry .s.t.e -width 20 -textvariable res2 -justify center" );
	cmd( "pack .s.t.l .s.t.e" );
		
	cmd( "frame .s.c" );
	cmd( "label .s.c.l -text \"Number of parallel\nLSD processes\"" );
	cmd( "entry .s.c.e -width 5 -justify center -validate focusout -vcmd { if [ string is integer -strict %%P ] { set cores %%P; return 1 } { %%W delete 0 end; %%W insert 0 $cores; return 0 } } -invcmd { bell } -justify center" );
	cmd( ".s.c.e insert 0 $cores" ); 
	cmd( "label .s.c.w -text \"(a number higher than the\nnumber of processors/cores\nis not recommended)\"" );
	cmd( "pack .s.c.l .s.c.e .s.c.w" );
	
	cmd( "frame .s.p" );
	cmd( "label .s.p.l -text \"Number of threads\nper LSD process\"" );
	cmd( "entry .s.p.e -width 5 -justify center -validate focusout -vcmd { if [ string is integer -strict %%P ] { set threads %%P; return 1 } { %%W delete 0 end; %%W insert 0 $threads; return 0 } } -invcmd { bell } -justify center" );
	cmd( ".s.p.e insert 0 $threads" ); 
	cmd( "label .s.p.w -text \"(a number higher than 1\nis only useful when parallel\ncomputation is enabled)\"" );
	cmd( "pack .s.p.l .s.p.e .s.p.w" );
	
	cmd( "frame .s.o" );
	cmd( "checkbutton .s.o.nores -text \"Skip generating results files\" -variable no_res" );
	cmd( "checkbutton .s.o.n -text \"Native batch format\" -variable natBat" );
	cmd( "checkbutton .s.o.dozip -text \"Generate zipped files\" -variable dozip" );
	cmd( "checkbutton .s.o.docsv -text \"Comma-separated text format (.csv)\" -variable docsv" );
	cmd( "pack .s.o.nores .s.o.n .s.o.dozip .s.o.docsv -anchor w" );
	
	cmd( "pack .s.t .s.c .s.p .s.o -padx 5 -pady 5" );

	cmd( "okhelpcancel .s b { set choice 1 } { LsdHelp menurun.html#parallel } { set choice 2 }" );
	cmd( "bind .s.c.e <KeyPress-Return> { .s.b.ok invoke }" );
	
	cmd( "showtop .s" );
	cmd( "focus .s.c.e" );
	cmd( ".s.c.e selection range 0 end" );
	
	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );
	
	cmd( "set cores [ .s.c.e get ]" );
	cmd( "set threads [ .s.p.e get ]" );
	
	cmd( "destroytop .s" );
	
	Tcl_UnlinkVar( inter, "natBat" );
	Tcl_UnlinkVar( inter, "no_res" );
	Tcl_UnlinkVar( inter, "docsv" );
	Tcl_UnlinkVar( inter, "dozip" );

	if ( *choice == 2 )
		break;
	
	get_int( "cores", & param );
	get_int( "threads", & nature );
	if ( param < 1 || param > 64 ) 
		param = max_threads;
	
	get_int( "threads", & nature );
	if ( nature < 1 || nature > 64 ) 
		nature = max_threads;
	
	strncpy( out_bat, ( char * ) Tcl_GetVar( inter, "res2", 0 ), MAX_PATH_LENGTH - 1 );
	
	// select batch format & create batch file
	cmd( "if { $tcl_platform(platform) == \"windows\" } { if { $natBat == 1 } { set choice 1 } { set choice 2 } } { if { $natBat == 1 } { set choice 3 } { set choice 4 } }" );
	if ( fSeq )
		if ( *choice == 1 || *choice == 4 )
			sprintf( lab, "%s/%s_%d_%d.bat", out_dir, out_bat, ffirst, fnext - 1 );
		else
			sprintf( lab, "%s/%s_%d_%d.sh", out_dir, out_bat, ffirst, fnext - 1 );
	else
		if ( *choice == 1 || *choice == 4 )
			sprintf( lab, "%s/%s.bat", out_dir, out_bat );
		else
			sprintf( lab, "%s/%s.sh", out_dir, out_bat );
		
	f = fopen( lab, "wb" );						// binary mode to bypass CR/LF handling
		
	if ( *choice == 1 || *choice == 4 )			// Windows header
	{
		// convert to Windows folder separators (\)
		for ( i = 0; i < strlen( ch ); ++i ) 
			if ( ch[ i ] == '/' ) 
				ch[ i ] = '\\';
		win_dir[ MAX_PATH_LENGTH - 1 ] = '\0';
		strcpy( win_dir, out_dir );
		for ( i = 0; i < strlen( win_dir ); ++i ) 
			if ( win_dir[ i ] == '/' ) 
				win_dir[ i ]='\\';
		
		fprintf( f, "@echo off\nrem Batch generated by LSD\r\n" );
		fprintf( f, "echo Processing %d configuration files in up to %d parallel processes...\r\n", fnext - ffirst, param );
		fprintf( f, "if \"%%~1\"==\"\" (set LSD_EXEC=\"%s\") else (set LSD_EXEC=\"%%~1\")\r\n", ch );
		fprintf( f, "if \"%%~2\"==\"\" (set LSD_CONFIG_PATH=\"%s\") else (set LSD_CONFIG_PATH=\"%%~2\")\r\n", win_dir );
		fprintf( f, "set LSD_EXEC=%%LSD_EXEC:\"=%%\r\n" );
		fprintf( f, "set LSD_CONFIG_PATH=%%LSD_CONFIG_PATH:\"=%%\r\n" );
		fprintf( f, "echo LSD executable: %%LSD_EXEC%%\r\n" );
		fprintf( f, "echo Configuration path: %%LSD_CONFIG_PATH%%\r\n" );
		fprintf( f, "echo Use %s.bat LSD_EXEC CONFIG_PATH to change defaults\r\n", out_bat );
	}
	else										// Unix header
	{
		if ( ! natBat )							// Unix in Windows?
		{
			if ( strchr( ch, ':' ) != NULL )			// remove Windows drive letter
			{
				strcpy( msg, strchr( ch, ':' ) + 1 );
				strcpy( ch, msg );
			}
			if ( strchr( out_dir, ':' ) != NULL )		// remove Windows drive letter
			{
				strcpy( msg, strchr( out_dir, ':' ) + 1 );
				strcpy( out_dir, msg );
			}

			if ( ( lab1 = strstr( ch, ".exe" ) ) != NULL )	// remove Windows extension, if present
				lab1[ 0 ]='\0';
			else
				if ( ( lab1 = strstr( ch, ".EXE" ) ) != NULL ) 
					lab1[ 0 ]='\0';
		}
		
		// set background low priority in servers (cores/jobs > 8)
		if ( nature > 8 || ( param > 8 && fnext - ffirst > 8 ) )
		{
			sprintf( msg, "nice %s", ch );
			strcpy( ch, msg );
		}
		
		fprintf( f, "#!/bin/bash\n# Script generated by LSD\n" );
		fprintf( f, "echo \"Processing %d configuration files in up to %d parallel processes...\"\n", fnext - ffirst, param );
		fprintf( f, "if [ \"$1\" = \"\" ]; then LSD_EXEC=\"%s\"; else LSD_EXEC=\"$1\"; fi\n", ch );
		fprintf( f, "if [ \"$2\" = \"\" ]; then LSD_CONFIG_PATH=\"%s\"; else LSD_CONFIG_PATH=\"$2\"; fi\n", out_dir );
		fprintf( f, "echo \"LSD executable: $LSD_EXEC\"\n" );
		fprintf( f, "echo \"Configuration path: $LSD_CONFIG_PATH\"\n" );
		fprintf( f, "echo \"Use %s.sh LSD_EXEC CONFIG_PATH to change default paths\"\n", out_bat );
	}
	
	if ( fSeq && ( fnext - ffirst ) > param )	// if possible, work in blocks
	{
		num = ( fnext - ffirst ) / param;		// base number of cases per core
		sl = ( fnext - ffirst ) % param;		// remaining cases per core
		for ( i = ffirst, j = 1; j <= param; ++j )	// allocates files by the number of cores
		{
			if ( *choice == 1 || *choice == 4 )	// Windows
				fprintf( f, "start \"LSD Process %d\" /B \"%%LSD_EXEC%%\" -c %d -f \"%%LSD_CONFIG_PATH%%\\%s\" -s %d -e %d %s %s %s 1> \"%%LSD_CONFIG_PATH%%\\%s_%d.log\" 2>&1\r\n", j, nature, out_file, i, j <= sl ? i + num : i + num - 1, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file, j );
			else								// Unix
				fprintf( f, "$LSD_EXEC -c %d -f \"$LSD_CONFIG_PATH\"/%s -s %d -e %d %s %s %s > \"$LSD_CONFIG_PATH\"/%s_%d.log 2>&1 &\n", nature, out_file, i, j <= sl ? i + num : i + num - 1, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file, j );
			j <= sl ? i += num + 1 : i += num;
		}
	}
	else										// if not, do one by one
		for ( i = ffirst, j = 1; i < fnext; ++i, ++j )
			if ( fSeq )
				if ( *choice == 1 || *choice == 4 )	// Windows
					fprintf( f, "start \"LSD Process %d\" /B \"%%LSD_EXEC%%\" -c %d -f \"%%LSD_CONFIG_PATH%%\\%s_%d.lsd\" %s %s %s 1> \"%%LSD_CONFIG_PATH%%\\%s_%d.log\" 2>&1\r\n", j, nature, out_file, i, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file, i );
				else								// Unix
					fprintf( f, "$LSD_EXEC -c %d -f \"$LSD_CONFIG_PATH\"/%s_%d.lsd %s %s %s > \"$LSD_CONFIG_PATH\"/%s_%d.log 2>&1 &\n", nature, out_file, i, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file, i );
			else
			{	// get the selected file names, one by one
				cmd( "set res3 [lindex $bah %d]; set res3 [file tail $res3]; set last [expr [string last .lsd $res3] - 1]; set res3 [string range $res3 0 $last]", j - 1  );
				strncpy( out_file, ( char * ) Tcl_GetVar( inter, "res3", 0 ), MAX_PATH_LENGTH - 1 );
				
				if ( *choice == 1 || *choice == 4 )	// Windows
					fprintf( f, "start \"LSD Process %d\" /B \"%%LSD_EXEC%%\" -c %d -f \"%%LSD_CONFIG_PATH%%\\%s.lsd\" %s %s %s 1> \"%%LSD_CONFIG_PATH%%\\%s.log\" 2>&1\r\n", j, nature, out_file, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file );
				else								// Unix
					fprintf( f, "$LSD_EXEC -c %d -f \"$LSD_CONFIG_PATH\"/%s.lsd %s %s %s > \"$LSD_CONFIG_PATH\"/%s.log 2>&1 &\n", nature, out_file, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", out_file );
			}
	
	if ( fSeq )
		if ( *choice == 1 || *choice == 4 )	// Windows closing
		{
			fprintf( f, "echo %d log files being generated: %s_1.log to %s_%d.log .\r\n", j - 1, out_file, out_file, j - 1 );
			fclose( f );
		}
		else								// Unix closing
		{
			fprintf( f, "echo \"%d log files being generated: %s_1.log to %s_%d.log .\"\n", j - 1, out_file, out_file, j - 1 );
			fclose( f );
			chmod( lab, ACCESSPERMS );		// set executable perms
		}
	else
		if ( *choice == 1 || *choice == 4 )	// Windows closing
		{
			fprintf( f, "echo %d log files being generated.\r\n", j - 1 );
			fclose( f );
		}
		else								// Unix closing
		{
			fprintf( f, "echo \"%d log files being generated.\"\n", j - 1 );
			fclose( f );
			chmod( lab, ACCESSPERMS );		// set executable perms
		}
		
	plog( "\nParallel batch file created: %s", "", lab );
	
	if ( ! natBat )
		break;

	// ask if script/batch should be executed right away
	cmd( "set answer [ tk_messageBox -parent . -type yesno -icon question -default no -title \"Run Batch\" -message \"Run created script/batch?\" -detail \"The script/batch for running the configuration files was created. Press 'Yes' if you want to start the script/batch as separated processes now.\" ]; switch -- $answer { yes { set choice 1 } no { set choice 2 } }" ); 
	if ( *choice == 2 )
		break;

	// start the job
	cmd( "set oldpath [pwd]" );
	cmd( "set path \"%s\"", out_dir );
	if ( strlen( out_dir ) > 0 )
		cmd( "cd $path" );

	cmd( "if { $tcl_platform(platform) == \"windows\" } { set choice 1 } { set choice 0 }" );
	if ( *choice == 1 )						// Windows?
		cmd( "exec %s &", lab );
	else									// Unix
		cmd( "exec %s &", lab );

	plog( "\nParallel batch file started: %s", "", lab );
	cmd( "tk_messageBox -parent . -type ok -icon info -title \"Run Batch\" -message \"Script/batch started\" -detail \"The script/batch was started in separated process(es). The results and log files are being created in the folder:\\n\\n$path\\n\\nCheck the '.log' files to see the results or use the command 'tail  -F  <name>.log' in a shell/command prompt to follow simulation execution (there is one log file per assigned process/core).\"" );
	
	cmd( "set path $oldpath; cd $path" );
	
break;


// Start NO WINDOW job as a separate background process
case 69:

	// check a model is already loaded
	if ( ! struct_loaded )
	{ 
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to start a 'No Window' batch.\"" );
		break;
	}

	// check for existing NW executable
	sprintf( lab, "%s/lsd_gnuNW", exec_path );			// form full executable name
	cmd( "if {$tcl_platform(platform) == \"windows\"} {set choice 1} {set choice 0}" );
	if ( *choice == 1 )
		strcat( lab, ".exe" );							// add Windows ending

	if ( ( f = fopen( lab, "rb" ) ) == NULL ) 
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Executable file 'lsd_gnuNW' not found\" -detail \"Please create the required executable file using the option 'Model'/'Generate 'No Window' Version' in LMM.\"" );
		break;
	}
	fclose( f );
	
	// check if NW executable file is older than running executable file
	sprintf( lab_old, "%s/%s", exec_path, exec_file );	// form full exec name

	// get OS info for files
	if ( stat( lab, &stExe ) == 0 && stat( lab_old, &stMod ) == 0 )
	{
		if ( difftime( stExe.st_mtime, stMod.st_mtime ) < 0 )
		{
			cmd( "set answer [tk_messageBox -parent . -title Warning -icon warning -type okcancel -default cancel -message \"Old executable file\" -detail \"The existing No Window executable file is older than the current version of the current executable.\n\nPress 'OK' to continue anyway or 'Cancel' to abort. Please recompile the model using the option 'Model'/'Generate 'No Window' Version' in LMM menu.\"]; if [ string equal $answer ok ] { set choice 1 } { set choice 2 }" );
			if ( *choice == 2 )
				break;
		}
	}
	
	Tcl_LinkVar( inter, "no_res", ( char * ) & no_res, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "docsv", ( char * ) & docsv, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "dozip", ( char * ) & dozip, TCL_LINK_BOOLEAN );
	Tcl_LinkVar( inter, "overwConf", ( char * ) & overwConf, TCL_LINK_BOOLEAN );

	// Only ask to overwrite configuration if there are changes
	overwConf = unsaved_change( ) ? true : false;

	add_to_tot = false;
	
	cmd( "set simNum %d", sim_num );
	cmd( "set firstFile \"%s_%d\"", simul_name, seed );
	cmd( "set lastFile \"%s_%d\"", simul_name, seed + sim_num - 1 );
	cmd( "set totFile \"%s\"", simul_name );
	cmd( "set resExt %s", docsv ? "csv" : "res" );
	cmd( "set totExt %s", docsv ? "csv" : "tot" );
	cmd( "set zipExt %s", dozip ? ".gz" : "" );

	// confirm overwriting current configuration
	cmd( "set b .batch" );
	cmd( "newtop $b \"Start Batch\" { set choice 2 }" );

	cmd( "frame $b.f1" );
	cmd( "label $b.f1.l -text \"Model configuration\"" );
	cmd( "label $b.f1.w -text \"%s\" -fg red", simul_name );
	cmd( "pack $b.f1.l $b.f1.w" );
	
	cmd( "frame $b.f2" );
	
	cmd( "frame $b.f2.t" );
	cmd( "label $b.f2.t.l -text \"Time steps:\"" );
	cmd( "label $b.f2.t.w -text \"%d\" -fg red", max_step );
	cmd( "pack $b.f2.t.l $b.f2.t.w -side left -padx 2" );
	
	cmd( "frame $b.f2.n" );
	cmd( "label $b.f2.n.l -text \"Number of simulations:\"" );
	cmd( "label $b.f2.n.w -text \"%d\" -fg red", sim_num );
	cmd( "pack $b.f2.n.l $b.f2.n.w -side left -padx 2" );
	cmd( "pack $b.f2.t $b.f2.n" );

	cmd( "frame $b.f3" );
	cmd( "label $b.f3.l -text \"Results file(s)\"" );
	
	if ( sim_num > 1 )	// multiple runs case
	{
		cmd( "frame $b.f3.w" );
		
		cmd( "frame $b.f3.w.l1" );
		cmd( "label $b.f3.w.l1.l -text \"from:\"" );
		cmd( "label $b.f3.w.l1.w -fg red -text \"$firstFile.$resExt$zipExt\"" );
		cmd( "pack $b.f3.w.l1.l $b.f3.w.l1.w -side left -padx 2" );
		
		cmd( "frame $b.f3.w.l2" );
		cmd( "label $b.f3.w.l2.l -text \"to:\"" );
		cmd( "label $b.f3.w.l2.w -fg red -text \"$lastFile.$resExt$zipExt\"" );
		cmd( "pack $b.f3.w.l2.l $b.f3.w.l2.w -side left -padx 2" );
		
		cmd( "pack $b.f3.w.l1 $b.f3.w.l2" );
	}
	else				// single run case
		cmd( "label $b.f3.w -fg red -text \"$firstFile.$resExt$zipExt\"" );

	cmd( "pack $b.f3.l $b.f3.w" );

	cmd( "frame $b.f4" );
	cmd( "label $b.f4.l1 -text \"Totals file (last steps)\"" );
	cmd( "label $b.f4.l2 -fg red -text \"$totFile.$totExt$zipExt\"" );
	
	cmd( "set choice [ expr [ file exists \"%s%s$firstFile.$resExt$zipExt\" ] || [ file exists \"%s%s$totFile.$totExt$zipExt\" ] ]", path, strlen( path ) > 0 ? "/" : "", path, strlen( path ) > 0 ? "/" : "" );
	cmd( "label $b.f4.l3 -text \"\n\"", *choice ? "(WARNING: existing files in destination\nfolder will be overwritten)" : "" );
	cmd( "pack $b.f4.l1 $b.f4.l2 $b.f4.l3" );
	
	cmd( "frame $b.f5" );
	cmd( "checkbutton $b.f5.nores -text \"Skip generating results files\" -variable no_res" );
	cmd( "checkbutton $b.f5.dozip -text \"Generate zipped files\" -variable dozip -command { \
			if $dozip { \
				set zipExt .gz \
			} else { \
				set zipExt \"\" \
			}; \
			if { $simNum > 1 } { \
				$b.f3.w.l1.w configure -text \"$firstFile.$resExt$zipExt\"; \
				$b.f3.w.l2.w configure -text \"$lastFile.$resExt$zipExt\"; \
			} else { \
				$b.f3.w configure -text \"$firstFile.$resExt$zipExt\"; \
			}; \
			$b.f4.l2 configure -text \"$totFile.$totExt$zipExt\"; \
			if { [ file exists \"%s%s$firstFile.$resExt$zipExt\" ] || [ file exists \"%s%s$totFile.$totExt$zipExt\" ] } { \
				$b.f4.l3 configure -text \"(WARNING: existing files in destination\nfolder will be overwritten)\" \
			} else { \
				$b.f4.l3 configure -text \"\n\" \
			} \
		}", path, strlen( path ) > 0 ? "/" : "", path, strlen( path ) > 0 ? "/" : "" );
	cmd( "checkbutton $b.f5.docsv -text \"Comma-separated text format (.csv)\" -variable docsv -command { \
			if $docsv { set resExt csv; set totExt csv } { \
				set resExt res; \
				set totExt tot \
			}; \
			if { $simNum > 1 } { \
				$b.f3.w.l1.w configure -text \"$firstFile.$resExt$zipExt\"; \
				$b.f3.w.l2.w configure -text \"$lastFile.$resExt$zipExt\"; \
			} else { \
				$b.f3.w configure -text \"$firstFile.$resExt$zipExt\"; \
			}; \
			$b.f4.l2 configure -text \"$totFile.$totExt$zipExt\"; \
			if { [ file exists \"%s%s$firstFile.$resExt$zipExt\" ] || [ file exists \"%s%s$totFile.$totExt$zipExt\" ] } { \
				$b.f4.l3 configure -text \"(WARNING: existing files in destination\nfolder will be overwritten)\" \
			} else { \
				$b.f4.l3 configure -text \"\n\" \
			} \
		}", path, strlen( path ) > 0 ? "/" : "", path, strlen( path ) > 0 ? "/" : "" );
	cmd( "checkbutton $b.f5.tosave -text \"Update configuration file\" -variable overwConf" );
	cmd( "pack $b.f5.nores $b.f5.dozip $b.f5.docsv %s -anchor w", overwConf ? "$b.f5.tosave" : "" );
	
	cmd( "pack $b.f1 $b.f2 $b.f3 $b.f4 $b.f5 -padx 5 -pady 5" );
		
	cmd( "okhelpcancel $b b { set choice 1 } { LsdHelp menurun.html#batch } { set choice 2 }" );
	
	cmd( "showtop $b" );
	
	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );
	
	cmd( "destroytop .batch" );
	Tcl_UnlinkVar( inter, "no_res" );
	Tcl_UnlinkVar( inter, "docsv" );
	Tcl_UnlinkVar( inter, "dozip" );
	Tcl_UnlinkVar( inter, "overwConf" );

	if ( *choice == 2 )
	{
		*choice = 0;
		break;
	}

	for ( n = r; n->up != NULL; n = n->up );
	blueprint->empty( );		    // update blueprint to consider last changes
	set_blueprint( blueprint, n );
	
	if ( overwConf )				// save if needed
		if ( ! save_configuration( r ) )
		{
			cmd( "set answer [ tk_messageBox -parent . -type okcancel -default cancel -icon warning -title Warning -message \"File '%s.lsd' cannot be saved\" -detail \"Check if the drive or the file is set READ-ONLY. Press 'OK' to run the simulation without saving the initialization file.\" ]; switch -- $answer { ok { set choice 1 } cancel { set choice 2 } } ", simul_name  );
			if ( *choice == 2 )
			{
				*choice = 0;
				break;
			}
		}

	// start the job
	cmd( "set oldpath [pwd]" );
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd $path" );

	if ( *choice == 1 )							// Windows?
		cmd( "exec %s -f %s %s %s %s >& %s.log  &", lab, struct_file, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", simul_name );
	else										// Unix
		cmd( "exec nice %s -f %s %s %s %s >& %s.log  &", lab, struct_file, no_res ? "-r" : "", docsv ? "-t" : "", dozip ? "" : "-z", simul_name );

	cmd( "tk_messageBox -parent . -type ok -icon info -title \"Start 'No Window' Batch\" -message \"Script/batch started\" -detail \"The current configuration was started as a 'No Window' background job. The results files are being created in the folder:\\n\\n$path\\n\\nCheck the '%s.log' file to see the results or use the command 'tail  -F  %s.log' in a shell/command prompt to follow simulation execution.\"", simul_name, simul_name );
	
	cmd( "set path $oldpath; cd $path" );
break;


// Load network
case 88:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to load a network structure file.\"" );
		break;
	}

	cmd( "set bah \"%s\"", simul_name );

	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	cmd( "set bah [ tk_getOpenFile -parent . -title \"Open Network Structure File\"  -defaultextension \".net\" -initialdir \"$path\" -initialfile \"$bah.net\" -filetypes { { {Pajek network files} {.net} } { {All files} {*} } } ]" );
	*choice = 0;
	cmd( "if { [ string length $bah ] > 0 && ! [ fn_spaces \"$bah\" . ] } { set netPath [ file dirname $bah ]; set netFile [ file tail $bah ]; set posExt [ string last . $netFile ]; if { $posExt >= 0 } { set netExt [ string range $netFile [ expr $posExt + 1 ] end ]; set netFile [ string range $netFile 0 [ expr $posExt - 1 ] ] } { set netExt \"\" } } { set choice 2 }" );
	if ( *choice == 2 )
		break;

	lab1 = ( char * ) Tcl_GetVar( inter, "netPath", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "netFile", 0 );
	lab3 = ( char * ) Tcl_GetVar( inter, "netExt", 0 );
	if ( strlen( lab2 ) == 0 )
		break;

	// try to read the object name from network file (in first comment line)
	snprintf( lab, MAX_PATH_LENGTH, "%s%s%s%s%s", lab1, foldersep( lab1 ), lab2, strlen( lab3 ) == 0 ? "" : ".", lab3 );
	strcpy( lab_old, "(none)" );			// no object name yet
	if ( ( f = fopen( lab, "r" ) ) )
	{
		fgets( ch, MAX_PATH_LENGTH, f );	// get first line
		sscanf( ch, "%% %99s", lab_old );	// get first string after the comment char
		fclose( f );
	}
	else
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid file or directory\" -detail \"Please check if an existing network structure file (Pajek .net format) was selected.\"" );
		break;
	}

	cmd( "set TT .objs" );
	cmd( "newtop $TT \"Load Network\" { set choice 2 }" );

	cmd( "frame $TT.l" );
	cmd( "label $TT.l.l -text \"Suggested object:\"" );
	cmd( "label $TT.l.n -fg red -text \"%s\"", lab_old );
	cmd( "pack $TT.l.l $TT.l.n -padx 2" );

	cmd( "frame $TT.v" );
	cmd( "label $TT.v.l -text \"Object representing\nthe network nodes\"" );

	cmd( "frame $TT.v.t" );
	cmd( "scrollbar $TT.v.t.v_scroll -command \"$TT.v.t.lb yview\"" );
	cmd( "listbox $TT.v.t.lb -width 25 -selectmode single -yscroll \"$TT.v.t.v_scroll set\"" );
	cmd( "pack $TT.v.t.lb $TT.v.t.v_scroll -side left -fill y" );
	cmd( "mouse_wheel $TT.v.t.lb" );
	insert_object( "$TT.v.t.lb", root );
	cmd( "pack $TT.v.l $TT.v.t" );

	cmd( "pack $TT.l $TT.v -padx 5 -pady 5" );

	cmd( "okcancel $TT b { set choice 1 } { set choice 2 }" );	// insert ok button
	cmd( "bind $TT.v.t.lb <Double-1> { set choice 1 }" );

	cmd( "showtop $TT" );

	cmd( "set cur 0" );
	if ( ! strcmp( lab_old, "(none)" ) )
	{
		if ( r != NULL )
			strcpy( lab_old, r->label );
		else
			strcpy( lab_old, "" );
	}
	cmd( "for { set i 0 } { $i < [ $TT.v.t.lb size ] } { incr i } { if [ string equal [ $TT.v.t.lb get $i ] %s ] { set cur $i; break } }", lab_old );
	cmd( "$TT.v.t.lb selection set $cur" );
	cmd( "focus $TT.v.t.lb" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set nodeObj [ .objs.v.t.lb get [ .objs.v.t.lb curselection ] ]" );
	cmd( "destroytop .objs" );

	if ( *choice == 2 )
		break;

	lab4 = ( char * ) Tcl_GetVar( inter, "nodeObj", 0 );

	plog( "\nLoading network on object '%s' from file %s%s%s%s%s...\n", "", lab4, lab1, foldersep( lab1 ), lab2, strlen( lab3 ) == 0 ? "" : ".", lab3 );

	cur = root->search( lab4 );
	if ( cur != NULL && cur->up != NULL )
	{
		nLinks = cur->up->read_file_net( lab4, lab1, lab2, -1, lab3 );
		if ( nLinks == 0 )
		{
			cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid file or object\" -detail \"Please check the file contents for a valid Pajek network structure file (Pajek .net format) and make sure you select a valid object for attributing the network's nodes role.\"" );
			plog( "Error: No network links created\n" );
		}
		else
			plog( " %ld network links created\n", "", nLinks );
	}
	else
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid object\" -detail \"Please make sure you select a valid object for attributing the network's nodes role.\"" );
		plog( "Error: No network links created\n" );
	}
	
break;


// Save network
case 89:

	if ( ! struct_loaded )
	{
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No configuration loaded\" -detail \"Please load or create one before trying to save a network structure file.\"" );
		break;
	}

	cmd( "set TT .objs" );
	cmd( "newtop $TT \"Save Network\" { set choice 2 }" );

	cmd( "frame $TT.v" );
	cmd( "label $TT.v.l -text \"Object containing\nthe network nodes\"" );

	cmd( "frame $TT.v.t" );
	cmd( "scrollbar $TT.v.t.v_scroll -command \"$TT.v.t.lb yview\"" );
	cmd( "listbox $TT.v.t.lb -width 25 -selectmode single -yscroll \"$TT.v.t.v_scroll set\"" );
	cmd( "pack $TT.v.t.lb $TT.v.t.v_scroll -side left -fill y" );
	cmd( "mouse_wheel $TT.v.t.lb" );

	insert_object( "$TT.v.t.lb", root, true );
	cmd( "set numNets [ $TT.v.t.lb size ]" );
	if ( get_int( "numNets" ) == 0 )
	{
		cmd( "destroytop .objs" );
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No network object found\" -detail \"Please make sure there are objects set as network nodes before saving the network structure.\"" );
		break;
	}

	cmd( "pack $TT.v.l $TT.v.t" );

	cmd( "pack $TT.v -padx 5 -pady 5" );

	cmd( "okcancel $TT b { set choice 1 } { set choice 2 }" );	// insert ok button
	cmd( "bind $TT.v.t.lb <Double-1> { set choice 1 }" );

	cmd( "showtop $TT" );

	cmd( "$TT.v.t.lb selection set 0" );
	cmd( "focus $TT.v.t.lb" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set nodeObj [ .objs.v.t.lb get [ .objs.v.t.lb curselection ] ]" );
	cmd( "destroytop .objs" );

	if ( *choice == 2 )
		break;

	lab4 = ( char * ) Tcl_GetVar( inter, "nodeObj", 0 );
	cur = root->search( lab4 );
	if ( cur == NULL || cur->node == NULL || cur->up == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid object\" -detail \"Please make sure you select an object which is already a node of an existing network.\"" );
		break;
	}

	// make sure there is a path set
	cmd( "set path \"%s\"", path );
	if ( strlen( path ) > 0 )
		cmd( "cd \"$path\"" );

	cmd( "set bah \"%s\"", simul_name );
	cmd( "set bah [ tk_getSaveFile -parent . -title \"Save Network Structure File\"  -defaultextension \".net\" -initialdir \"$path\" -initialfile \"$bah.net\" -filetypes { { {Pajek network files} {.net} } } ]" );
	*choice = 0;
	cmd( "if { [ string length $bah ] > 0 && ! [ fn_spaces \"$bah\" . ] } { set netPath [ file dirname $bah ]; set netFile [ file tail $bah ]; set posExt [ string last . $netFile ]; if { $posExt >= 0 } { set netExt [ string range $netFile [ expr $posExt + 1 ] end ]; set netFile [ string range $netFile 0 [ expr $posExt - 1 ] ] } { set netExt \"\" } } { set choice 2 }" );
	if ( *choice == 2 )
		break;

	lab1 = ( char * ) Tcl_GetVar( inter, "netPath", 0 );
	lab2 = ( char * ) Tcl_GetVar( inter, "netFile", 0 );
	lab3 = ( char * ) Tcl_GetVar( inter, "netExt", 0 );
	if ( strlen( lab2 ) == 0 )
		break;

	plog( "\nSaving network on object '%s' to file %s%s%s%s%s...\n", "", lab4, lab1, foldersep( lab1 ), lab2, strlen( lab3 ) == 0 ? "" : ".", lab3 );

	nLinks = cur->up->write_file_net( lab4, lab1, lab2, -1 );
	if ( nLinks == 0 )
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid file or object\" -detail \"Please check the chosen directory/file for WRITE access and make sure you select a valid object for retrieving the network's nodes.\"" );
		plog( "Error: No network links saved\n" );
	}
	else
		plog( " %ld network links saved\n", "", nLinks );

break;


// Unload network
case 93:

	if ( ! struct_loaded )
		break;

	if ( ! discard_change( ) )	// check for unsaved configuration changes
		break;

	cmd( "set TT .objs" );
	cmd( "newtop $TT \"Unload Network\" { set choice 2 }" );

	cmd( "frame $TT.v" );
	cmd( "label $TT.v.l -text \"Object containing\nthe network nodes\"" );

	cmd( "frame $TT.v.t" );
	cmd( "scrollbar $TT.v.t.v_scroll -command \"$TT.v.t.lb yview\"" );
	cmd( "listbox $TT.v.t.lb -width 25 -selectmode single -yscroll \"$TT.v.t.v_scroll set\"" );
	cmd( "pack $TT.v.t.lb $TT.v.t.v_scroll -side left -fill y" );
	cmd( "mouse_wheel $TT.v.t.lb" );

	insert_object( "$TT.v.t.lb", root, true );
	cmd( "set numNets [ $TT.v.t.lb size ]" );
	if ( get_int( "numNets" ) == 0 )
	{
		cmd( "destroytop .objs" );
		cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"No network object found\" -detail \"Please make sure there are objects set as network nodes before unloading the network structure.\"" );
		break;
	}

	cmd( "pack $TT.v.l $TT.v.t" );

	cmd( "pack $TT.v -padx 5 -pady 5" );

	cmd( "okcancel $TT b { set choice 1 } { set choice 2 }" );	// insert ok button
	cmd( "bind $TT.v.t.lb <Double-1> { set choice 1 }" );

	cmd( "showtop $TT" );

	cmd( "$TT.v.t.lb selection set 0" );
	cmd( "focus $TT.v.t.lb" );

	*choice = 0;
	while ( *choice == 0 )
		Tcl_DoOneEvent( 0 );

	cmd( "set nodeObj [ .objs.v.t.lb get [ .objs.v.t.lb curselection ] ]" );
	cmd( "destroytop .objs" );

	if ( *choice == 2 )
		break;

	lab4 = ( char * ) Tcl_GetVar( inter, "nodeObj", 0 );
	cur = root->search( lab4 );
	if ( cur == NULL || cur->node == NULL || cur->up == NULL )
	{
		cmd( "tk_messageBox -parent . -type ok -title Error -icon error -message \"Invalid object\" -detail \"Please make sure you select an object which is already a node of an existing network.\"" );
		break;
	}

	plog( "\nRemoving network on object '%s'\n", "", lab4 );

	cur->up->delete_net( lab4 );

break;


// No-operation: toggle the state of the model structure windows, refresh window
case 70:

break;


default:

	plog( "\nWarning: choice %d not recognized", "", *choice );
}

*choice = 0;
return r;
}


/****************************************************
SHOW_SAVE
****************************************************/
void show_save( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
	{
		if ( cv->save == 1 || cv->savei == 1 )
		{
			if ( cv->param == 1 )
				sprintf( msg, "Object: %s \tParameter:\t", n->label );
			else
				sprintf( msg, "Object: %s \tVariable :\t", n->label );
			if ( cv->savei == 1 )
			{
				if ( cv->save == 1 )
				   strcat( msg, " (memory and disk)" );
				else
				   strcat( msg, " (disk only)" );
			}
			plog( msg );
			plog( "%s\n", "highlight", cv->label );
			++lcount;
		}
	}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			co = blueprint->search( cb->blabel );
		else
			co = cb->head; 
		show_save( co );
	}
}


/****************************************************
SHOW_OBSERVE
****************************************************/
void show_observe( object *n )
{
	variable *cv;
	object *co;
	description *cd;
	int app;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
	{
		cd = search_description( cv->label );
		if ( cd != NULL && cd->observe=='y' )
		{
			if ( cv->param == 1 )
				plog( "Object: %s \tParameter:\t", "", n->label );
			else
				plog( "Object: %s \tVariable :\t", "", n->label );

			plog( "%s (%lf)\n", "highlight", cv->label, cv->val[ 0 ] );
			++lcount;
		}
	}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			co = blueprint->search( cb->blabel );
		else
			co = cb->head; 
		show_observe( co );
	}
}


/****************************************************
SHOW_INITIAL
****************************************************/
void show_initial( object *n )
{
	variable *cv, *cv1;
	object *co;
	description *cd;
	int i;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
	{
		cd = search_description( cv->label );
		if ( cd != NULL && cd->initial == 'y' )
		{
			if ( cv->param == 1 )
				plog( "Object: %s \tParameter:\t", "", n->label );
			if ( cv->param == 0 )
				plog( "Object: %s \tVariable :\t", "", n->label );
			if ( cv->param == 2 )
				plog( "Object: %s \tFunction :\t", "", n->label );
		
			++lcount;
			plog( "%s \t", "highlight", cv->label );
			
			if ( cd->init == NULL || strlen( cd->init ) == 0 )
			{
				for ( co = n; co != NULL; co = co->hyper_next( co->label ) )
				{
					cv1 = co->search_var( NULL, cv->label );
					plog( " %g", "", cv1->val[ 0 ] );
				}
			}
			else
			{
				for ( i = 0; cd->init[ i ] != 0; ++i )
				{
					switch ( cd->init[ i ] )
					{
						case '[': 
							plog( "\\\[" );
							break;
						case ']': 
							plog( "]" );
							break;
						case '"': 
							plog( "\\\"" );
							break;
						case '{': 
							plog( "\{" );
							break;
						default: 
							plog( "%c", "", cd->init[ i ] );
							break;          
					}
				}
			} 
			plog("\n" );
		}
	}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head != NULL )
		{
			co = cb->head; 
			show_initial( co );
		}
	} 
}


/****************************************************
SHOW_PLOT
****************************************************/
void show_plot( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		if ( cv->plot )
		{
			if ( cv->param == 1 )
				plog( "Object: %s \tParameter:\t", "", n->label );
			if ( cv->param == 0 )
				plog( "Object: %s \tVariable :\t", "", n->label );
			if ( cv->param == 2 )
				plog( "Object: %s \tFunction :\t", "", n->label );
			plog( "%s\n", "highlight", cv->label );
			lcount++;
		}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			co = blueprint->search( cb->blabel );
		else
			co = cb->head; 
		show_plot( co );
	}
}


/****************************************************
SHOW_DEBUG
****************************************************/
void show_debug( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		if ( cv->debug == 'd' )
		{
			if ( cv->param == 0 )
				plog( "Object: %s \tVariable :\t", "", n->label );
			if ( cv->param == 2 )
				plog( "Object: %s \tFunction :\t", "", n->label );
			plog( "%s\n", "highlight", cv->label );
			lcount++;
		}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			co = blueprint->search( cb->blabel );
		else
			co = cb->head; 
		show_debug( co );
	}
}


/****************************************************
SHOW_PARALLEL
****************************************************/
void show_parallel( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		if ( cv->parallel )
		{
			plog( "Object: %s \tVariable :\t", "", n->label );
			plog( "%s\n", "highlight", cv->label );
			lcount++;
		}

	for ( cb = n->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			co = blueprint->search( cb->blabel );
		else
			co = cb->head; 
		show_parallel( co );
	}
}


/****************************************************
CLEAN_DEBUG
****************************************************/
void clean_debug( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		cv->debug = 'n';

	for ( cb = n->b; cb != NULL; cb = cb->next )
		for ( co = cb->head; co != NULL; co = co->next )
			clean_debug( co );
}


/****************************************************
CLEAN_SAVE
****************************************************/
void clean_save( object *n )
{
	variable *cv;
	object *co;
	bridge *cb; 

	for ( cv = n->v; cv != NULL; cv = cv->next )
	{
		cv->save = 0;
		cv->savei = 0;
	}
	for ( cb = n->b; cb != NULL; cb = cb->next )
		for ( co = cb->head; co !=NULL; co = co->next )
			clean_save( co );
}


/****************************************************
CLEAN_PLOT
****************************************************/
void clean_plot( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		cv->plot = false;
	 
	for ( cb = n->b; cb != NULL; cb = cb->next )
		for ( co = cb->head; co !=NULL; co = co->next )
			clean_plot( co );
}


/****************************************************
CLEAN_PARALLEL
****************************************************/
void clean_parallel( object *n )
{
	variable *cv;
	object *co;
	bridge *cb;

	for ( cv = n->v; cv != NULL; cv = cv->next )
		cv->parallel = false;
	 
	for ( cb = n->b; cb != NULL; cb = cb->next )
		for ( co = cb->head; co !=NULL; co = co->next )
			clean_parallel( co );
}


/****************************************************
WIPE_OUT
****************************************************/
void wipe_out( object *d )
{
	object *cur;
	variable *cv;

	cmd( "if [ info exists ModElem ] { set pos [ lsearch -exact $ModElem \"%s\" ]; if { $pos >= 0 } { set ModElem [ lreplace $ModElem $pos $pos ] } }", d->label );

	change_descr_lab( d->label, "", "", "", "" );

	for ( cv = d->v; cv != NULL; cv = cv->next )
	{
		cmd( "if [ info exists ModElem ] { set pos [ lsearch -exact $ModElem \"%s\" ]; if { $pos >= 0 } { set ModElem [ lreplace $ModElem $pos $pos ] } }", cv->label  );

		change_descr_lab( cv->label, "" , "", "", "" );
	}

	cur = d->hyper_next( d->label );
	if ( cur != NULL )
		wipe_out( cur );

	delete_bridge( d );
}


/****************************************************
CHECK_LABEL
Control that the label l does not already exist in the model
Also prevents invalid characters in the names
****************************************************/
int check_label( char *l, object *r )
{
	object *cur;
	variable *cv;
	bridge *cb;

	Tcl_SetVar( inter, "nameVar", l, 0 );
	cmd( "if [ regexp {^[a-zA-Z_][a-zA-Z0-9_]*$} $nameVar ] { set answer 1 } { set answer 0 }" );
	const char *answer = Tcl_GetVar( inter, "answer", 0 );
	if ( *answer == '0' )
		return 2;				// error if invalid characters (incl. spaces)

	if ( ! strcmp( l, r->label ) )
		return 1;

	for ( cv = r->v; cv != NULL; cv = cv->next )
		if ( ! strcmp( l, cv->label ) )
			return 1;

	for ( cb = r->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			cur = blueprint->search( cb->blabel );
		else
			cur = cb->head; 
		
		if ( check_label( l, cur ) )
			return 1;
	} 

	return 0;
}


/****************************************************
SET_SHORTCUTS
Define keyboard shortcuts to menu items
****************************************************/
void set_shortcuts( const char *window, const char *help )
{
	cmd( "bind %s <F1> { LsdHelp %s }", window, help  );
	cmd( "bind %s <Control-l> {set choice 17}; bind %s <Control-L> {set choice 17}", window, window  );
	cmd( "bind %s <Control-s> {set choice 18}; bind %s <Control-S> {set choice 18}", window, window  );
	cmd( "bind %s <Control-e> {set choice 20}; bind %s <Control-E> {set choice 20}", window, window  );
	cmd( "bind %s <Control-q> {set choice 11}; bind %s <Control-Q> {set choice 11}", window, window  );
	cmd( "bind %s <Control-v> {set param 0; set choice 2}; bind %s <Control-V> {set param 0; set choice 2}", window, window  );
	cmd( "bind %s <Control-p> {set param 1; set choice 2}; bind %s <Control-P> {set param 1; set choice 2}", window, window  );
	cmd( "bind %s <Control-n> {set param 2; set choice 2}; bind %s <Control-N> {set param 2; set choice 2}", window, window  );
	cmd( "bind %s <Control-d> {set choice 3}; bind %s <Control-D> {set choice 3}", window, window  );
	cmd( "bind %s <Control-o> {set choice 19}; bind %s <Control-O> {set choice 19}", window, window  );
	cmd( "bind %s <Control-i> {set choice 21}; bind %s <Control-I> {set choice 21}", window, window  );
	cmd( "bind %s <Control-a> {set choice 26}; bind %s <Control-A> {set choice 26}", window, window  );
	cmd( "bind %s <Control-r> {set choice 1}; bind %s <Control-R> {set choice 1}", window, window  );
	cmd( "bind %s <Control-m> {set choice 22}; bind %s <Control-M> {set choice 22}", window, window  );
	cmd( "bind %s <Control-f> {set choice 50}; bind %s <Control-F> {set choice 50}", window, window  );
	cmd( "bind %s <Control-u> {set choice 28}; bind %s <Control-U> {set choice 28}", window, window  );
	cmd( "bind %s <Control-g> {set choice 30}; bind %s <Control-G> {set choice 30}", window, window  );
	cmd( "bind %s <Control-b> {set choice 34}; bind %s <Control-B> {set choice 34}", window, window  );
	cmd( "bind %s <Control-z> {set choice 37}; bind %s <Control-Z> {set choice 37}", window, window  );
	cmd( "bind %s <Control-w> {set choice 38}; bind %s <Control-W> {set choice 38}", window, window  );
	cmd( "bind %s <Control-Tab> {set strWindowOn [expr ! $strWindowOn]; set choice 70}", window  );
}


/****************************************************
CONTROL_TOCOMPUTE
****************************************************/
void control_tocompute( object *r, char *l )
{
	object *cur;
	variable *cv;
	bridge *cb;

	for ( cv = r->v; cv != NULL; cv = cv->next )
	{
		if ( cv->save == 1 )
			cmd( "tk_messageBox -parent . -type ok -title Warning -icon warning -message \"Cannot save item\" -detail \"Item '%s' set to be saved but it will not be registered for the Analysis of Results, since object '%s' is not set to be computed.\"", cv->label, l );
	}

	for ( cb = r->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			cur=blueprint->search(cb->blabel);
		else
			cur = cb->head; 
		
		control_tocompute( cur, l );
	} 
}


/****************************************************
INSERT_OBJECT
****************************************************/
void insert_object( const char *w, object *r, bool netOnly )
{
	object *cur;
	bridge *cb;

	if ( ! netOnly || r->node != NULL )
		cmd( "%s insert end %s", w, r->label );

	for ( cb = r->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			cur = blueprint->search( cb->blabel );
		else
			cur = cb->head; 
		
		insert_object( w, cur, netOnly );
	}
}


/****************************************************
SHIFT_VAR
****************************************************/
void shift_var( int direction, char *vlab, object *r )
{
	variable *cv, *cv1 = NULL, *cv2 = NULL;
	
	if ( direction == -1 )
	{	// shift up
		if ( ! strcmp( vlab, r->v->label ) )
			return; 	// variable already at the top
		
		if ( ! strcmp( vlab, r->v->next->label ) )
		{	// second var, must become the head of the chain
			cv = r->v->next->next;	// third
			cv1 = r->v; 			// first
			r->v = r->v->next; 		// shifted up
			r->v->next = cv1;
			cv1->next = cv;
			return;
		}  
		
		for ( cv = r->v; cv != NULL; cv = cv->next )
		{
			if ( ! strcmp( vlab, cv->label ) )
			{
				cv2->next = cv;
				cv1->next = cv->next;
				cv->next = cv1;
				return;
			}
			
			cv2 = cv1;
			cv1 = cv;
		}
	}
	
	if ( direction == 1 )
	{	// move down
		if ( ! strcmp( vlab, r->v->label ) )
		{	// it's the first
			if ( r->v->next == NULL )
				return; 			// it is unique
			
			cv = r->v;				// first
			cv1 = cv->next->next;	// third
			r->v = cv->next;		// first is former second
			r->v->next = cv;		// second is former first
			cv->next = cv1;			// second points to third
			return;
		}
		
		for ( cv = r->v; cv != NULL; cv = cv->next )
		{
			if ( ! strcmp( vlab,cv->label ) )
			{
				if ( cv->next == NULL )
					return;			// already at the end
				
				cv1->next = cv->next;
				cv->next = cv->next->next;
				cv1->next->next = cv;
				return;
			}
			
			cv1 = cv;
		}
	} 

	plog("\nWarning: should never reach this point in move_var" ); 
}


/****************************************************
SHIFT_DESC
****************************************************/
void shift_desc( int direction, char *dlab, object *r )
{
	bridge *cb, *cb1 = NULL, *cb2 = NULL;
	
	if ( direction == -1 )
	{	// shift up
		if ( ! strcmp( dlab, r->b->blabel ) )
			return; 	// object already at the top
		
		if ( ! strcmp( dlab, r->b->next->blabel ) )
		{	// second var, must become the head of the chain
			cb = r->b->next->next;	// third
			cb1 = r->b; 			// first
			r->b = r->b->next; 		// shifted up
			r->b->next = cb1;
			cb1->next = cb;
			return;
		}  
		
		for ( cb = r->b; cb != NULL; cb = cb->next )
		{
			if ( ! strcmp( dlab, cb->blabel ) )
			{
				cb2->next = cb;
				cb1->next = cb->next;
				cb->next = cb1;
				return;
			}
			
			cb2 = cb1;
			cb1 = cb;
		}
	}
	
	if ( direction == 1 )
	{	//move down
		if ( ! strcmp( dlab, r->b->blabel ) )
		{	// it's the first
			if ( r->b->next == NULL)
				return; 			// it is unique
			
			cb = r->b;				// first
			cb1 = cb->next->next;	// third
			r->b = cb->next;		// first is former second
			r->b->next = cb;		// second is former first
			cb->next = cb1;			// second points to third
			return;
		}
		
		for ( cb = r->b; cb != NULL; cb = cb->next )
		{
			if ( ! strcmp( dlab, cb->blabel ) )
			{
				if ( cb->next == NULL )
					return;			// already at the end
				
				cb1->next = cb->next;
				cb->next = cb->next->next;
				cb1->next->next = cb;
				return;
			}
			
			cb1 = cb;
		}
	} 
	 
	plog("\nWarning: should never reach this point in shift_desc" ); 
}


/****************************************************
SENSITIVITY_TOO_LARGE
****************************************************/
void sensitivity_too_large( void )
{
	cmd( "set answer [tk_messageBox -parent . -type okcancel -icon warning -default cancel -title Warning -message \"Too many cases to perform sensitivity analysis\" -detail \"The required number of configuration points to perform sensitivity analysis is likely too large to be processed in reasonable time.\n\nPress 'OK' if you want to continue anyway or 'Cancel' to abort the command now.\"]; switch -- $answer {ok {set choice 1} cancel {set choice 0}}" );
}


/****************************************************
SENSITIVITY_CREATED
****************************************************/
void sensitivity_created( void )
{
	plog( "\nSensitivity analysis configurations produced: %d", "", findexSens - 1 );
	cmd( "tk_messageBox -parent . -type ok -icon info -title \"Sensitivity Analysis\" -message \"Configuration files created\" -detail \"LSD has created configuration files (.lsd) for all the sensitivity analysis required points.\n\nTo run the analysis first you have to create a 'No Window' version of the model program, using the 'Model'/'Generate 'No Window' Version' menu option in LMM. This step has to be done every time you modify your equations file.\n\nSecond, start the processing of sensitivity configuration files by selecting 'Run'/'Create/Run Parallel Batch...' menu option.\n\nAlternatively, open a command prompt (terminal window) and execute the following command in the directory of the model:\n\n> lsd_gnuNW  -f  <configuration_file>  -s  <n>\n\nReplace <configuration_file> with the name of your original configuration file WITHOUT the '.lsd' extension and <n> with the number of the first configuration file to be run (usually 1). If your configuration files are in a subdirectory of your model directory, please add their relative path before the configuration file name (i.e. <path>/<configuration_file>).\"" );
}


/****************************************************
SENSITIVITY_UNDEFINED
****************************************************/
void sensitivity_undefined( void )
{
 	cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Sensitivity analysis items not found\" -detail \"Before using this option you have to select at least one parameter or lagged variable initial value to perform the sensitivity analysis and inform the corresponding values to be explored.\n\nTo set the sensitivity analysis values (or ranges), use the 'Sensitivity Analysis' button in the 'Model'/'Change Element...' menu option (or the corresponding context menu option) and inform the values or range(s) using the syntax explained in the 'Sensitivity Analysis' entry window (it is possible to paste a list of values from the clipboard). You can repeat this procedure for each required parameter or initial value.\n\nSensitivity Analysis values are NOT saved in the standard LSD configuration file (.lsd) and if needed they MUST be saved in a LSD sensitivity analysis file (.sa) using the 'File'/'Save Sensitivity...' menu option.\"" );
}


/****************************************************
LOAD_CONFIGURATION_FAILED
****************************************************/
void load_configuration_failed( void )
{
	cmd( "tk_messageBox -parent . -type ok -icon error -title Error -message \"Configuration file cannot be reloaded\" -detail \"Previously loaded configuration could not be restored. Check if LSD still has WRITE access to the model directory.\n\nCurrent configuration will be reset now.\"" );
}


/****************************************************
SAVE_POS
Save user position in browser
****************************************************/
void save_pos( object *r )
{
	// save the current object & cursor position for quick reload
	strcpy( lastObj, r->label );
	cmd( "if { ! [ string equal [ .l.s.c.son_name curselection ] \"\" ] } { set lastList 2 } { set lastList 1 }" );
	cmd( "if { $lastList == 1 } { set lastItem [ .l.v.c.var_name curselection ] } { set lastItem [ .l.s.c.son_name curselection ] }" );
	cmd( "if { $lastItem == \"\" } { set lastItem 0 }" );
}


/****************************************************
RESTORE_POS
Restore user position in browser
****************************************************/
object *restore_pos( object *r )
{
	object *n;
	
	if ( strlen( lastObj ) > 0 )
	{
		for ( n = r; n->up != NULL; n = n->up );
		n = n->search( lastObj );
		if ( n != NULL )
		{
			cmd( "if [ info exists lastList ] { set listfocus $lastList }" );
			cmd( "if [ info exists lastItem ] { set itemfocus $lastItem }" );
			return n;
		}
	}
	return r;
}


/****************************************************
UNSAVED_CHANGE
Read or set the UnsavedChange flag and update windows titles accordingly
****************************************************/
bool unsavedChange = false;		// control for unsaved changes in configuration
#define WND_NUM 7
const char *wndName[ ] = { ".", ".log", ".str", ".ini", ".da", ".deb", ".lat" };

bool unsaved_change( bool val )
{
	int i; 
	
	if ( unsavedChange != val )
	{
		unsavedChange = val;
		char chgMark[ ] = "\0\0";
		chgMark[ 0 ] = unsavedChange ? '*' : ' ';
		
		// change all the possibly open (single) windows
		for ( i = 0; i < WND_NUM; ++i )
		{
			cmd( "if [ winfo exist %s ] { wm title %s \"%s[ string range [ wm title %s ] 1 end ]\" }", wndName[ i ], wndName[ i ], chgMark, wndName[ i ]  );
		}
		// handle (possibly multiple) run-time plot windows
		cmd( "set a [ split [ winfo children . ] ]" );
		cmd( "foreach i $a { if [ string match .plt* $i ] { wm title $i \"%s[ string range [ wm title $i ] 1 end ]\" } }", chgMark  );
	}
	
	return unsavedChange;
}

bool unsaved_change( void )
{
	return unsavedChange;
}


/****************************************************
DISCARD_CHANGE
Ask user to discard changes in configuration, if applicable
Returns: 0: abort, 1: continue without saving
****************************************************/
bool discard_change( bool checkSense, bool senseOnly )
{
	// don't stop if simulation is runnig
	if ( running )
	{
		cmd( "set answer [tk_messageBox -parent .log -type ok -icon error -title Error -message \"Cannot quit LSD\" -detail \"Cannot quit while simulation is running. Press 'OK' to continue simulation processing. If you really want to abort the simulation, press 'Stop' in the 'Log' window first.\"]" );
		return false;
	}
	// nothing to save?
	if ( ! unsavedData && ! unsavedChange && ! unsavedSense )
		return true;					// yes: simply discard configuration
	else								// no: ask for confirmation
		if ( ! senseOnly && unsavedData )
			cmd( "set answer [tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Discard data?\" -detail \"All data generated and not saved will be lost!\nDo you want to continue?\"]" );
		else
			if ( ! senseOnly && unsavedChange )
			{
				Tcl_SetVar( inter, "filename", simul_name , 0 );
				cmd( "set answer [tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Discard changes?\" -detail \"Recent changes to configuration '$filename' are not saved!\nDo you want to discard and continue?\"]" );
			}
			else						// there is unsaved sense data
				if ( checkSense )
					cmd( "set answer [tk_messageBox -parent . -type yesno -default yes -icon question -title Confirmation -message \"Discard changes?\" -detail \"Recent changes to sensitivity data are not saved!\nDo you want to discard and continue?\"]" );
				else
					return true;		// checking sensitivity data is disabled

	cmd( "if [ string equal $answer yes ] { set ans 1 } { set ans 0 }" );  
	const char *ans = Tcl_GetVar( inter, "ans", 0 );
	if ( atoi( ans ) == 1 )
		return true;
	else
		return false;
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


/****************************************************
TCL_GET_VAR_CONF
Function to get variable configuration from Tcl
****************************************************/
int Tcl_get_var_conf( ClientData cdata, Tcl_Interp *inter, int argc, const char *argv[ ] )
{
	char vname[ MAX_ELEM_LENGTH ], res[ 2 ];
	variable *cv;
	
	if ( argc != 3 )					// require 2 parameters: variable name and property
		return TCL_ERROR;
		
	if ( argv[ 1 ] == NULL || argv[ 2 ] == NULL || ! strcmp( argv[ 1 ], "(none)" ) )
		return TCL_ERROR;
	
	sscanf( argv[ 1 ], "%99s", vname );	// remove unwanted spaces
	cv = currObj->search_var( NULL, vname );

	if ( cv == NULL )					// variable not found
		return TCL_ERROR;

	// get the appropriate value for variable
	res[ 1 ] = '\0';					// default is 1 char string array
	if ( ! strcmp( argv[ 2 ], "save" ) )
		res[ 0 ] = cv->save ? '1' : '0';
	else 
		if ( ! strcmp( argv[ 2 ], "plot" ) )
			res[ 0 ] = cv->plot ? '1' : '0';
		else
			if ( ! strcmp( argv[ 2 ], "debug" ) )
				res[ 0 ] = cv->debug == 'd' ? '1' : '0';
			else
				if ( ! strcmp( argv[ 2 ], "parallel" ) )
					res[ 0 ] = cv->parallel ? '1' : '0';
				else
					return TCL_ERROR;
	
	Tcl_SetResult( inter, res, TCL_VOLATILE );
	return TCL_OK;		
}


/****************************************************
TCL_SET_VAR_CONF
Function to set variable configuration from Tcl
****************************************************/
int Tcl_set_var_conf( ClientData cdata, Tcl_Interp *inter, int argc, const char *argv[ ] )
{
	char vname[ MAX_ELEM_LENGTH ];
	variable *cv;
	object *cur;
	
	if ( argc != 4 )					// require 3 parameters: variable name, property and value
		return TCL_ERROR;
		
	if ( argv[ 1 ] == NULL || argv[ 2 ] == NULL || 
		 argv[ 3 ] == NULL || ! strcmp( argv[ 1 ], "(none)" ) )
		return TCL_ERROR;
	
	sscanf( argv[ 1 ], "%99s", vname );	// remove unwanted spaces
	cv = currObj->search_var( NULL, vname );
	
	if ( cv == NULL )					// variable not found
		return TCL_ERROR;

	// set the appropriate value for variable (all instances)
	for ( cur = currObj; cur != NULL; cur = cur->hyper_next( cur->label ) )
	{
		cv = cur->search_var( NULL, vname );
		if ( ! strcmp( argv[ 2 ], "save" ) )
		{
			cv->save = ( ! strcmp( argv[ 3 ], "1" ) ) ? true : false;
			redrawReq = true;
		}
		else 
			if ( ! strcmp( argv[ 2 ], "savei" ) )
			{
				cv->savei = ( ! strcmp( argv[ 3 ], "1" ) ) ? true : false;
				redrawReq = true;
			}
			else
				if ( ! strcmp( argv[ 2 ], "plot" ) )
					cv->plot = ( ! strcmp( argv[ 3 ], "1" ) ) ? true : false;
				else
					if ( ! strcmp( argv[ 2 ], "debug" ) )
						cv->debug  = ( ! strcmp( argv[ 3 ], "1" ) ) ? 'd' : 'n';
					else
						if ( ! strcmp( argv[ 2 ], "parallel" ) )
							cv->parallel  = ( ! strcmp( argv[ 3 ], "1" ) ) ? true : false;
						else
							return TCL_ERROR;
	}
	unsaved_change( true );				// signal unsaved change

	if ( cv->save || cv->savei )
	{
		for ( cur = currObj; cur != NULL; cur = cur->up )
			if ( cur->to_compute == 0 )
			{
				cmd( "tk_messageBox -parent . -type ok -title Warning -icon warning -message \"Cannot save item\" -detail \"Item\n'%s'\nset to be saved but it will not be registered for the Analysis of Results, since object\n'%s'\nis not set to be computed.\"", vname, cur->label );
			}
	}
	
	return TCL_OK;		
}
