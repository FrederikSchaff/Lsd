/*************************************************************

	LSD 7.1 - May 2018
	written by Marco Valente, Universita' dell'Aquila
	and by Marcelo Pereira, University of Campinas

	Copyright Marco Valente
	LSD is distributed under the GNU General Public License
	
 *************************************************************/

/****************************************************
VARIAB.CPP
The (C++) object variable is devoted to contain numerical values
of the model. Only double precision floating point numbers are considered
in LSD.
Variables are mainly storages for information. Actually, all the work is
done by LSD objects.
The most important task of variables is to return their value when requested.
It is done by comparing the global time of the simulation with the time
the variable was most recently updated. If the value requested (considering the
lag ) is already available, that is returned. Otherwise, the variable shifts
its lagged values, and calls its equation to compute the new value.
All fields and functions in variables are public, so that users may override
the default mechanism.
The fields composing a variables are

- char *label;
name of the variable. It needs to be unique in the model

- object *up;
address of the object containing the variable

- variable *next;
pointer to the next variable contained in the object. Variables in an object
are organized as a linked chain, and can be reached only  via their fields
next

- double *val;
vector of numerical values. val[ 0 ] is the most recent value computed by the
equation, that is, computed at time last_update. val[1] is the value computed
at time last_update - 1; val[2] at time last_update - 2 and so on.

- int num_lag;
number of lagged values stored for the variable

- int save;
flag identifying whether the variable has to be saved or not in the result file

- int plot;
Flag used to indicate variables that are plotted in the run time graph.

- char debug;
flag used to indicate the variables to debug. If this flag is equal 'd', when the
simulation is run in debug mode it stops immediately after the computation of
its value.

- int deb_cond;
Like the flag debug, but it stops the simulation if the attached coondition is
satisfied. It does not require that the simulation is run in debug mode.
Its different values represent the different conditions for stopping: <, > or ==

- double deb_cnd_val;
numerical value used for the conditional stop

- int under_computation;
control flag used to avoid infinite recursion of an equation calling itself.
Used to issue a message of error
- int last_update;
contain the global time when it was lastly computed the equation for the variable

- int param;
Flag set to 1, in case the variable is considered a parameter. In case it is,
when requested the value it is always returned its field val[ 0 ].

- char data_loaded;
flag indicatibe whether the variable has been initiliazed with numerical values
set as default by the system or if they were actually chosen by the user.
The flag is 0 in case of newly created objects and 1 in case the variable's
values has been at least shown once in the initial values editor window.
This flag is also saved in the data file, so thatthis information is not lost.
The flag prevents to run a simulation if the data where not confirmed by users.

- char computable;
Flag not currently used.



The methods of the (C++) object variable are:

- int init(object *_up, char *_label, int _num_lag, double *val, int _save);
perform the initialization.

- double cal(object *caller, int lag );
it is its main function. Return the numerical value

       val[last_update+lag-t]

if the condition

       t-lag<=last_update

is satisfied. That means that either the variable has already been updated,
and therefore the requested value is available, or that, though the variable
has not been still updated in the time step, the value requested is a lagged one
and therefore can be retrived from the vector of the past values.

Only in case the lag requested is zero and the variable has not been computed
at the present time step, the method shifts its lagged values and calls the method
fun
that perform the equation computation.

- void empty( void ) ;
It is used to free all the memory assigned to the variable. Used by
object::delete_obj to cancel an object.

In the file FUNXXX.CPP there is the code for the last method of variable.
This is the only file that is model specific, since it contains the equations
for the variables. The names are normally of the form FUNXXX.CPP, where XXX
is a code for the different models.

- double fun(object *caller);
It is a method common to all the variables and stores the code for the equations.
Each equation needs to be defined as a block like:

if (!strcmp(label, "LabelOfTheVariable"))
{
... here any code
res=avalue;
goto end;
}

The method is common to all the variables, so that the blocks ensure that
only their piece of code is actually executed. Any code legal in C++ is allowed,
including the methods and function provided with LSD.
The value assigned to "res" is assigned as val[ 0 ] to the variable.
The final line goto end; ensures that the equation has been computed.
See file FUNXXX.CPP for more information on this

Functions used here from other files are:

- void plog(char *m);
LSDMAIN.CPP print  message string m in the Log screen.

- int deb(object *r, object *c, char *lab, double *res, bool interact);
DEBUG.CPP
activate the debugger.

****************************************************/

#include "decl.h"

clock_t start_profile[ 100 ], end_profile[ 100 ];

#ifdef PARALLEL_MODE
// semaphore to enable just a single parallel call at a time
atomic < bool > parallel_ready( true );
bool worker_ready = true;
bool worker_crashed = false;
condition_variable update;
mutex thr_ptr_lock;
mutex update_lock;
mutex crash_lock;
#endif	


/****************************************************
INIT
****************************************************/
int variable::init( object *_up, char const *_label, int _num_lag, double *v, int _save )
{
	int i;

#ifdef PARALLEL_MODE
	// prevent concurrent use by more than one thread
	lock_guard < mutex > lock( parallel_comp );
#endif	
	
	total_var++;

	up =_up;
	label = NULL;
	i = strlen(_label)+1;
	label = new char[ i ];
	strcpy( label, _label );
	param = 0;
	num_lag =_num_lag;
	if ( num_lag >= 0 )
	{
		val = new double[ num_lag + 1 ];
		for ( i = 0; i < num_lag + 1; i++ )
			val[ i ] = v[ i ];
	}
	else
		val = NULL;
	
	next = NULL;
	last_update = 0;
	save =_save;
	savei = false;
	under_computation = false;
	deb_cond = 0;
	deb_cnd_val = 0;
	data_loaded = '-';
	plot = false;
	parallel = false;
	observe = false;
	data = NULL;
	lab_tit = NULL;

	return 0;
}


/***************************************************
CAL
Standard version (non parallel computation)
****************************************************/
double variable::cal( object *caller, int lag )
{
	int i, eff_lag, time;
	clock_t start, end;
	double app;

	if ( param == 1 )
		return val[ 0 ];					//it is a parameter, ignore lags
	
#ifdef PARALLEL_MODE
	// prepare mutex for variables and functions updated in multiple threads
	unique_lock < mutex > guard( parallel_comp, defer_lock );
#endif	

	if ( param == 0 )					// it's a variable
	{
		// effective lag for variables (compatible with older versions)
		eff_lag = ( last_update < t ) ? lag - 1 : lag;

		// check lag error and return past value if available
		if ( lag != 0 )
		{	
			if ( eff_lag < 0 )			// with negative lag
				goto error;

			if ( eff_lag > num_lag )	// in principle, invalid lag
			{
				if ( ! ( save || savei ) )	// and not saved
					goto error;
				else
					if ( lag > t - start )	// or before there are saved values
						goto error;
				
				return data[ t - lag ];	// use saved past value				
			}
			else
				return val[ eff_lag ];	// use regular past value
		}
		else
		{
			if ( last_update >= t )		// already calculated this time step
				return( val[ 0 ] );		
#ifdef PARALLEL_MODE
			// prevent parallel computation of the same variable
			if ( parallel_mode )
				 guard.lock( );
			if ( last_update >= t )			// recheck if not computed during lock
				return( val[ 0 ] );		
#endif	
		}
	}
	else								// function
	{
		if ( lag < 0 || lag > num_lag )	// with invalid lag
			goto error;
		
		if ( lag > 0 )					// lagged value
			return val[ lag - 1 ]; 

		if ( caller == NULL )			// update or inadequate caller
			return val[ 0 ];   

#ifdef PARALLEL_MODE
		// prevent parallel computation of the same function
		if ( parallel_mode )
			 guard.lock( );
#endif	
	}

	// there is a value to be computed

	if ( under_computation )
	{
		sprintf( msg, "equation for '%s' (object '%s') requested \nits own value while computing its current value", label, up->label );
		error_hard( msg, "dead-lock",
					"check your code to prevent this situation" );
		return 0;
	}

	under_computation = true;

#ifdef PARALLEL_MODE
	if ( fast_mode == 0 && ! parallel_mode )
#else
	if ( fast_mode == 0 )
#endif	
	{
		// add the Variable to the stack
		if ( stacklog != NULL && stacklog->next == NULL )
		{
			++stack;
			stacklog->next = new lsdstack;
			stacklog->next->next=NULL;
			stacklog->next->prev = stacklog;
			strcpy( stacklog->next->label, label );
			stacklog->next->ns = stack;
			stacklog->next->vs = this;
			stacklog = stacklog->next;
		} 
		else
		{
			sprintf( msg, "failure while pushing '%s' (object '%s')", label, up->label );
			error_hard( msg, "internal error",
						"if error persists, please contact developers" );
			return 0;
		}

#ifndef NO_WINDOW
		if ( stack_info >= stack && ( ! prof_obs_only || observe ) )
			start_profile[ stack - 1 ] = start = clock( );
		else
			if ( prof_aggr_time )
				start = clock( );				
#endif
	}
#ifndef NO_WINDOW
	else
		if ( prof_aggr_time )
			start = clock( );				
#endif	

	// Compute the Variable's equation
	user_exception = true;			// allow distinguishing among internal & user exceptions
	try 							// do it while catching exceptions to avoid obscure aborts
	{
		app = fun( caller );
	}
	catch ( exception& exc )
	{
		plog( "\n\nAn exception was detected while computing the equation \nfor '%s' requested by object '%s'", "", label, caller == NULL ? "(none)" : caller->label );
		quit = 2;
		throw;
	}
	catch ( int p )		// avoid general catch of error_hard() throwing to lsdmain()
	{
		throw p;
	}
	catch ( ... )
	{
		if ( quit != 2 )			// error message not already presented?
		{
			plog( "\n\nAn unknown problem was detected while computing the equation \nfor '%s' requested by object '%s'", "", label, caller == NULL ? "(none)" : caller->label );
			quit = 2;
			throw;
		}
		else
		{
			app = NAN;				// mark result as invalid
			use_nan = true;			// and allow propagation
		}
	}
	user_exception = false;

	for ( i = 0; i < num_lag; ++i ) 	// scale down the past values
		val[ num_lag - i ] = val[ num_lag - i - 1 ];
	val[ 0 ] = app;

	last_update = t;

#ifdef PARALLEL_MODE
	if ( fast_mode == 0 && ! parallel_mode )
#else
	if ( fast_mode == 0 )
#endif	
	{
#ifndef NO_WINDOW
		if ( prof_aggr_time )
		{
			end = clock( );
			time = end - start;
			
			if ( ( ! prof_obs_only || observe ) && time > prof_min_msecs )
			{
				string var_name = label;
				int a = prof[ var_name ].ticks, b = prof[ var_name ].comp;
				prof[ var_name ].ticks += time;
				prof[ var_name ].comp++;
				a = prof[ var_name ].ticks; b = prof[ var_name ].comp;
			}
		}

		if ( stack_info >= stack && ( ! prof_obs_only || observe ) )
		{
			end_profile[ stack - 1 ] = prof_aggr_time ? end : clock( );

			time = 1000 * ( end_profile[ stack - 1 ] - start_profile[ stack - 1 ] ) / CLOCKS_PER_SEC;

			if ( time >= prof_min_msecs )
			{
				set_lab_tit( this );
				plog( "\n%-12.12s(%-.10s)\t=", "prof1", label, lab_tit );
				plog( "%.4g\t", "highlight", val[ 0 ] );
				plog( "t=" );
				plog( "%d\t", "highlight", t );
				plog( "msecs=" );
				plog( "%d\t", "highlight", time );
				plog( "stack=" );
				plog( "%d\t", "highlight", stack );
				plog( "caller=%s%s%s", "", caller == NULL ? "SYSTEM" : caller->label, caller == NULL ? "" : "\ttrigger=", caller == NULL || stacklog == NULL || stacklog->prev == NULL ? "" : stacklog->prev->label );
			}
		}
		
		// update debug log file
		if ( log_file != NULL && t >= log_start && t <= log_stop )
			fprintf( log_file, "%s\t= %g\t(t=%d)\n", label, val[ 0 ], t );

		if ( debug_flag && t == when_debug && debug == 'd' && deb_cond == 0 )
			deb( ( object * ) up, caller, label, &val[ 0 ], true );
		else
			switch ( deb_cond )
			{
				case 0: 
					break;
				case 1: 
					if ( val[ 0 ] == deb_cnd_val )
						deb( ( object * ) up, caller, label, &val[ 0 ], true );
					break;
				case 2: 
					if ( val[ 0 ] > deb_cnd_val )
						deb( ( object * ) up, caller, label, &val[ 0 ], true );
					break;
				case 3: 
					if ( val[ 0 ] < deb_cnd_val )
						deb( ( object * ) up, caller, label, &val[ 0 ], true );
					break;
				default:
					sprintf( msg, "conditional debug '%d' in variable '%s'", deb_cond, label );
					error_hard( msg, "internal error", 
								"if error persists, please contact developers" );
					return -1;
			}
#endif
		// remove the element from the stack
		if ( stacklog != NULL && stacklog->prev != NULL )
		{
			stacklog = stacklog->prev;
			delete stacklog->next;
			stacklog->next = NULL;
			stack--;
		}
		else
		{
			sprintf( msg, "failure while poping '%s' (in object '%s')", label, up->label );
			error_hard( msg, "internal error", 
						"if error persists, please contact developers" );
			return 0;
		}
	}
	
	under_computation = false;

	return val[ 0 ];	//by default the requested value is the last one, not yet computed

	error:
	
	sprintf( msg, "variable or function '%s' (object '%s') requested \nwith lag=%d but declared with lag=%d\nThree possible fixes:\n- change the model configuration, declaring '%s' with at least lag=%d,\n- change the code of '%s' requesting the value of '%s' with lag=%d maximum, or\n- mark '%s' to be saved (variables only)", label, up->label, lag, num_lag, label, lag, caller == NULL ? "(none)" : caller->label, label, num_lag, label );
	error_hard( msg, "lag error", 
				"check your configuration or code to prevent this situation" );
	
	return 0;
}


#ifdef PARALLEL_MODE
/***************************************************
CAL_WORKER
Multi-thread worker for parallel computation
****************************************************/
void worker::cal_worker( void )
{
	int i;
	double app;

	// create try-catch block to capture exceptions in thread and reroute to main thread
	try
	{
		running = true;
		
		// update object map and register all signal handlers
		unique_lock< mutex > lock_map( thr_ptr_lock );
		thr_id = this_thread::get_id( );
		thr_ptr[ thr_id ] = this;
		lock_map.unlock( );
		handle_signals( signal_wrapper );
		
		free = true;
	
		while ( running )
		{
			// wait for variable calculation message
			unique_lock< mutex > lock_worker( lock );
			run.wait( lock_worker, [ this ]{ return ! free; }  );
			
			// exit if shutdown or continue if already updated
			if ( running && var != NULL && var->last_update < t )
			{	// prevent parallel computation of the same variable
				lock_guard < mutex > lock_var( var->parallel_comp );
				
				// recheck if not computed during lock
				if ( var->last_update >= t )			
					goto end;		

				if ( var->under_computation )
				{
					sprintf( err_msg1, "Dead-lock" );
					sprintf( err_msg2, "the equation for '%s' in object '%s' requested its own value\nwhile parallel-computing its current value", var->label, var->up->label );
					sprintf( err_msg3, "Check your code to prevent this situation." );
					user_excpt = true;
					throw;
				}

				var->under_computation = true;

				// compute the Variable's equation
				user_excpt = true;			// allow distinguishing among internal & user exceptions
				try 						// do it while catching exceptions to avoid obscure aborts
				{
					app = var->fun( NULL );
				}
				catch ( ... )
				{
					pexcpt = current_exception( );
					sprintf( err_msg1, "Equation error" );
					sprintf( err_msg2, "an exception was detected while parallel-computing the equation\nfor '%s' in object '%s'", var->label, var->up->label );
					sprintf( err_msg3, "Check your code to prevent this situation." );
					throw;
				}
				user_excpt = false;

				// scale down the past values
				for ( i = 0; i < var->num_lag; ++i )
					var->val[ var->num_lag - i ] = var->val[ var->num_lag - i - 1 ];
				var->val[ 0 ] = app;

				var->last_update = t;
				var->under_computation = false;
			}
			
		end:
			var = NULL;
			free = true;
			// create context to send signal to update scheduler if needed
			if ( ! worker_ready )
			{
				unique_lock< mutex > lock_update( update_lock );
				// recheck if still needed
				if ( ! worker_ready )
				{
					worker_ready = true;
					update.notify_one( );
				}
			}			
		}
	}
	catch ( ... )
	{
		// only capture exception if not already done
		if ( pexcpt != nullptr )
		{
			pexcpt = current_exception( );
			sprintf( err_msg1, "Unknown error" );
			sprintf( err_msg2, "an exception was detected while parallel-computing the equation\nfor '%s' in object '%s'", var->label, var->up->label );
			sprintf( err_msg3, "Disable parallel computation for this variable\nor check your code to prevent this situation." );
		}
	}
	
	free = false;
	running = false;	
}


/***************************************************
WORKER constructor
****************************************************/
worker::worker( void )
{
	running = false;
	free = false;
	pexcpt = nullptr;
	signum = -1;
	var = NULL;
	strcpy( err_msg1, "" );
	strcpy( err_msg2, "" );
	strcpy( err_msg3, "" );

	// launch new thread (waiting mode)
	thr = thread( & worker::cal_worker, this );
}


/***************************************************
WORKER destructor
****************************************************/
worker::~worker( void )
{
	// command thread shutdown if running
	if ( running )
	{
		unique_lock< mutex > lock_worker( lock );
		running = free = false;	
		run.notify_one( );
	}
	
	// wait for shutdown and check exception
	if ( thr.joinable( ) )
		thr.join( );
	
	// remove thread id from threads map
	thr_ptr.erase( thr_id );
}


/***************************************************
SIGNAL
Handle system signals in worker
****************************************************/
void worker::signal( int sig )
{
	if ( var != NULL && var->label != NULL  )
		sprintf( err_msg1, "\n\nUnknown error: signal received while parallel-computing the equation\nfor '%s' in object '%s'. Disable parallel computation for this variable\nor check your code to prevent this situation.", var->label, var->up->label != NULL ? var->up->label : "(none)" );
	else
		sprintf( err_msg1, "\n\nUnknown error: signal received by a parallel worker thread.\nDisable parallel computation to prevent this situation." );

	// signal & kill thread
	signum = sig;
	free = false;
	running = false;
}


/***************************************************
SIGNAL_WRAPPER
Reformat signal function format to comply with OS
****************************************************/
void worker::signal_wrapper( int signum )
{
	// pointer to the appropriate worker object 
	worker *me = thr_ptr[ this_thread::get_id( ) ];
	
	// call member function
	me->signal( signum );
}


/***************************************************
CAL
Multi-thread CAL version (parallel computation)
****************************************************/
void worker::cal( variable *v )
{
	unique_lock< mutex > worker_lock( lock );
	var = v;
	free = false;
	run.notify_one( );
}


/****************************************************
CHECK
Check if worker is running and handle problems
****************************************************/
bool worker::check( void )
{
	if ( running )				// nothing to do?
		return true;
	
	// only process first worker crash
	lock_guard< mutex > lock_crash( crash_lock );
	if ( ! worker_crashed )
	{		
		worker_crashed = true;
		
		if ( signum >= 0 )
		{
			plog( err_msg1 );
			user_exception = user_excpt;
			signal_handler( signum );
		}
		else
		{
			if ( pexcpt != nullptr )
			{
				user_exception = user_excpt;
				error_hard( err_msg2, err_msg1, err_msg3 );
				rethrow_exception( pexcpt );
			}
			else
			{
				if ( var != NULL && var->label != NULL )
					sprintf( msg, "while computing variable '%s' (object '%s') a multi-threading worker crashed", var->label, var->up->label != NULL ? var->up->label : "(none)" );
				else
					sprintf( msg, "multi-threading worker crashed" );
				
				error_hard( msg, "parallel computation error", 
							"disable parallel computation for this variable\nor check your code to prevent this situation.\n\nPlease choose 'Quit LSD Browser' in the next dialog box" );
			}
		}
	}
	
	return false;
}


/***************************************************
PARALLEL_UPDATE
Multi-thread scheduler for parallel updating
****************************************************/
void parallel_update( variable *v, object* p, object *caller )
{
	bool ready[ max_threads ], wait = false;
	bridge *cb;
	clock_t start;
	int i, nt, wait_time;
	object *co;
	variable *cv;
	
	// find the beginning of the linked list chain for current object
	for ( cb = p->up->b; strcmp( cb->blabel, p->label ) && cb->next != NULL; cb = cb->next );
	
	// if problematic pointers or single instanced object, update as usual
	if ( cb == NULL || cb->head == NULL || cb->head->next == NULL )
	{
		v->cal( caller, 0 );
		return;
	}
	
	// prevent concurrent parallel update and multi-threading in a single core
	if ( parallel_ready && max_threads > 1 )
		parallel_ready = false;
	else
	{
		v->cal( caller, 0 );
		return;
	}
		
	// set ready worker threads
	for ( nt = 0, i = 0; i < max_threads; ++i )
	{
		ready[ i ] = workers[ i ].free;
		if ( ! ready[ i ] )
			++nt;
	}
	
	if ( nt > 0 )
	{
		sprintf( msg, "variable '%s' (object '%s') %d parallel worker(s) crashed", cv->label, cv->up->label, i );
		error_hard( msg, "multi-threading inconsistency", 
					"disable parallel computation for this variable or check your code to prevent this situation.\n\nPlease choose 'Quit LSD Browser' in the next dialog box" );
		return;
	}
				
	// scan all instances of current object under current parent
	for ( co = cb->head; co != NULL; co = co->next )
	{
		cv = co->search_var( co, v->label );
		
		// compute only if not updated
		if ( cv != NULL && cv->last_update < t )
		{
			// if no worker available, wait to free existing ones
			while ( nt >= max_threads )
			{
				// if starting wait, reset chronometer
				if ( ! wait )
				{
					wait = true;
					start = clock( );
				}
				else		// already waiting
				{
					wait_time = ( clock( ) - start ) / CLOCKS_PER_SEC;
					if ( wait_time > MAX_WAIT_TIME )
					{
						sprintf( msg, "variable '%s' (object '%s') took more than %d seconds\nwhile computing value for time %d", cv->label, cv->up->label, MAX_WAIT_TIME, t );
						error_hard( msg, "dead-lock in parallel computation", 
									"disable parallel computation for this variable or check your code to prevent this situation.\n\nPlease choose 'Quit LSD Browser' in the next dialog box" );
						return;
					}
				}
				
				// look for free and stopped workers
				for ( i = 0; i < max_threads; ++i )
				{
					workers[ i ].check( );
					
					if ( ! ready[ i ] && workers[ i ].free )
					{
						--nt;
						ready[ i ] = true;
						wait = false;
					}
				}
				
				// sleep process until first worker is free
				if ( nt >= max_threads )
				{
					unique_lock< mutex > lock_update( update_lock );
					worker_ready = false;
					if ( ! update.wait_for ( lock_update, chrono::milliseconds( MAX_TIMEOUT ), [ ]{ return ! worker_ready; } ) )
						{
							worker_ready = true;
							plog( "\nWarning: workers timeout (%d millisecs.), continuing...", "", MAX_TIMEOUT );
							break;
						}
				}
				
				// recheck running workers
				for ( i = 0; i < max_threads; ++i )
					workers[ i ].check( );
			}
		
			// find first free worker
			for ( i = 0; ! ready[ i ] && i < max_threads; ++i );
			// if something go wrong, wait fist worker (always there)
			if ( i >= max_threads )
			{
				sprintf( msg, "variable '%s' (object '%s') had a multi-threading inconsistency,\nmaybe a dead-lock state", cv->label, cv->up->label );
				error_hard( msg, "multi-threading inconsistency", 
							"disable parallel computation for this variable or check your code to prevent this situation.\n\nPlease choose 'Quit LSD Browser' in the next dialog box" );
				return;
			}
			else
			{	// start the computation of the current variable instance in the worker
				++nt;
				ready[ i ] = false;
				workers[ i ].cal( cv );
			}
		}
	}
	
	// wait last threads finish processing
	while ( nt > 0 )
	{	// if starting wait, reset chronometer
		if ( ! wait )
		{
			wait = true;
			start = clock( );
		}
		else		// already waiting
		{
			wait_time = ( clock( ) - start ) / CLOCKS_PER_SEC;
			if ( wait_time > MAX_WAIT_TIME )
			{
				sprintf( msg, "variable '%s' (object '%s') took more than %d seconds\nwhile computing value for time %d", cv->up->label, cv->label, MAX_WAIT_TIME, t );
				error_hard( msg, "dead-lock in parallel computation", 
							"disable parallel computation for this variable or check your code to prevent this situation.\n\nPlease choose 'Quit LSD Browser' in the next dialog box" );
				return;
			}
		}
		
		// wait till each thread finishes
		for ( i = 0; i < max_threads; ++i )
		{
			if ( ! ready[ i ] && workers[ i ].free )
			{
				--nt;
				ready[ i ] = true;
				wait = false;
			}
			
			// check worker problem
			workers[ i ].check( );
		}
	}
	
	// re-enable concurrent parallel update
	parallel_ready = true;
}
#endif


/****************************************************
EMPTY
****************************************************/
void variable::empty( void ) 
{
#ifdef PARALLEL_MODE
	// prevent concurrent use by more than one thread
	lock_guard < mutex > lock( parallel_comp );
#endif	
		
	if ( ( data != NULL && save != true && savei != true ) || label == NULL )
	{
		sprintf( msg, "failure while deallocating variable %s", label );
		error_hard( msg, "internal error", 
					"if error persists, please contact developers" );
		return;
	}

	total_var--;
	
	delete [ ] label;
	delete [ ] data;
	delete [ ] lab_tit;
	delete [ ] val;
}
