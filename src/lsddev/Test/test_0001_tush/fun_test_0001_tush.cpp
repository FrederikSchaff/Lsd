//#define EIGENLIB			// uncomment to use Eigen linear algebra library

#include "fun_head_fast.h"

#define TUSH_MODEL "test_0001_tush"
#include "../tush_test_utilities.h"

// do not add Equations in this area

MODELBEGIN

// insert your equations here, ONLY between the MODELBEGIN and MODELEND words

EQUATION("Test")
/*
Comment
*/
	std::string logEntry = "Test log. Step: " + std::to_string(t) + ".";
	plog(std::string(logEntry + '\n').c_str());
	TEST_UNCHANGED(logEntry);
RESULT(0)









MODELEND

// do not add Equations in this area

void close_sim( void )
{
	// close simulation special commands go here
}
