/**************************************************************************
 *
 * $Id: //db/ed10-update1/tests/xa/tuxedo/testxaserver.cpp#1 $
 *
 * $$RW_INSERT_HEADER "dbyrs.str"
 *
 **************************************************************************
 *
 * Server program for Tuxedo application to test SourcePro DB XA
 * functionality.
 * It has server initialization code, shutdown code and the services,
 * which run on the server side.
 *
 **************************************************************************/

#include <rw/db/db.h>
#include <atmi.h>

#include <stdarg.h> //for va_start/va_end
#include <iostream> // for ostream

#ifndef AUTOTEST
#include "tututil.h"
#else
#include <rw/rwtest/rwtest.h>
#endif

/******************************************************************************/
// Macromagic to handle a windows quirk
// (windows runtime library prefixes *nprintf calls with _)

#ifdef rw_vsnprintf
#undef rw_vsnprintf
#endif

#ifdef _WIN32
#include <stdio.h> // for *printf()
#define rw_vsnprintf _vsnprintf
#else
#define rw_vsnprintf vsnprintf
#endif

/******************************************************************************/
// Utility macro definitions
// These macros are used to check if a condition is true, and report the 
// failure if the condition isn't.
// Unless specified, the macros assume the name of the RWDBXATester object
// used in the assertion checking is f_.
// The macros follow the following naming convention:
//     RWDB_ASSERT(|_RET|_F)
// The 'plain' version (no sufix) handles output of an error message if the 
//     check fails, then returns true if the assertion matched the expected
//     value (true or Tuxedo) or false if the assetion failed
// The _RET variant returns from the current function with a value of false if
//     the assertion failed
// The _F variant takes a printf/variable argument style format block rather 
//     than a string

#define RWDB_ASSERT(expr, msg) \
    f_->assertTrue(msg, __LINE__, (expr))

#define RWDB_ASSERT_RET(expr, msg) \
    if(!RWDB_ASSERT(expr, msg)) return false

#define RWDB_ASSERT_F(expr, VA_LIST)          \
    RWDB_ASSERT(expr, format VA_LIST)

/******************************************************************************/
// Constants and utility function.

// format() converts a format string and parameters into a char* for cases
// where a function versions that takes a format string isn't available.
// As this uses an internal temporary buffer, any contents will be overwritten
// the next time it is called, so the contents should be copied immedietly.
const char* format (const char* fmat, ...)
{
    static char tmp [16384];

    va_list va;
    va_start (va, fmat);
    rw_vsnprintf (tmp, sizeof(tmp)/sizeof(char)-1, fmat, va);
    va_end (va);

    return tmp;
}

// RECORD_ID defines a 'magic' number that happens to be the key used in 
// for the test record during testing.
// TABLE_NAME defines a 'magic' name that is the name of the table used
// for testing in.
#define RECORD_ID 1000
#define TABLE_NAME "rwdbtuxtesttable"

/******************************************************************************/
// RWDBXATest is an assertion counting and output management system.
//
// It serves the same purpose as the RWDBXATester and RWQETest classes used
// in the testxaclient.cpp file.  The major difference is this class is created
// as a part of the execution process, rather than inserted as a layer between
// the main() function and the execution process.  This difference is dictated
// by the tuxedo structure.
//

class RWDBXATest
{
private:
    //Static instance reference for RWDB error handler
    static RWDBXATest* inst;
    static void errorHandler(const RWDBStatus& status);

    //Member variables
    unsigned assert_, fail_;
    std::ostream *outs_, *errs_;
public:
    RWDBXATest (std::ostream *out = &std::cout, 
                std::ostream *err = &std::cerr);

#ifdef AUTOTEST
    ~RWDBXATest ();
#endif

    std::ostream& outstr() {return *outs_;}
    std::ostream& errstr() {return *errs_;}

    void print ( const char* message )
        {
#ifdef AUTOTEST
            (*outs_) << REPORTPREFIX << message << std::endl;
#else
            (*outs_) << message << std::endl;
#endif
        }

    void setFunctionTag( const char* functionTag ) 
        { 
#ifdef AUTOTEST
            (*outs_) << std::endl << REPORTPREFIX << functionTag << std::endl;
#else
            (*outs_) << functionTag << std::endl;
#endif
        }

    RWBoolean assertTrue ( const char* tag, int line, RWBoolean condition);
};

RWDBXATest* RWDBXATest::inst=0;

void RWDBXATest::errorHandler(const RWDBStatus& status)
{
    if (status.isValid() )
        return;
    (*inst->errs_) << std::endl << "Error code          : " 
                   << status.errorCode() << std::endl 
                   << "Error Message       : " << status.message()
                   << std::endl << "Vendor Message1     : " 
                   << status.vendorMessage1() << std::endl 
                   << "Vendor Message2     : " << status.vendorMessage2()
                   << std::endl << "Vendor Error1       : " 
                   << status.vendorError1() << std::endl 
                   << "Vendor Error2       : " << status.vendorError2()
                   << std::endl;
}

RWDBXATest::RWDBXATest (std::ostream *out, std::ostream *err) : 
    assert_(0), fail_(0), outs_(out), errs_(err) {
    inst=this;
    RWDBManager::setErrorHandler(errorHandler);
#ifdef AUTOTEST
    (*outs_) << REPORTPREFIX  "-------------------------------" << std::endl
             << REPORTPREFIX TEST_TAG "     = " << std::endl
             << REPORTPREFIX COMPILER_ID "  = " RW_TEST_COMPILER
#ifdef RW_TEST_LIBSTD
        " with " RW_TEST_LIBSTD
#endif   // RW_TEST_LIBSTD
             << std::endl
             << REPORTPREFIX MACHINE_ID "   = " RW_TEST_MACHINE << std::endl
             << REPORTPREFIX REMARK "      = " << std::endl << std::endl;
#endif
}

#ifdef AUTOTEST
RWDBXATest::~RWDBXATest () {
    (*outs_) << REPORTPREFIX WARNING_STRING " = 0" << std::endl
             << REPORTPREFIX NUMBER_TEST_CASES " = " << assert_ << std::endl
             << REPORTPREFIX CUMULATIVE_ERRORS " = " << fail_ << std::endl
             << std::endl << std::endl;
}
#endif

//assertTrue should use atomic increments, but test client isn't threaded, so this isn't strictly needed. (client is limiting factor in calls)
RWBoolean RWDBXATest::assertTrue ( const char* tag, int line, 
                                   RWBoolean condition)
{
    ++assert_;
    if(!condition){
#ifdef AUTOTEST
        (*errs_) << REPORTPREFIX "-       " ASSERTION_TAG " = " 
                 << tag << std::endl
                 << REPORTPREFIX "-       " FILE_NAME " = " __FILE__
                 << std::endl
                 << REPORTPREFIX "-       " LINE_NUMBER " = " 
                 << line << std::endl;
//         // write string to the debugger console in format understood 
//         // by MSVC
//         fprintf (0, "\n%s(%d) : assertion failure: %s\n\n",
//                  __FILE__, line, tag);
#else
        (*errs_) << tag << std::endl;
#endif
        ++fail_;
    }
    return condition;
}

class RWDBTuxLinkError {}; //something to throw during construction

class RWDBTuxApp
{
public:
    RWDBTuxApp(RWDBXATest *framework, int argc, char* const* argv);
    ~RWDBTuxApp() {}

    RWBoolean insertData( const int value, const RWCString& data );
    RWBoolean updateData( const int value, const RWCString& data );
    RWBoolean deleteData( const int value );
    RWBoolean checkData( const RWCString& data );
private:
    RWDBXATest *f_;
    RWDBDatabase xaDB_;
    RWDBConnection xaCon_;

    void parseOptions (int argc,char* const* argv, RWCString& serverType, 
                      RWCString& serverName, RWCString& userName,
                      RWCString& password, RWCString& databaseName, 
                      RWCString& pstring);
};

RWDBTuxApp::RWDBTuxApp(RWDBXATest *framework, int argc, char* const* argv) :
    f_(framework)
{
    RWCString serverType, serverName, userName, password, databaseName, pstring;
#ifdef AUTOTEST
    serverType = RWDB_SERVERTYPE;
    pstring = RWDB_PSTRING;
#endif
    parseOptions (argc, argv, serverType, serverName, userName, password, 
                  databaseName, pstring);

    f_->outstr() << "---- Database Parameters for XA connection ----" 
                 << std::endl << "Server Type   : " << serverType
                 << std::endl << "Server Name   : " << serverName
                 << std::endl << "User Name     : " << userName
                 << std::endl << "Password      : " << password
                 << std::endl << "Database Name : " << databaseName
                 << std::endl << "Connect String: " << pstring
                 << std::endl;

    if(!RWDB_ASSERT(!serverType.isNull() && !pstring.isNull(), 
                    "Server Initialization Arguements Not Passed.\n"
                    "Usage: -T <server type> -R <propertyString> [-S "
                    "<serverName>] [-U <userName>] [-P <password>] "
                    "[-B <databaseName>]\n"))
        throw RWDBTuxLinkError();

//     RWDBDatabase::connect(FALSE);
    xaDB_ = RWDBManager::database ( serverType, serverName, userName,
                                    password, databaseName, pstring );

    if(!RWDB_ASSERT(xaDB_.isValid(), "Error opening database for XA database "
                     "connection."))
        throw RWDBTuxLinkError();

    xaCon_ = xaDB_.connection();
//     xaDB_.defaultConnections(0);

    if(!RWDB_ASSERT(xaCon_.isValid(), "Error creating connection for XA "
                    "database connection."))
        throw RWDBTuxLinkError();

}

void RWDBTuxApp::parseOptions(int argc, char* const* argv, 
                              RWCString& serverType, RWCString& serverName, 
                              RWCString& userName, RWCString& password, 
                              RWCString& databaseName, RWCString& pstring)
{
    static size_t counter = 1;
    RWCString str;
    size_t i;

    //first, forward to the end of the string or the -- token (we don't care what comes before it)
    for (; counter < argc && strcmp(argv[counter], "--");++counter);
    if (counter > (argc - 1))
        return;//exit if we ran out of args, or found -- as the last item

    for (++counter; counter < argc; ++counter) {
        //Start by moving off the -- token, stop when we roll off the last argument
        str = argv[counter];
        if (str[0] != '-') {
            f_->errstr() << "Unexpected value encountered: " <<  argv[counter];
            continue;
        }
        if(++counter == argc){
            f_->errstr() << "Missing argument value for: " << str;
            break;
        }

        switch(str[1]){
        case 'T': serverType = RWCString( argv[counter] ); break;
        case 'S': serverName = RWCString( argv[counter] ); break;
        case 'U': userName = RWCString( argv[counter] ); break;
        case 'P': password = RWCString( argv[counter] ); break;
        case 'B': databaseName = RWCString( argv[counter] ); break;
        case 'R': pstring = RWCString( argv[counter] ); break;
        default:
            --counter; //Step backwards as we didn't read a valid option
            f_->errstr() << "Unexpected option encountered: " << str;
        }
    }
}

// This method inserts a key/value pair into the table.
RWBoolean RWDBTuxApp::insertData( const int value, const RWCString& data )
{
    f_->setFunctionTag("insertData");
    RWDB_ASSERT_RET(xaDB_.isValid(), "XA database invalid.");
//     RWDBConnection xaCon_ = xaDB_.connection();
    RWDB_ASSERT_RET(xaCon_.isValid(), "XA database connection invalid.");
    RWDBTable testTable = xaDB_.table( TABLE_NAME );
    RWDB_ASSERT_RET(testTable.exists(xaCon_),"Test table not found.");
    
    RWDBInserter xainsert = testTable.inserter();
    xainsert << value << data;
    return RWDB_ASSERT(xainsert.execute(xaCon_).isValid(),"Error inserting data "
                     "into test table.");
}

// This method updates a key/value pair in the table.
RWBoolean RWDBTuxApp::updateData( const int value, const RWCString& data )
{
    f_->setFunctionTag("updateData");
    RWDB_ASSERT_RET(xaDB_.isValid(), "XA database invalid.");
//     RWDBConnection xaCon_ = xaDB_.connection();
    RWDB_ASSERT_RET(xaCon_.isValid(), "XA database connection invalid.");
    RWDBTable testTable = xaDB_.table( TABLE_NAME );
    RWDB_ASSERT_RET(testTable.exists(xaCon_),"Test table not found.");
    
    RWDBUpdater xaupdate = testTable.updater();
    xaupdate << testTable["data"].assign(data);
    xaupdate.where(testTable["value"] == value);
    return RWDB_ASSERT(xaupdate.execute(xaCon_).isValid(),"Error updating data "
                     "in test table.");
}

// This method deletes a key/value pair from the table.
RWBoolean RWDBTuxApp::deleteData( const int value )
{
    f_->setFunctionTag("deleteData");
    RWDB_ASSERT_RET(xaDB_.isValid(), "XA database invalid.");
//     RWDBConnection xaCon_ = xaDB_.connection();
    RWDB_ASSERT_RET(xaCon_.isValid(), "XA database connection invalid.");
    RWDBTable testTable = xaDB_.table( TABLE_NAME );
    RWDB_ASSERT_RET(testTable.exists(xaCon_),"Test table not found.");
    
    RWDBDeleter xadelete = testTable.deleter();
    xadelete.where( testTable["value"] == value );
    return RWDB_ASSERT(xadelete.execute(xaCon_).isValid(),"Error deleting data "
                     "in test table.");
}

// This method checks that the value for the test record stored in the table
// is the same as the value passed in.  Must be called from inside an XA
// transaction
RWBoolean RWDBTuxApp::checkData( const RWCString& data)
{
    f_->setFunctionTag("checkData");
    RWDB_ASSERT_RET(xaDB_.isValid(), "XA database invalid.");
//     RWDBConnection xaCon_ = xaDB_.connection();
    RWDB_ASSERT_RET(xaCon_.isValid(), "XA database connection invalid.");
    RWDBTable testTable = xaDB_.table( TABLE_NAME );
    RWDB_ASSERT_RET(testTable.exists(xaCon_),"Test table not found.");
    
    RWDBReader xareader = testTable.reader( xaCon_ );
    int count = 0;
    while( xareader() )
    {
        int storedValue;
        RWCString storedData;

        count++;        
        if ( data.isNull() || count > 1 )
        {
            break;
        }
        
        xareader >> storedValue >> storedData;
        RWCString err = "Wrong data in test table: found ";
        err+=storedData;
        err+=", expected ";
        err+=data;
        RWDB_ASSERT_RET( RECORD_ID == storedValue && data == storedData, 
                       err.data());
    }
 
    return RWDB_ASSERT_F( ( data.isNull() && (! count) ) ||
               ( !data.isNull() && (count == 1) ),  ("Wrong number of rows in "
                                                     "test table: found %d, "
                                                     "expected %d", count, 
                                                     ( data.isNull() ? 0 : 1 )));
}

#ifdef AUTOTEST
#include <fstream>
std::ofstream target("testxaserver.out");
RWDBXATest testinst(&target, &target);
#else
RWDBXATest testinst();
#endif

RWDBTuxApp* application = 0;
//Needs to be a pointer, as argc/argv needed for construction

///////////////////////////////////////////////////////////////////////////
//
// tpsvrinit()
//
///////////////////////////////////////////////////////////////////////////
int tpsvrinit(int argc, char *argv[])
{
    if(! testinst.assertTrue("tpsvrinit: failed to open database", __LINE__, 
                             tpopen() != -1))
    {
        switch ( tperrno )
        {
        case TPESYSTEM:
            testinst.errstr() << "Tuxedo System error"
                              << std::endl;
            break;
        case TPEOS:
            testinst.errstr() << "Operating System error"
                              << std::endl;
            break;
        case TPEPROTO:
            testinst.errstr() << "Called in improper context"
                              << std::endl;
            break;
        case TPERMERR:
            testinst.errstr() << "Failed to open Resource Manager"
                              << std::endl;
            break;
        default:
            testinst.errstr() << "Unknown failure cause: " << tperrno
                              << std::endl;
            break;
        }
        //Following must be called at some point during cleanup
        //This is called in tpsvrdone(), if called.
        //delete application;
        return -2;     /* causes the server to exit */
    }
    try{
        application = new RWDBTuxApp(&testinst, argc, argv);
    }catch(RWDBTuxLinkError e){
        return -1;
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////////
//
// tpsvrdone()
//
///////////////////////////////////////////////////////////////////////////
void tpsvrdone()
{
    if(! testinst.assertTrue("tpsvrclose: failed to close database", __LINE__, 
                             tpclose() != -1))
    {
        switch ( tperrno )
        {
        case TPESYSTEM:
            testinst.errstr() << "Tuxedo System error"
                              << std::endl;
            break;
        case TPEOS:
            testinst.errstr() << "Operating System error"
                              << std::endl;
            break;
        case TPEPROTO:
            testinst.errstr() << "Called in improper context"
                              << std::endl;
            break;
        case TPERMERR:
            testinst.errstr() << "Failed to close Resource Manager"
                              << std::endl;
            break;
        default:
            testinst.errstr() << "Unknown failure cause: " << tperrno
                              << std::endl;
            break;
        }
    }
    delete application;
}

///////////////////////////////////////////////////////////////////////////
//
// service SERV_INS_DATA
//
///////////////////////////////////////////////////////////////////////////
extern "C"
void SERV_INS_DATA( TPSVCINFO *rqst )
{
    RWCString valueData = RWCString( rqst->data );
    int separator = valueData.first(':');
    RWCString data = RWCString( valueData( separator + 1,
                                           valueData.length() - separator - 1
                                         )
                              );
    int value = RWDBValue( RWCString( valueData( 0, separator ) ) ).asInt();

    if ( application->insertData( value, data ) ){
        tpreturn (TPSUCCESS, 0, NULL, 0L, 0);
    }else{
        tpreturn (TPFAIL, 0, NULL, 0L, 0);
    }
}

///////////////////////////////////////////////////////////////////////////
//
// service SERV_UPD_DATA
//
///////////////////////////////////////////////////////////////////////////
extern "C"
void SERV_UPD_DATA( TPSVCINFO *rqst )
{
    RWCString valueData = RWCString( rqst->data );
    int separator = valueData.first(':');
    
    RWCString data = RWCString( valueData( separator + 1,
                                           valueData.length() - separator - 1
                                         )
                              );
    int value = RWDBValue( RWCString( valueData( 0, separator ) ) ).asInt();

    if ( application->updateData( value, data ) ){
        tpreturn (TPSUCCESS, 0, NULL, 0L, 0);
    }else{
        tpreturn (TPFAIL, 0, NULL, 0L, 0);
    }
}

///////////////////////////////////////////////////////////////////////////
//
// service SERV_DEL_DATA
//
///////////////////////////////////////////////////////////////////////////
extern "C"
void SERV_DEL_DATA( TPSVCINFO *rqst )
{
    RWCString valueData = RWCString( rqst->data );
    int separator = valueData.first(':');
    
    int value = RWDBValue( RWCString( valueData( 0, separator ) ) ).asInt();

    if ( application->deleteData( value ) ){
        tpreturn (TPSUCCESS, 0, NULL, 0L, 0);
    }else{
        tpreturn (TPFAIL, 0, NULL, 0L, 0);
    }
}

///////////////////////////////////////////////////////////////////////////
//
// service SERV_CHK_DATA
//
///////////////////////////////////////////////////////////////////////////
extern "C"
void SERV_CHK_DATA( TPSVCINFO *rqst )
{
    RWCString valueData = RWCString( rqst->data );
    int separator = valueData.first(':');

    RWCString data = RWCString( valueData( separator + 1,
                                           valueData.length() - separator - 1
                                         )
                              );

    if ( application->checkData( data ) ){
        tpreturn (TPSUCCESS, 0, NULL, 0L, 0);
    }else{
        tpreturn (TPFAIL, 0, NULL, 0L, 0);
    }
}
