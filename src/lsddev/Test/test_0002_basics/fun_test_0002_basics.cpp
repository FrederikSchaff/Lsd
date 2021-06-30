//#define EIGENLIB			// uncomment to use Eigen linear algebra library

#include "fun_head_fast.h"

#define TUSH_MODEL "test_0002_basics"
#include "../tush_test_utilities.h"

// do not add Equations in this area

MODELBEGIN

// insert your equations here, ONLY between the MODELBEGIN and MODELEND words

EQUATION("Test")
/*
Comment
*/
	std::string logEntry = "Test log. Step: " + std::to_string(t) + ".";	
	TEST_UNCHANGED_PLOG(logEntry);

	V("Init");

	//Test if routine is correct.
	
RESULT(0)

EQUATION("Init")
/*
Initialises the model
*/	
	std::string logEntry = "Initialising Agent tree.";
    TEST_UNCHANGED_PLOG(logEntry);

//Two children, one exists already
	ADDOBJ("child");
	
	//Each child has three parents, one exists already
	int c_par = 0;
	CYCLE(cur,"child"){		
		WRITES(cur,"c",++c_par*100); //Names: A=100, B=200, ...
		TEST_UNCHANGED_PLOG("Added agent " + std::to_string(c_par*100) + "=" + std::to_string( VS(cur,"c")) );

		ADDNOBJS(cur,"parent",2);		
		
		//Each parent has 2 grandparents, one exists already
		int p_par = 0;
		CYCLES(cur,cur1,"parent"){			
			
			WRITES(cur1,"p",c_par*100 + ++p_par*10); //Names: AA=110, AB=120, BA=210, ...
			TEST_UNCHANGED_PLOG("Added parent " + std::to_string(p_par*10 + c_par*100) + "=" + std::to_string( VS(cur1,"p")) );

			ADDOBJS(cur1,"grandparent");
			
			int g_par = 0;
			CYCLES(cur1,cur2,"grandparent"){
				WRITES(cur2,"g",c_par*100 + p_par*10 + ++g_par);
				TEST_UNCHANGED_PLOG("Added grandparent " + std::to_string(g_par + p_par*10 + c_par*100) + "=" + std::to_string( VS(cur2,"g")) );
			}
		}
	}
PARAMETER //execute exactly once.	
RESULT(0)


EQUATION("CYCLE_TEST"){
/*
Test that the cycling works as intended and parameters keep unchanged.
Also tests that Init was called first.
*/	
//Each child has three parents, one exists already
	int c_par = 0;
	CYCLE(cur,"child"){		
		++c_par;		
		TEST_UNCHANGED_PLOG("Find agent " + std::to_string(c_par*100) + "=" + std::to_string( VS(cur,"c")) );

		//Each parent has 2 grandparents, one exists already
		int p_par = 0;
		CYCLES(cur,cur1,"parent"){			
			++p_par;			
			TEST_UNCHANGED_PLOG("Find parent " + std::to_string(p_par*10 + c_par*100) + "=" + std::to_string( VS(cur1,"p")) );
			
			int g_par = 0;
			CYCLES(cur1,cur2,"grandparent"){
				++g_par;				
				TEST_UNCHANGED_PLOG("Find grandparent " + std::to_string(g_par + p_par*10 + c_par*100) + "=" + std::to_string( VS(cur2,"g")) );
			}
		}
	}
}
RESULT(0)








MODELEND

// do not add Equations in this area

void close_sim( void )
{
	// close simulation special commands go here
}
