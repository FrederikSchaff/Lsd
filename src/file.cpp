/*************************************************************

	LSD 7.1 - May 2018
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente
	LSD is distributed under the GNU General Public License
	
 *************************************************************/

/****************************************************
FILE.CPP
Contains the methods and functions used to save and load a model. The
data for a model are composed by a structure, initial values and simulation
setting.

The method of object contained in this file are:

- void object::save_struct(FILE *f, char *tab)
Saves the structure of the object (that is, the label,
variables and parameters and descendants, not number of objects).
This method is called first time by the browser in INTERF.CPP by the root of the
model.
Calls the save_struct for all the descendant type.

- void object::save_param(FILE *f )
Save the numerical values for the object (one digit
for each group of object of this type) and the initial values for variables.
It save also option information, that is whether to save, plot or debug the
variables.
It calls the save_param for all the descendant type.
It is called in the browser, INTERF.CPP, immediately after save_struct, by the
root of the model.

- void object::load_struct(FILE *f )
Initialize a model by creating  one as defined
in the data file. The model, after this stage, has only one instance for each
object type and variables and parameters are simply labels.

- int object::load_param(char *file_name, int repl)
It loads from the file named as specified the data
for the object. It is made in specular way in respect of save_param.
Called from browser in INTERF.CPP immediately after load_struct.

- void object::replicate(int num, int propagate)
Counts the number of instances (this included) of this type of object and
make as many copy as necessary to have num of them. In case propagate is 1,
it sends the replicate message to the hypernext, first. It should be applied
to a structure model, that is, where the object is there are no more than
one instance in any group.


- void copy_descendant(object *from, object *to)
This is a function called by replicate above, so that the replicated object
are initialized also the descendants.

-object *create(object *root)
The main cycle for the Browser, from which it exits only to run a simulation
or to quit the program. The cycle is just once call to browsw followed by
a call to operate.

- int browse(object *r, int *choice);
build the browser window and waits for an action (on the form of
values for choice or choice_g different from 0)

- object *operate(Tcl_Interp *in, int *choice, object *r);
takes the value of choice and operate the relative command on the
object r. See the switch for the complete list of the available commands

- void clean_debug(object *n);
remove all the flags to debug from any variable in the model

- void clean_save(object *n);
remove all the flags to save from any variable in the model

- void clean_plot(object *n);
remove all the flags to plot from any variable in the model


Functions used here from other files are:

- void plog(char *m);
LSDMAIN.CPP print  message string m in the Log screen.

- void analysis(int *choice);
ANALYSIS.CPP analysis of result files

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

- FILE *search_data_str(char *name, char *init, char *str)
UTIL.CPP given a string name, returns the file with that name and the current position
placed immediately after the string str found after the string init. Needed to
not get confused managing the data files, where the same string appears twice,
in the structure definition and in the data section.

- FILE *search_data_ent(char *name, variable *v)
UTIL.CPP given the file name name, the routine searches for the data line for the variable
(or parameter) v. It is not messed up by same labels for variables and objects.

****************************************************/

#include "decl.h"


/****************************************************
OBJECT::SAVE_STRUCT
****************************************************/
void object::save_struct( FILE *f, char const *tab )
{
	int count = 0, i, j;
	char tab1[ 30 ];
	object *o, *o1;
	bridge *cb, *cb1;
	variable *var, *c_var;

	if ( up == NULL )
		fprintf( f, "\t\n" );

	strcpy( tab1, tab );
	fprintf( f, "%sLabel %s\n%s{\n", tab1, label, tab1 );
	strcat( tab1, "\t" );
	
	if ( b != NULL )
		cb1=b->next;

	for ( cb = b; cb != NULL; cb = cb->next )
	{
		fprintf( f, "%sSon: %s\n", tab1, cb->blabel );
		if ( cb->head == NULL )
			o = blueprint->search( cb->blabel );
		else
			o = cb->head;
		o->save_struct( f, tab1 );	
	}

	for ( var = v; var != NULL; var = var->next )
	{
		if ( var->param == 0 )
			fprintf( f, "%sVar: %s\n", tab1, var->label );
		if ( var->param == 1 )
			fprintf( f, "%sParam: %s\n", tab1, var->label );
		if ( var->param == 2)
			fprintf( f, "%sFunc: %s\n", tab1, var->label );		
	}   
	 
	fprintf( f, "\n" );
	fprintf( f, "%s}\n\n", tab );
} 


/****************************************************
OBJECT::SAVE_PARAM
****************************************************/
void object::save_param(FILE *f )
{
	int i, count;
	object *cur;
	variable *cv, *cv1;
	char ch, ch1, ch2;
	description *cur_descr;
	bridge *cb;

	count=0;
	fprintf(f, "\nObject: %s", label);
	if (to_compute == 1 )
	  fprintf(f, " C");
	else
	  fprintf(f, " N");

	for (cur=this;cur!=NULL; )
	 {skip_next_obj(cur, &count);
	  fprintf(f, "\t%d", count);  
	  for ( ;go_brother(cur)!=NULL; cur=cur->next);

	  cur=cur->hyper_next(cur->label);
	 }
	fprintf(f, "\n");

	for (cv=v; cv!=NULL; cv=cv->next)
	{
	  //search for unloaded data
	  ch='+';
	  if (cv->param==1 || cv->num_lag>0)
	  {
	  for (cur=this; cur!=NULL; cur=cur->hyper_next(label))
		{cv1=cur->search_var(NULL, cv->label);
		 if (cv1->data_loaded=='-')
		  {ch='-';
		   break;
		  }
		}
	  }
	  else
	   {//avoid marking as to initialize for elements not worth it
		cur_descr = search_description(cv->label);
		cur_descr->initial='n';
	   }

	 ch1 = cv->save ? 's' : 'n';
	 ch1 = cv->savei ? toupper( ch1 ) : ch1;

	 ch2 = cv->plot ? 'p' : 'n';
	 ch2 = cv->parallel ? toupper( ch2 ) : ch2;
	 
	 if (cv->param == 0 )
		fprintf( f, "Var: %s %d %c %c %c %c", cv->label, cv->num_lag, ch1, ch, cv->debug, ch2 );
	 if (cv->param == 1 )
		fprintf( f, "Param: %s %d %c %c %c %c", cv->label, cv->num_lag, ch1, ch, cv->debug, ch2 );
	 if (cv->param==2)
		fprintf( f, "Func: %s %d %c %c %c %c", cv->label, cv->num_lag, ch1, ch, cv->debug, ch2 );

	 for (cur=this; cur!=NULL; cur=cur->hyper_next(label))
	   {cv1=cur->search_var(NULL, cv->label);
		if (cv1->param == 1 )
		  {if (cv1->data_loaded=='+')
			fprintf(f, "\t%g", cv1->val[ 0 ]);
		   else
			fprintf(f, "\t%c", '0');
		   }
		 else
		  {
		  for ( i = 0; i<cv->num_lag; ++i )
			  {

			 if (cv1->data_loaded=='+')
			  fprintf(f, "\t%g", cv1->val[ i ]);
			 else
				 fprintf(f, "\t%c", '0');
			}  
		  }  
	   }
	 fprintf(f, "\n");
	}
	
	for ( cb = b; cb != NULL; cb = cb->next )
		if ( cb->head != NULL )
			cb->head->save_param( f );
}


/****************************************************
OBJECT::LOAD_PARAM
****************************************************/
bool object::load_param(char *file_name, int repl, FILE *f )
{
	char str[MAX_ELEM_LENGTH], ch, ch1, ch2;
	int num, i;
	object *cur;
	variable *cv, *cv1;
	double app;
	bridge *cb;

	if ( f == NULL )
	 f=search_data_str(file_name, "DATA", label); 
	else
	 {
	  fscanf(f, "%99s", str); //skip the 'Object: '
	  fscanf(f, " %99s", str); //skip the 'label'  
	 } 

	if ( f == NULL )
		return false;

	if (fscanf(f, " %c", &ch)!=1)
		 return false;
	if (ch=='C')
	 to_compute=1;
	else
	 to_compute=0;

	for (cur=this;cur!=NULL; )
	{
	  if (fscanf(f, "\t%d", &num ) != 1 )
		 return false;
	  cur->to_compute=to_compute;
	  cur->replicate(num, 0 );

	  for ( ;go_brother(cur)!=NULL; cur=cur->next);
	  cur=cur->hyper_next(cur->label);
	}

	for (cv=v; cv!=NULL; cv=cv->next)
	{
	  fscanf(f, "%99s ", str); //skip the 'Element: '
	  fscanf(f, "%99s ", str); //skip the 'label'
	  if ( f == NULL )
		 return false;

	  if (fscanf(f, "%d %c %c %c %c", &(cv->num_lag ), &ch1, &ch, &(cv->debug), &ch2   )!=5)
		 return false;

	  cv->save = ( tolower( ch1 ) == 's' ) ? true : false;
	  cv->savei = ( ch1 == 'S' || ch1 == 'N' ) ? true : false;

	  cv->plot = ( tolower( ch2 ) == 'p' ) ? true : false;
	  cv->parallel = ( ch2 == 'P' || ch2 == 'N' ) ? true : false;

	  for (cur=this; cur!=NULL; repl==1?cur=cur->hyper_next(label):cur=NULL)
	  {
		 cv1=cur->search_var(NULL, cv->label);
		 cv1->val=new double[cv->num_lag+1];
		 cv1->num_lag=cv->num_lag;
		 cv1->save=cv->save;
		 cv1->savei=cv->savei;
		 cv1->plot=cv->plot;
		 cv1->parallel = cv->parallel;
		 cv1->param=cv->param;
		 cv1->debug=cv->debug;
		 cv1->data_loaded=ch;
		 if (cv1->param == 1 )
		 {
			if (fscanf(f, "%lf", &app ) != 1 )
				return false;
			else
				cv1->val[ 0 ]=app;
		 }
		 else
		 {
			for ( i = 0; i<cv->num_lag; ++i )
			{
			  if ( ( num = fscanf(f, "\t%lf", &app ) ) != 1 )
				  return false;
				else // Places values shifted one position, since they are "time 0" values
				  cv1->val[ i ]=app;
			}
			cv1->val[cv->num_lag]=0;
		 }
	  }
	}

	for ( cb = b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL || ! cb->head->load_param( file_name, repl, f ) )
			return false;
		num = 0;
	}

	if ( up == NULL )	//this is the root, and therefore the end of the loading
		set_blueprint( blueprint, this );
	 
	return true;
}


/****************************************************
OBJECT::REPLICATE
****************************************************/
void object::replicate(int num, int propagate)
{
	object *cur, *app, *cur1, *app1, *cur2;
	variable *cv;
	int i, usl;
	bridge *cb;

	if (propagate == 1 )
		cur=hyper_next(label);
	else
		cur=NULL;
	if (cur != NULL )
		cur->replicate(num, 1);
	skip_next_obj(this, &usl);
	for (cur=this, i=1; i<usl; cur=cur->next, ++i );

	for (i=usl; i<num; ++i )
	{
		app=cur->next;
		cur->next=new object;
		cur->next->init(up, label);
		cur->next->to_compute=to_compute;
		cur->next->next=app;
		cur->to_compute=to_compute;
		app=cur->next;
		for (cv=v; cv!=NULL; cv=cv->next)
			app->add_var_from_example(cv);

		copy_descendant(this, app);
	}
}


/****************************************************
COPY_DESCENDANT
****************************************************/
void copy_descendant(object *from, object *to)
{
	object *cur, *app;
	bridge *cb, *cb1;
	variable *cv;
	int count;

	if ( from->b == NULL )
	{
		to->b = NULL;
		return;
	}
	
	if ( from->b->head == NULL )
		app = blueprint->search( from->b->blabel );
	else
		app = from->b->head;
	
	to->b = new bridge;
	to->b->blabel = new char[ strlen( from->b->blabel ) + 1 ];
	strcpy( to->b->blabel, from->b->blabel );
	to->b->counter_updated = false;
	to->b->mn = NULL;

	to->b->head = new object;
	to->b->head->init( to, app->label );
	
	for ( cv = app->v; cv != NULL; cv = cv->next )
		to->b->head->add_var_from_example( cv );
	
	to->b->head->to_compute = app->to_compute;
	copy_descendant( app, to->b->head );
	to->b->next = NULL;
	cb = to->b;

	for ( cb1 = from->b->next; cb1 != NULL; cb1 = cb1->next )
	{ 
		cb->next = new bridge;
		cb = cb->next;
		cb->next = NULL;
		cb->mn = NULL;
		cb->blabel = new char[ strlen( cb1->blabel ) + 1 ];
		strcpy( cb->blabel, cb1->blabel );
		cb->counter_updated = false;
		
		if ( cb1->head == NULL )
			app = blueprint->search( cb1->blabel );
		else
			app = cb1->head;
		
		cb->head = new object;    
		cb->head->init( to, app->label );
		cb->head->next = NULL;
		cb->head->to_compute = app->to_compute;
		for ( cv = app->v; cv != NULL; cv = cv->next )
			cb->head->add_var_from_example( cv );
		copy_descendant( app, cb->head );
	}
}


/****************************************************
OBJECT::LOAD_STRUCT
****************************************************/
bool object::load_struct( FILE *f )
{
	char ch[ MAX_ELEM_LENGTH ];
	int len, i = 0;
	bridge *cb;
	variable *cv;

	fscanf(f,"%99s",ch);
	while ( strcmp( ch, "Label" ) && ++i < MAX_FILE_TRY )
	 fscanf(f,"%99s",ch);

	if ( i >= MAX_FILE_TRY )
		return false;

	fscanf(f,"%99s",ch);
	len=strlen(ch);
	if (label == NULL )
	  {label=new char[len+1];
		strcpy(label, ch);
	  }

	i = 0;
	fscanf(f, "%*[{\r\t\n]%99s", ch);
	while ( strcmp( ch, "}" ) && ++i < MAX_FILE_TRY )
	{
	 if (!strcmp( ch, "Son:"))
	   { fscanf(f, "%*[ ]%99s", ch);
		 add_obj( ch, 1, 0 );
	   for (cb=b; strcmp(cb->blabel, ch); cb=cb->next);

	   if ( cb->head == NULL || ! cb->head->load_struct( f ) )
		   return false;
	   }
	 if (!strcmp( ch, "Var:"))
	   { fscanf(f, "%*[ ]%99s", ch);
		 add_empty_var(ch);
		 cmd( "lappend ModElem %s",ch );
	   }
	  if (!strcmp( ch, "Param:"))
		{ fscanf(f, "%*[ ]%99s", ch);
		 cv=add_empty_var(ch);
		 cv->param=1;
		 cmd( "lappend ModElem %s",ch );
		}
	  if (!strcmp( ch, "Func:"))
		{ fscanf(f, "%*[ ]%99s", ch);
		 cv=add_empty_var(ch);
		 cv->param=2;
		 cmd( "lappend ModElem %s",ch );
	   }

	 fscanf(f, "%*[{\r\t\n]%99s", ch);
	}

	if ( i >= MAX_FILE_TRY )
		return false;

	return true;
}


/*****************************************************************************
LOAD_DESCRIPTION
******************************************************************************/
bool load_description( char *msg, FILE *f )
{
	char type[20],label[MAX_ELEM_LENGTH];
	description *app;
	char str[10*MAX_LINE_SIZE], str1[10*MAX_LINE_SIZE];
	int done, i, j;

	label[ MAX_ELEM_LENGTH - 1 ] = '\0';
	if (strncmp( msg, "Object", 6) == 0 )
	{
	  strcpy(type, "Object");
	  strncpy(label, msg+7, MAX_ELEM_LENGTH-1);
	} 
	else
		if (strncmp( msg, "Variable", 8) == 0 )
		{
			strcpy(type, "Variable");
			strncpy(label, msg+9, MAX_ELEM_LENGTH-1);
		} 
		else
			if (strncmp( msg, "Parameter", 9) == 0 )
			{
				strcpy(type, "Parameter");
				strncpy(label, msg+10, MAX_ELEM_LENGTH-1);
			} 
			else
				if (strncmp( msg, "Function", 6) == 0 )
				{
					strcpy(type, "Function");
					strncpy(label, msg+9, MAX_ELEM_LENGTH-1);
				} 
				else
					return false;
	 
	if ( descr == NULL )
		app = descr = new description;
	else  
	{
		for ( app = descr; app->next != NULL; app = app->next );
		app->next = new description;
		app = app->next;
	} 
	app->next = NULL;
	app->text = app->init = NULL;
	app->label = new char[ strlen( label ) + 1 ];
	strcpy( app->label, label );
	app->type = new char[ strlen( type ) + 1 ];
	strcpy( app->type, type );

	strcpy(str1, "" );
	fgets(str, MAX_LINE_SIZE, f );//skip the first newline character
	for ( j = 0 ; fgets( str, MAX_LINE_SIZE, f ) != NULL && strncmp( str, "END_DESCRIPTION", 15 ) &&  strncmp( str, "_INIT_", 6 ) && strlen( str1 ) < 9*MAX_LINE_SIZE && j < MAX_FILE_TRY ; ++j )
		strcat(str1, str);

	if ( strncmp( str, "END_DESCRIPTION", 15 ) && strncmp( str, "_INIT_", 6 ) )
		return false;

	kill_trailing_newline(str1);

	app->text=new char[strlen(str1)+1];
	strcpy(app->text, str1);

	if ( ! strncmp( str, "_INIT_", 6 ) )
	{
		strcpy(str1, "" );
		for ( j = 0 ; fgets( str, MAX_LINE_SIZE, f ) != NULL && strncmp( str, "END_DESCRIPTION", 15 ) && strlen( str1 ) < 9*MAX_LINE_SIZE && j < MAX_FILE_TRY ; ++j )
			strcat(str1, str);

		if ( strncmp( str, "END_DESCRIPTION", 15 ) )
			return false;

		kill_trailing_newline(str1);
		app->init=new char[strlen(str1)+1];
		strcpy(app->init, str1);
	}
	else
	{
		app->init=new char[ 1 ];
		strcpy(app->init, "" );
	}
	app->initial='n';
	app->observe='n';  

	return true;
} 


/*****************************************************************************
EMPTY_DESCR
******************************************************************************/
void empty_description( void )
{
	description *cur, *cur1;
	for ( cur1 = descr; cur1 != NULL; cur1 = cur )
	{
		cur = cur1->next;
		delete [ ] cur1->label;
		delete [ ] cur1->type;
		delete [ ] cur1->text;
		delete [ ] cur1->init;
		delete cur1;
	}
	descr = NULL;
}


/*****************************************************************************
SAVE_DESCRIPTION
******************************************************************************/
void save_description(object *r, FILE *f )
{
	object *cur;
	variable *cv;
	description *cd;
	bridge *cb;

	cd=search_description(r->label);
	if (cd == NULL )
	  {
	   add_description(r->label, "Object", "(no description available)");
	   plog( "\nWarning: description for '%s' not found. New one created.", "", r->label );
	   cd=search_description(r->label);
	  } 

	if (cd->init == NULL )     
	  fprintf(f, "%s_%s\n%s\nEND_DESCRIPTION\n\n",cd->type,cd->label,cd->text);
	else
	  fprintf(f, "%s_%s\n%s\n_INIT_\n%s\nEND_DESCRIPTION\n\n",cd->type,cd->label,cd->text, cd->init);

	for (cv=r->v; cv!=NULL; cv=cv->next)
	 {
	  cd=search_description(cv->label);
	  if (cd == NULL )
	  {if (cv->param == 0 )
		 add_description(cv->label, "Variable", "(no description available)");
	   if (cv->param == 1 )
		 add_description(cv->label, "Parameter", "(no description available)");  
	   if (cv->param==2)
		 add_description(cv->label, "Function", "(no description available)");  
	   add_description(cv->label, "Object", "(no description available)");
	   plog( "\nWarning: description for '%s' not found. New one created.", "", cv->label );
	   cd=search_description(cv->label);
	  } 

	  if (cd->init == NULL )     
		fprintf(f, "%s_%s\n%s\nEND_DESCRIPTION\n\n",cd->type,cd->label,cd->text);
	  else
		fprintf(f, "%s_%s\n%s\n_INIT_\n%s\nEND_DESCRIPTION\n\n",cd->type,cd->label,cd->text, cd->init);
	   
	 }

	for ( cb = r->b; cb != NULL; cb = cb->next )
		if ( cb->head != NULL )
			save_description( cb->head, f );
}


/*****************************************************************************
SET_BLUEPRINT
	copy the naked structure of the model into another object, called blueprint, 
	to be used for adding objects without example
******************************************************************************/
void set_blueprint( object *container, object *r )
{
	int temp;
	object *cur, *cur1;
	variable *cv;
	bridge *cb, *cb1;
	
	if ( r == NULL )
		return;

	for ( cv = r->v; cv != NULL; cv = cv->next )
		container->add_var_from_example( cv );
	
	delete [ ] container->label;
	
	container->label = new char[ strlen( r->label ) + 1 ];
	strcpy( container->label, r->label );

	for ( cb = r->b; cb != NULL; cb = cb->next )
	{
		if ( cb->head == NULL )
			continue;
		
		cur1 = cb->head;
		container->add_obj( cur1->label, 1, 0 );
		for ( cb1 = container->b; strcmp( cb1->blabel, cb->blabel ); cb1 = cb1->next );
		cur = cb1->head;
		set_blueprint( cur, cur1 );
	}
}


/*****************************************************************************
LOAD_CONFIGURATION
	Load current defined configuration
	If reload is true, just the structure and the parameters are retrieved
	Returns: 0: load ok, 1,2,3,4,...: load failure
******************************************************************************/
int load_configuration( object *r, bool reload )
{
	int i, j = 0, load = 0;
	char msg[ MAX_LINE_SIZE ], full_name[ 2 * MAX_PATH_LENGTH ];
	object *cur;
	variable *cur_var, *cur_var1;
	description *cur_descr;
	FILE *f, *g;
	
	for ( cur = r; cur->up != NULL; cur = cur->up );
	r = cur;
	
	r->empty( );
	r->init( NULL, "Root" );
	empty_description( );
	add_description( "Root", "Object", "(no description available)" );
	blueprint->empty( );
	blueprint->init( NULL, "Root" );
	empty_cemetery( );
	nodesSerial = 0;							// restart network nodes serial counter

	if ( ! struct_loaded )
	{
		delete [ ] struct_file;
		if ( strlen( path ) > 0 )
		{
			struct_file = new char[ strlen( path ) + strlen( simul_name ) + 6 ];
			sprintf( struct_file, "%s/%s.lsd", path, simul_name );
		}
		else
		{
			struct_file = new char[ strlen( simul_name ) + 6 ];
			sprintf( struct_file, "%s.lsd", simul_name );
		}
	}
	
	f = fopen( struct_file, "rt" );
	if ( f == NULL )
		return 1;

	struct_loaded = r->load_struct( f );
	if ( ! struct_loaded )
	{
		load = 2;
		goto endLoad;
	}
	
	strcpy( msg, "" );
    fscanf( f, "%999s", msg );					//should be DATA
	if ( ! ( ! strcmp( msg, "DATA" ) && r->load_param( struct_file, 1, f ) ) )
	{
		load = 3;
		goto endLoad;
	}
	
	if ( reload )								// just quick reload?
		goto endLoad;
		
	sim_num = 1;
	fscanf( f, "%999s", msg );					//should be SIM_NUM 
	if ( ! ( ! strcmp( msg, "SIM_NUM" ) && fscanf( f, "%d", &sim_num ) ) )
	{
		load = 4;
		goto endLoad;
	}
	
	seed = 1;
	fscanf( f, "%999s", msg );					//should be SEED
	if ( ! ( ! strcmp( msg, "SEED" ) && fscanf( f, "%d", &seed ) ) )
	{
		load = 5;
		goto endLoad;
	}
	
	max_step = 100;
	fscanf( f, "%999s", msg );					//should be MAX_STEP
	if ( ! ( ! strcmp( msg, "MAX_STEP" ) && fscanf( f, "%d", &max_step ) ) )
	{
		load = 6;
		goto endLoad;
	}

	fscanf( f, "%999s", msg );					//should be EQUATION
	if ( strcmp( msg, "EQUATION" ) )
	{
		load = 7;
		goto endLoad;
	}
	strcpy( msg, "NONE" );
	fgets( msg, MAX_PATH_LENGTH - 1, f );
    if ( msg[ strlen( msg ) - 1 ] == '\n' )
		msg[ strlen( msg ) - 1 ] = '\0';
    if ( msg[ strlen( msg ) - 1 ] == '\r' )
		msg[ strlen( msg ) - 1 ] = '\0';

	// use the current equation name only if the file exists
	snprintf( full_name, 2 * MAX_PATH_LENGTH - 1, "%s/%s", exec_path, msg + 1 );
	g = fopen( full_name, "r" );
	if ( g != NULL )
	{
		fclose( g );
		strncpy( equation_name, msg + 1, MAX_PATH_LENGTH - 1 );
	}
	
	fscanf( f, "%999s", msg );					//should be MODELREPORT
	if ( ! ( ! strcmp( msg, "MODELREPORT" ) && fscanf( f, "%499s", name_rep ) ) )
	{
		load = 8;
		goto endLoad;
	}

	fscanf( f, "%999s", msg );					//should be DESCRIPTION
	if ( strcmp( msg, "DESCRIPTION" ) )
	{
		load = 9;
		goto endLoad;
	}  
	
	i = fscanf( f, "%999s", msg );				//should be the first description   
	for ( j = 0; strcmp( msg, "DOCUOBSERVE" ) && i == 1 && j < MAX_FILE_TRY; ++j )
	{ 
		i = load_description( msg, f );
		if ( ! fscanf( f, "%999s", msg ) ) 
			i = 0;
	}
	
	if ( i == 0 || j >= MAX_FILE_TRY )
	{
		load = 10;
		goto endLoad;
	} 
	
	fscanf( f, "%999s", msg );  
	for ( j = 0; strcmp( msg, "END_DOCUOBSERVE" ) && j < MAX_FILE_TRY; ++j )
	{
		cur_descr = search_description( msg );
		if ( cur_descr != NULL )
		{
			cur_descr->observe = 'y';
			cur_var = r->search_var( NULL, msg );
			if ( cur_var != NULL )
				for ( cur = cur_var->up; cur != NULL; cur = cur->hyper_next( cur_var->up->label ) )
				{
					cur_var1 = cur->search_var( NULL, cur_var->label );
					if ( cur_var1 != NULL )
						cur_var1->observe = true;
				}
		}
		fscanf( f, "%999s", msg );
	}
	
	if ( j >= MAX_FILE_TRY )
	{
		load = 11;
		goto endLoad;
	} 
	
	fscanf( f, "%999s", msg );  				//should be the DOCUINITIAL
	if ( strcmp( msg, "DOCUINITIAL" ) )
	{
		load = 12;
		goto endLoad;
	}  
	
	fscanf( f, "%999s", msg );  
	for ( j = 0; strcmp( msg, "END_DOCUINITIAL" ) && j < MAX_FILE_TRY; ++j )
	{
		cur_descr = search_description( msg );
		if ( cur_descr != NULL )
			cur_descr->initial = 'y';
		fscanf( f, "%999s", msg );
	}
	
	if ( j >= MAX_FILE_TRY )
	{
		load = 13;
		goto endLoad;
	} 
	
	fscanf( f, "%999s\n", msg );				// here is the equation file
	if ( strcmp( msg, "EQ_FILE" ) )
	{
		load = 0;								// optional
		goto endLoad;
	}

	strcpy( lsd_eq_file, "" );
	for ( j = 0; fgets( msg, MAX_LINE_SIZE - 1, f ) != NULL && strncmp( msg, "END_EQ_FILE", 11 ) && strlen( lsd_eq_file ) < MAX_FILE_SIZE - MAX_LINE_SIZE && j < MAX_FILE_TRY; ++j )
		strcat( lsd_eq_file, msg );
	
	// remove extra \n and \r (Windows) at the end
    if ( lsd_eq_file[ strlen( lsd_eq_file ) - 1 ] == '\n' )
		lsd_eq_file[ strlen( lsd_eq_file ) - 1 ] = '\0';
    if ( lsd_eq_file[ strlen( lsd_eq_file ) - 1 ] == '\r' )
		lsd_eq_file[ strlen( lsd_eq_file ) - 1 ] = '\0';
	
	if ( ! ignore_eq_file && strcmp( lsd_eq_file, eq_file ) )
	{
		plog( "\nWarning: the configuration file has been previously run with different equations\nfrom those used to create the LSD model program.\nChanges may affect the simulation results. You can offload the original\nequations in a new equation file and compare differences using TkDiff in LMM\n(menu File)." );
	}  
	
endLoad:
	fclose( f );
	
	t = 0;
	
#ifndef NO_WINDOW
	unsaved_change( false );
#endif

	return load;
}


/*********************************
SAVE_SINGLE
*********************************/
void save_single(variable *vcv)
{
	FILE *f;
	int i;

#ifdef PARALLEL_MODE
	// prevent concurrent use by more than one thread
	lock_guard < mutex > lock( vcv->parallel_comp );
#endif	
	set_lab_tit(vcv);
	sprintf( msg, "%s_%s-%d_%d_seed-%d.res", vcv->label, vcv->lab_tit, vcv->start,vcv->end,seed-1);
	f = fopen( msg, "wt");  // use text mode for Windows better compatibility

	fprintf(f, "%s %s (%d %d)\t\n",vcv->label, vcv->lab_tit, vcv->start, vcv->end);

	for ( i = 0; i<=t-1; ++i )
	 {
	  if (i>=vcv->start && i <=vcv->end && !is_nan(vcv->data[ i ]))		// save NaN as n/a
		fprintf(f,"%lf\t\n",vcv->data[ i ]);
	  else
		fprintf(f,"%s\t\n", nonavail);
	  }
	  
	fclose( f ); 
}


/*****************************************************************************
SAVE_CONFIGURATION
	Save current defined configuration (renaming if appropriate)
	Returns: true: save ok, false: save failure
******************************************************************************/
bool save_configuration( object *r, int findex )
{
	int indexDig = ( findex > 0 ) ? ( int ) floor( log10( findex ) + 2 ) : 0;
	object *cur;
	description *cur_descr;
	
	delete [ ] struct_file;
	
	if ( strlen( path ) > 0 )
	{
		struct_file = new char[ strlen( path ) + strlen( simul_name ) + 6 + indexDig ];
		sprintf( struct_file, "%s/%s", path, simul_name );
	}
	else
	{
		struct_file = new char[ strlen( simul_name ) + 6 + indexDig ];
		sprintf( struct_file, "%s", simul_name );
	}
	if ( findex > 0 )
		sprintf( struct_file, "%s_%d.lsd", struct_file, findex );
	else
		sprintf( struct_file, "%s.lsd", struct_file );
	
	FILE * f = fopen( struct_file, "w" );
	if ( f == NULL )
		return false;

	for ( cur = r; cur->up != NULL; cur = cur->up );
	
	cur->save_struct( f, "" );
	fprintf( f, "\nDATA\n" );
	cur->save_param( f );
	
	int delta = ( findex > 0 ) ? sim_num * ( findex - 1 ) : 0;
	fprintf( f, "\nSIM_NUM %d\nSEED %d\nMAX_STEP %d\nEQUATION %s\nMODELREPORT %s\n", sim_num, seed + delta, max_step, equation_name, name_rep );
	
	fprintf( f, "\nDESCRIPTION\n\n" );
	save_description( cur, f );
	
	fprintf( f, "\nDOCUOBSERVE\n" );
	for ( cur_descr = descr; cur_descr != NULL; cur_descr = cur_descr->next )
		if ( cur_descr->observe == 'y' )   
			fprintf( f, "%s\n", cur_descr->label );
	fprintf( f, "\nEND_DOCUOBSERVE\n\n" );
	
	fprintf( f, "\nDOCUINITIAL\n" );
	for ( cur_descr = descr; cur_descr != NULL; cur_descr = cur_descr->next )
		if ( cur_descr->initial == 'y' )     
			fprintf( f, "%s\n", cur_descr->label );
	fprintf( f, "\nEND_DOCUINITIAL\n\n" );
	
	save_eqfile( f );
	
	fclose( f );
	
#ifndef NO_WINDOW
	if ( findex <= 0 )
		unsaved_change( false );		// no changes to save
#endif
	
	return true;
}


/*****************************************************************************
LOAD_SENSITIVITY
	Load defined sensitivity analysis configuration
	Returns: 0: load ok, 1,2,3,4,...: load failure
******************************************************************************/
int load_sensitivity( object *r, FILE *f )
{
	int i;
	char cc, lab[ MAX_ELEM_LENGTH ];
	object *n;
	variable *cv;
	sense *cs;
	
	// read data from file (1 line per element, '#' indicate comment)
	while ( ! feof( f ) )
	{	// read element by element, skipping comments
		fscanf( f, "%99s", lab );			// read string
		while ( lab[ 0 ] == '#' )			// start of a comment
		{
			do								// jump to next line
				cc = fgetc( f );
			while ( ! feof( f ) && cc != '\n' );
			fscanf( f, "%99s", lab );		// try again
		}

		if ( feof( f ) )					// ended too early?
			break;

		for ( n = r; n->up != NULL; n = n->up );// check if element exists
		cv = n->search_var( n, lab );
		if ( cv == NULL || ( cv->param != 1 && cv->num_lag == 0 ) )
			goto error1;					// and not parameter or lagged variable
		
		// create memory allocation for new variable		
		if ( rsense == NULL )				// allocate first element
			rsense = cs = new sense;
		else								// allocate next ones
		{
			cs->next = new sense;
			cs = cs->next;
		}
		cs->v = NULL;						// initialize struct pointers
		cs->next = NULL;

		cs->label = new char[ strlen( lab ) + 1 ];  // save element name
		strcpy( cs->label, lab );
		
		// get lags and # of values to test
		if ( fscanf( f, "%d %d ", &cs->lag, &cs->nvalues ) < 2 )
			goto error2;
					
		// get variable type (newer versions)
		if ( fscanf( f, "%c ", &cc ) < 1 )
			goto error3;

		if ( cc == 'i' || cc == 'd' || cc == 'f' )
		{
			cs->integer = ( cc == 'i' ) ? true : false;
			fscanf( f, ": " );	 			// remove separator
		}
		else
			if ( cc == ':' )
				cs->integer = false;
			else
				goto error4;				
		
		if ( cs->lag == 0 )					// adjust type and lag #
			cs->param = 1;
		else
		{
			cs->param = 0;
			cs->lag = abs( cs->lag ) - 1;
		}

		cs->v = new double[ cs->nvalues ];	// get values
		for ( i = 0; i < cs->nvalues; ++i )
			if ( ! fscanf( f, "%lf", &cs->v[ i ] ) )
				goto error5;
			else
				if ( cs->integer )
					cs->v[ i ] = round( cs->v[ i ] );
	}	

	return 0;
	
	// error handling
	error1:
		i = 1;
		goto error;
	error2:
		i = 2;
		goto error;
	error3:
		i = 3;
		goto error;
	error4:
		i = 4;
		goto error;
	error5:
		i = 5;
		goto error;
		
	error:
	empty_sensitivity( rsense );		// discard read data
	rsense = NULL;
		
	return i;
}


/*****************************************************************************
EMPTY_SENSITIVITY
	Deallocate sensitivity analysis memory
******************************************************************************/
void empty_sensitivity( sense *cs )
{
	if ( cs == NULL )		// prevent invalid calls (last variable)
		return;
	
	if ( cs->next != NULL )	// recursively start from the end of the list
		empty_sensitivity( cs->next );
#ifndef NO_WINDOW
	else
		NOLH_clear( );		// deallocate DoE (last object only)
#endif
	if ( cs->v != NULL )	// deallocate requested memory, if applicable
		delete cs->v;
	if ( cs->label != NULL )
		delete cs->label;

	delete cs;				// suicide
}


/*****************************************************************************
SAVE_SENSITIVITY
	Save current sensitivity configuration
	Returns: true: save ok, false: save failure
******************************************************************************/
bool save_sensitivity( FILE *f )
{
	int i;
	sense *cs;

	for ( cs = rsense; cs != NULL; cs = cs->next )
	{
		if ( cs->param == 1 )
			fprintf( f, "%s 0 %d %c:", cs->label, cs->nvalues, cs->integer ? 'i' : 'f' );	
		else
			fprintf( f, "%s -%d %d %c:", cs->label, cs->lag + 1, cs->nvalues, cs->integer ? 'i' : 'f' );
		for ( i = 0; cs->v != NULL && i < cs->nvalues; ++i )
			fprintf( f," %g", cs->v[ i ] );
		fprintf( f,"\n" );
	}
	
	return ! ferror( f );
}
