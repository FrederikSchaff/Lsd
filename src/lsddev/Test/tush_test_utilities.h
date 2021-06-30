#ifndef LSD_TUSH_TEST_UTILITIES_H
#define LSD_TUSH_TEST_UTILITIES_H
/*
    This file contains simple helper utilities for testing LSD Macros with Tush.
    See <LSD>/dev/3_regression_tests.md for more details.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <deque>

#define TUSH_LOG "tushTestLog.log"
#ifndef TUSH_MODEL
    #define TUSH_MODEL "model name missing"
#endif
#define TEST_UNCHANGED(ENTRY) LSD_TUSH_TEST_UTILITIES::tushTest.addLog(ENTRY)
#define TEST_UNCHANGED_PLOG(ENTRY) \
    std::string __logEntry = std::string(ENTRY); \
    plog( (__logEntry + "\n").c_str()); \
    LSD_TUSH_TEST_UTILITIES::tushTest.addLog(__logEntry)

/**************************
 * TushTest allows to print prepared strings to the tush log. 
 * Every statement printed here can be regression tested.
 * */
namespace LSD_TUSH_TEST_UTILITIES {

    class TushTest {
    private:
        std::deque<std::string> tushLogs;
        std::string modelName;

    public:
        TushTest() : modelName{TUSH_MODEL} {}

        ~TushTest() {
            std::ofstream tushLog;
            tushLog.open(TUSH_LOG, std::ios_base::out | std::ios_base::trunc);
            if (tushLog.is_open()) {
                tushLog << "Begin of test logs for \'" + modelName + "\'\n\n\n";
                for (std::string logEntry : tushLogs) {
                    tushLog << logEntry << '\n';
                }
                tushLog << "\n\nEnd of test logs for \'" + modelName + "\'\n";
            }
            tushLog.close();
        }

        void addLog(std::string logStatement) {
            tushLogs.push_back(logStatement);
        }
    };

    TushTest tushTest;
}
#endif