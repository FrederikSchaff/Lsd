//#define EIGENLIB			// uncomment to use Eigen linear algebra library

#include "fun_head_fast.h"

#define TUSH_MODEL "Test_0001_tush"
#include "../tush_test_utilities.cpp"

// do not add Equations in this area

MODELBEGIN

// insert your equations here, ONLY between the MODELBEGIN and MODELEND words

EQUATION("Test")
/*
Comment
*/

	std::string logEntry = "Test log. Step: " + std::to_string(t) + ".";
	plog(std::string(logEntry + '\n').c_str());
	tushTest.addLog(logEntry);
RESULT(0)









MODELEND

// do not add Equations in this area

void close_sim( void )
{
	// close simulation special commands go here
}
