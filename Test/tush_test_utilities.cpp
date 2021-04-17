/*
    This file contains simple helper utilities for testing LSD Macros with Tush.
    See <LSD>/dev/3_regression_tests.md for more details.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <deque>

using std::string;
using std::deque;
using std::ofstream;
using std::ios_base;

#define TUSH_LOG "tushTestLog.log"
#ifndef TUSH_MODEL
    #define TUSH_MODEL "model name missing"
#endif

/**************************
 * TushTest allows to print prepared strings to the tush log. 
 * Every statement printed here can be regression tested.
 * */

class TushTest {
    private:
    deque<string> tushLogs;
    string modelName;

    public:
    TushTest() : modelName{TUSH_MODEL} {}

    ~TushTest() {
        ofstream tushLog;
        tushLog.open(TUSH_LOG,ios_base::out | ios_base::trunc);
        if (tushLog.is_open()){
            tushLog << "Begin of test logs for \'" + modelName + "\'\n\n\n";
            for (string logEntry : tushLogs){
                tushLog << logEntry << '\n';
            } 
            tushLog << "\n\nEnd of test logs for \'" + modelName + "\'\n";
        }
        tushLog.close();
    }

    void addLog(string logStatement) {
        tushLogs.push_back(logStatement);
    }
};

TushTest tushTest;


