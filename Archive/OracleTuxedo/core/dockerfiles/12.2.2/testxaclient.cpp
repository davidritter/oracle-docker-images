/**************************************************************************
 *
 * $Id: //db/ed10-update1/tests/xa/tuxedo/testxaclient.cpp#1 $
 *
 * $$RW_INSERT_HEADER "dbyrs.str"
 *
 **************************************************************************
 *
 * Client program for Tuxedo application to test SourcePro DB XA
 * functionality.
 *
 * This program first creates the database environemnt by creating
 * required database tables. It then calls various services on the server
 * and also draws transaction boundries. At the end, it drops the tables
 * created.
 *
 **************************************************************************/

#include <atmi.h>

#include <string.h>

#include <rw/db/db.h>
#include <stdarg.h> //for va_start/va_end

// The AUTOTEST macro is used internally by RogueWave for testing purposes
// End users _should_ _not_ define this macro.
// If this is to be used as part of an internal test suite, a suitable
// scafolding must be provided in the form of an RWQETest class that implements
// all functionality expected by the code.
#ifndef AUTOTEST

#include "tututil.h"
#include "tutdefs.h"

//Stub RWQETest to provide test status information to the user.
class RWQETest
{
public:
    RWQETest () {}
    virtual ~RWQETest () {}
    virtual void runTest () = 0;
    int go ()
        {
            runTest();
            return 0;
        }
    void outputFile  ( const char* ) {}
    void print ( const char* message ) { std::cout << message << std::endl; }
    void setFunctionTag( const char* functionTag ) 
        { std::cout << functionTag << std::endl; }
    void setStepTag ( const char* ) {}
    RWBoolean assertTrue ( const char* assertionTag, const char*, int,
                        RWBoolean condition, const char* idString=0)
        {
            if(!condition)
                std::cout << assertionTag << std::endl;
            return condition;
        }

    RWBoolean warnTrue ( const char* warningTag, const char*, int,
                      RWBoolean condition, const char* idString=0)
        { 
            if(!condition)
                std::cout << warningTag << std::endl;
            return condition;
        }
};

//Glue for AUTOTEST mode switching (only try next step if current step passes)
#define XACHAIN &&

#else // ifndef AUTOTEST
#include <rw/rwtest/rwtest.h>

//Try next step if current step fails
#define XACHAIN ;

#endif // ifndef AUTOTEST

/******************************************************************************/
// Macromagic to handle a windows quirk
// (windows runtime library prefixes *nprintf calls with _)

#ifdef rw_snprintf
#undef rw_snprintf
#endif

#ifdef rw_vsnprintf
#undef rw_vsnprintf
#endif

#ifdef _WIN32
#include <stdio.h> // for *printf()
#define rw_snprintf _snprintf
#define rw_vsnprintf _vsnprintf
#else
#define rw_snprintf snprintf
#define rw_vsnprintf vsnprintf
#endif

/******************************************************************************/
// Utility macro definitions
// These macros are used to check if a condition is true, and report the 
// failure if the condition isn't.
// Unless specified, the macros assume the name of the RWDBXATester object
// used in the assertion checking is f_.
// The macros follow the following naming convention:
//     RWDB_(TUX_)(ASSERT|WARN)(|_RET|_F)
// The TUX_ token designates that the macro tests for equality with -1
//     rather than if the expression is true
// The ASSERT and WARN macros are functionally equivilent, though calls to the
//     WARN variant are less of a concern, as they may trigger when things have
//     already gone wrong, and are usually symptoms of a larger problem.
// The 'plain' version (no sufix) handles output of an error message if the 
//     check fails, then returns true if the assertion matched the expected
//     value (true or Tuxedo) or false if the assetion failed
// The _RET variant returns from the current function with a value of false if
//     the assertion failed
// The _F variant takes a printf/variable argument style format block rather 
//     than a string

#define RWDB_ASSERT(expr, msg) \
    f_->assertTrue(msg, __FILE__, __LINE__, (expr))

#define RWDB_ASSERT_RET(expr, msg) \
    if(!RWDB_ASSERT(expr, msg)) return false

#define RWDB_ASSERT_F(expr, VA_LIST) \
    RWDB_ASSERT(expr, format VA_LIST)

#define RWDB_WARN_RET(expr, msg) \
    if(!f_->warnTrue(msg, __FILE__, __LINE__, (expr))) return false

#define RWDB_TUX_ASSERT(status, method, msg) \
    f_->assertTuxedo(msg, __FILE__, __LINE__, status, method)

#define RWDB_TUX_WARN(status, method, msg) \
    f_->warnTuxedo(msg, __FILE__, __LINE__, status, method)

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
// for the test record during testing.  It is defined as a string, as
// it is passed to tuxedo in string form, rather than numeric form
// TABLE_NAME defines a 'magic' name that is the name of the table used
// for testing in.
#define RECORD_ID "1000"
#define TABLE_NAME "rwdbtuxtesttable"

///////////////////////////////////////////////////////////////////////////
//
// struct xaer
//
// determines Tuxedo errors.
//
///////////////////////////////////////////////////////////////////////////
struct tuxedoError
{
    int errorCode;
    const char* errorName;
    const char* errorMessage;
} tuxedoErrorTable[] =
    {
        TPEINVAL, "TPEINVAL", //0
        "invalid arguement(s) passed",
        TPEABORT, "TPEABORT", //1
        "service returned failure",
        TPEBLOCK, "TPEBLOCK", //3
        "blocking condition found",
        TPENOENT, "TPENOENT", //6
        "call failed due to space limitations or service name not found",
        TPEOS, "TPEOS", //7
        "operating System error",
        TPEPERM, "TPEPERM", //8
        "authentication failure",
        TPEPROTO, "TPEPROTO", //9
        "called in improper context",
        TPESVCERR, "TPESVCERR", //10
        "service routine encountered an error",
        TPESVCFAIL, "TPESVCFAIL", //11
        "service returned \"FAILED\"",
        TPESYSTEM, "TPESYSTEM", //12
        "tuxedo System error",
        TPETIME, "TPETIME", //13
        "timeout occurred",
        TPETRAN, "TPETRAN", //14
        "service does not support transaction or error starting transaction",
        TPGOTSIG, "TPGOTSIG", //15
        "received signal",
        TPEITYPE, "TPEITYPE", //17
        "send buffer of invalid type",
        TPEOTYPE, "TPEOTYPE", //18
        "receive buffer of invalid type",
        TPEHAZARD, "TPEHAZARD", //20
        "transaction failure",
        TPEHEURISTIC, "TPEHEURISTIC", //21
        "transaction partially committed"
    };

#define tuxedoErrorTableSize (sizeof(tuxedoErrorTable)/sizeof(struct tuxedoError))

// struct RWDBXAInitErr is something to throw if problems happen during
// construction
struct RWDBXAInitErr {};

/******************************************************************************/
// RWDBXATester is the assertion counting framework and test harness.
//
// In addition to providing methods (in the RWQETest class) that run the actual
// test application and count assertions, this method is responsible for parsing
// command line arguments and storing the connection parameter strings.

class RWDBXATester: public RWQETest
{ 
public:
    //Test Interface
    virtual void runTest ();

    //Error checking method
    RWBoolean assertTuxedo ( const char* assertionTag, const char* file, 
                         int line, int tuxedoStatus, const char* method, 
                         const char* idString=0 );
    RWBoolean warnTuxedo ( const char* assertionTag, const char* file, int line, 
                       int tuxedoStatus, const char* method,
                       const char* idString=0 );

    //Housekeeping method
    RWBoolean processArgs (int argc, char* argv[]);

    //Internal variables
    RWCString serverType, serverName, userName, password, databaseName, pstring;
};

// assertTuxedo utilizes the assertTrue method and provides a form output
// describing what method failed, what the error name is and a description
// of the error.  The later are looked up using the tuxedoErrorTable array.
RWBoolean RWDBXATester::assertTuxedo ( const char* assertionTag, 
                             const char* file, int line, int tuxedoStatus, 
                             const char* method, const char* )
{
    if ( -1 != tuxedoStatus ){
        return assertTrue(0,0,0,!0);
    }else{
        char buf[512];//Should be long enough...
        RWBoolean r;
        r=assertTrue(assertionTag, file, line, false);
        for ( unsigned int n = 0; n < tuxedoErrorTableSize; n++ )
        {
            if ( tuxedoErrorTable[n].errorCode == tperrno )
            {
                rw_snprintf(buf,sizeof(buf)/sizeof(char)-1,
                            "%s: %s - %s\n",
                            method, tuxedoErrorTable[n].errorName,
                            tuxedoErrorTable[n].errorMessage);
                print(buf);
                return r;
            }
        }
    
        rw_snprintf(buf,sizeof(buf)/sizeof(char)-1,
                    "UNDOCUMENTED ERROR: %s returned %d", method, tperrno);
        print(buf);
        return r;
    }
}

// warnTuxedo is functionaly equivilent to assertTuxedo, but uses warnTrue rather
// than assertTrue.
RWBoolean RWDBXATester::warnTuxedo ( const char* assertionTag,  const char* file, 
                           int line, int tuxedoStatus, const char* method, 
                           const char* )
{
    if ( -1 != tuxedoStatus ){
        return warnTrue(0,0,0,!0);
    }else{
        char buf[512];//Should be long enough...
        RWBoolean r;
        r = warnTrue(assertionTag, file, line, false);
        for ( unsigned int n = 0; n < tuxedoErrorTableSize; n++ )
        {
            if ( tuxedoErrorTable[n].errorCode == tperrno )
            {
                rw_snprintf(buf,sizeof(buf)/sizeof(char)-1,
                         "%s: %s - %s\n",
                         method, tuxedoErrorTable[n].errorName,
                         tuxedoErrorTable[n].errorMessage);
                print(buf);
                return r;
            }
        }
    
        rw_snprintf(buf,sizeof(buf)/sizeof(char)-1,
                "UNDOCUMENTED ERROR: %s returned %d", method, tperrno);
        print(buf);
        return r;
    }
}

// In non-AUTOTEST mode, processArgs() is basically a wrapper around the 
// initalizeDatabaseArguments method in tututil.cpp.  
// In AUTOTEST mode, processArgs() invokes the default argument parser in
// RWQETest, then does some processing of its own using the infrastructure
// provided in RWQETest.
//  -A means append output to output file
//  -C flags compiler name
//  -D means send output to directory
//  -G specifies the name of debug file (e.g., /dev/stderr)
//  -L specifies an alarm period in seconds
//  -M flags machine name
//  -O means send output to file
//
// The flags used to override databse defaults are
//  -B database name
//  -P password (insecure on multi-user systems)
//  -R non-XA property string
//  -S server name
//  -T server type
//  -U user name

RWBoolean RWDBXATester::processArgs(int argc, char* argv[])
{
#ifndef AUTOTEST
    initializeDatabaseArguments ( argc, argv, serverType, serverName, 
                                  userName, password, databaseName, pstring);
#else //ndef AUTOTEST
    RWQETest::processDefaultArguments(argc, argv);

    char arg[MAX_DEFAULT_ARG_LEN];

    *arg = '\0';

    serverType = RWDB_SERVERTYPE;
    serverName = RWDB_SERVERNAME;
    userName = RWDB_USER;
    password = RWDB_PASSWORD;
    databaseName = RWDB_DATABASE;
    pstring = RWDB_PSTRING;

    if (!getOptions.cmdLine() || !strlen(getOptions.cmdLine()))
        return TRUE;

    if (getOptions.getArgumentString('T',arg,MAX_DEFAULT_ARG_LEN-1))
        serverType=arg;

    if (getOptions.getArgumentString('S',arg,MAX_DEFAULT_ARG_LEN-1))
        serverName=arg;

    if (getOptions.getArgumentString('U',arg,MAX_DEFAULT_ARG_LEN-1))
        userName=arg;

    if (getOptions.getArgumentString('P',arg,MAX_DEFAULT_ARG_LEN-1))
        password=arg;

    if (getOptions.getArgumentString('B',arg,MAX_DEFAULT_ARG_LEN-1))
        databaseName=arg;

    if (getOptions.getArgumentString('R',arg,MAX_DEFAULT_ARG_LEN-1))
        pstring=arg;
#endif //ndef AUTOTEST
    return TRUE;
}

/******************************************************************************/
// RWDBXATestEnv represents the state of the database for the application
// 
// In a real application, the tables would be established during install, and
// likely would never be dropped, except as part of an upgrade.  As this 
// is an example, the tables used are transient in nature, and should be
// cleaned up as part of the exit process.
//
// Also set up here is the application wide error handler.  This is a private 
// member method.  In a production application, this would be set up as part
// of your application initilization process.

class RWDBXATestEnv
{
public:
    RWDBXATestEnv(RWDBXATester *framework);
    ~RWDBXATestEnv();
private:
    //Internal variables
    static RWDBXATester *f_; //Must be static to use with handler
    static void errorHandler(const RWDBStatus& status);

    RWBoolean createTable ();
    RWBoolean dropTable ();
};

RWDBXATester* RWDBXATestEnv::f_ = 0;

RWDBXATestEnv::RWDBXATestEnv (RWDBXATester *framework)
{
    f_ = framework;
    RWDBManager::setErrorHandler( errorHandler );
    if( !createTable() )
        throw RWDBXAInitErr();
}

RWDBXATestEnv::~RWDBXATestEnv ()
{
    dropTable();
}

// This method sets up our test table through a non-XA connection, created
// for that purpose.  Methods similar to this likely aren't needed in 
// production applications.
RWBoolean RWDBXATestEnv::createTable()
{
    f_->setFunctionTag("Creating test table...");
    RWDBDatabase aDB = RWDBManager::database
                       ( f_->serverType, f_->serverName, f_->userName, 
                         f_->password, f_->databaseName, f_->pstring );
    RWDB_ASSERT_RET(aDB.isValid(), "Error opening database for non-XA database "
                  "connection.");
    RWDBConnection aCon = aDB.connection();
    RWDB_ASSERT_RET(aCon.isValid(), "Error creating connection for non-XA "
                  "database connection.");

    RWDBTable testTable = aDB.table( TABLE_NAME );
    if ( testTable.exists(aCon) )
    {
        RWDB_ASSERT_RET(
            testTable.drop(aCon).isValid() && !testTable.exists(aCon, TRUE),
            "Table `" TABLE_NAME "` already exists.  Dropping existing test "
            "table failed.");
    }

    RWDBSchema aSchema = RWDBSchema();
    aSchema.appendColumn( "value", RWDBValue::Int );
    aSchema.appendColumn( "data", RWDBValue::String, 10 );
    RWDBStatus aStatus = aDB.createTable( TABLE_NAME, aSchema );
    
    RWDB_ASSERT_RET(
        aStatus.isValid() && testTable.exists( aCon, TRUE ),
        "Unable to create table `" TABLE_NAME "`.");
    f_->print("...Done");
    return true;
}

// This method cleans up the table we created in createTable, again using a
// non-XA connection.  As with the createTable method, a method similar to this
// likely isn't needed in a production application.
RWBoolean RWDBXATestEnv::dropTable()
{
    f_->setFunctionTag("Dropping test table...");
    RWDBDatabase aDB = RWDBManager::database
                       ( f_->serverType, f_->serverName, f_->userName, 
                         f_->password, f_->databaseName, f_->pstring );
    RWDB_ASSERT_RET(aDB.isValid(), "Error opening database for non-XA database "
                  "connection.");
    RWDBConnection aCon = aDB.connection();
    RWDB_ASSERT_RET(aCon.isValid(), "Error creating connection for non-XA "
                  "database connection.");

    RWDBTable testTable = aDB.table( TABLE_NAME );
    RWDB_WARN_RET ( testTable.exists(aCon), "Test table not found." );
    RWDB_WARN_RET (
            testTable.drop(aCon).isValid() && !testTable.exists(aCon, TRUE),
            "Unable to drop test table.");
    f_->print("...Done");
    return true;
}

// This is a simple error handling callback that echoes the problem to the 
// log file (if it is a problem).  A production callback would likely include 
// more sophisticated logic to determine what went wrong, and possibly a 
// recovery or retry mechanism.
void RWDBXATestEnv::errorHandler( const RWDBStatus& status )
{
    if(! RWDB_ASSERT( status.isValid() , "RWDB Error handler") ){
        f_->print(format("Error code          : %d", status.errorCode()));
        f_->print(format("Error Message       : %s", status.message().data()));
        f_->print(format("Vendor Message1     : %s", 
                         status.vendorMessage1().data()));
        f_->print(format("Vendor Message2     : %s", 
                         status.vendorMessage2().data()));
        f_->print(format("Vendor Error1       : %l", status.vendorError1()));
        f_->print(format("Vendor Error2       : %l", status.vendorError2()));
    }
}

/******************************************************************************/
// RWDBXATestApp is the 'real' application
//
// In a production program, this would be a class used by your application
// to interact with the BEA Tuxedo transaction manager.
//
// Along with the normal constructor and destructor, the public methods allow 
// a user of the class to insert into, query, update and delete from the
// database(s) the object is connected to.
// 
// The private methods include lower level methods used to interact with the 
// database connection and  table creation and deletion methods.  The table
// creation and deletion methods  would likely be located in the application 
// installer/uninstaller, rather than in the application itself.

class RWDBXATestApp
{
public:
    RWDBXATestApp(RWDBXATester *framework);
    ~RWDBXATestApp();

    //Run methods
    RWBoolean testInsertData (RWBoolean commit);
    RWBoolean testUpdateData (RWBoolean commit);
    RWBoolean testDeleteData (RWBoolean commit);
    RWBoolean testCheckData ();

private:
    //Internal variables
    RWDBXATester *f_;

    bool init_;
    char *sendbuf, *rcvbuf;
    
    //test guts
    inline RWBoolean beginXATransaction();
    inline RWBoolean commitXATransaction();
    inline RWBoolean abortXATransaction();
    inline RWBoolean callService(const char* serv);

    inline RWBoolean insertData();
    inline RWBoolean updateData();
    inline RWBoolean deleteData();
    inline RWBoolean checkData();
};

// This is the basic (default) constructor
// This constructor establishes the connection.
//
// In a production application, you likely would pass an RWDBDatabase 
// object, possibly with an RWDBConnection object to the constructor, rather
// than the connection parameters (stored in the RWDBXATester object), along
// with the transaction manager object.
RWDBXATestApp::RWDBXATestApp (RWDBXATester *framework) : 
    f_(framework), sendbuf(0), rcvbuf(0)
{
    f_->setFunctionTag("Opening Tuxedo Connection...");

    if(! RWDB_TUX_ASSERT(tpinit((TPINIT *) NULL), "tpinit", 
                        "Error opening tuxedo connection."))
        throw RWDBXAInitErr();

    init_=1;

    if((sendbuf = (char *) tpalloc((char *)"STRING", NULL, 20)) == NULL)
    {
        RWDB_TUX_ASSERT(-1, "tpalloc", "Error allocating send buffer");
        throw RWDBXAInitErr();
    }

    if((rcvbuf = (char *) tpalloc((char *)"STRING", NULL, 1)) == NULL)
    {
        RWDB_TUX_ASSERT(-1, "tpalloc", "Error allocating recieve buffer");
        throw RWDBXAInitErr();
    }
    f_->print("...Done");
}

RWDBXATestApp::~RWDBXATestApp ()
{
    if(init_){
        f_->setFunctionTag("Closing Tuxedo Connection...");
        if(rcvbuf)
            tpfree( rcvbuf );
        if(sendbuf)
            tpfree( sendbuf );
        RWDB_TUX_WARN(tpterm(), "tpterm", 
                          "Error closing tuxedo connection.");
        f_->print("...Done");
    }
}

inline RWBoolean RWDBXATestApp::beginXATransaction()
{
    f_->setStepTag("beginXATransaction");
    return RWDB_TUX_ASSERT(tpbegin( 60, 0 ), "tpbegin", 
                          "Error starting Tuxedo transaction.");
}

inline RWBoolean RWDBXATestApp::commitXATransaction()
{
    f_->setStepTag("commitXATransaction");
    return RWDB_TUX_ASSERT(tpcommit( 0 ), "tpcommit", "Error commiting "
                           "Tuxedo transaction.");

}

inline RWBoolean RWDBXATestApp::abortXATransaction()
{
    f_->setStepTag("abortXATransaction");
    return RWDB_TUX_ASSERT(tpabort( 0 ), "tpabort", "Error aborting "
                           "Tuxedo transaction.");
}

inline RWBoolean RWDBXATestApp::callService(const char* serv)
{
    long rcvlen;
    return RWDB_TUX_ASSERT(tpcall( (char *) serv, sendbuf, strlen(sendbuf), 
                                   &rcvbuf, &rcvlen, 0L ), serv,
                           format("Error calling Tuxedo %s service.", serv));
    //serv is more descriptive than "tpcall" when reporting the assert
}

// This method inserts a key/value pair into the table.
inline RWBoolean RWDBXATestApp::insertData()
{
    f_->setStepTag("insertData");
    return callService("SERV_INS_DATA");
}

// This method updates a key/value pair in the table.
inline RWBoolean RWDBXATestApp::updateData()
{
    f_->setStepTag("updateData");
    return callService("SERV_UPD_DATA");
}

// This method deletes a key/value pair from the table.
inline RWBoolean RWDBXATestApp::deleteData()
{
    f_->setStepTag("deleteData");
    return callService("SERV_DEL_DATA");
}

// This method checks that the value for the test record stored in the table
// is the same as the value passed in.  Must be called from inside an XA
// transaction
inline RWBoolean RWDBXATestApp::checkData()
{
    f_->setStepTag("checkData");
    return callService("SERV_CHK_DATA");
}

// This method checks that the value of the test record matches that passed in
// This method must be called from outside an XA transaction. (and is therefore 
// public)
RWBoolean RWDBXATestApp::testCheckData ()
{
    memcpy( sendbuf, "0:", 3 );
    RWBoolean ret = checkData();
    f_->print("...Done");
    return ret;
}

// This method tests the insertion of a value into the table, with the decision
// to commit or roll back dependant on the paramater.
RWBoolean RWDBXATestApp::testInsertData (RWBoolean commit)
{
    if(commit){
        f_->setFunctionTag("Testing data insertion with commit...");
        memcpy( sendbuf, RECORD_ID ":AAAA", 10 );
    }else{
        f_->setFunctionTag("Testing data insertion with abort...");
        memcpy( sendbuf, "2000:BBBB", 10 );
    }
    if(!beginXATransaction())
        return false;
    if(insertData() && commit){
        if(!commitXATransaction()){
            return false;
        }
    }else{
        if(!abortXATransaction() || commit ){
            return false;
        }
    }

    memcpy( sendbuf, "0:AAAA", 7 );
    return checkData();
}

// This method tests updating a value in the table for a given key, with the
// decision to commit or roll back dependant on the paramater.
RWBoolean RWDBXATestApp::testUpdateData (RWBoolean commit)
{
    if(commit){
        f_->setFunctionTag("Testing data update with commit...");
        memcpy( sendbuf, RECORD_ID ":ABCD", 10 );
    }else{
        f_->setFunctionTag("Testing data update with abort...");
        memcpy( sendbuf, RECORD_ID ":EFGH", 10 );
    }
    if(!beginXATransaction())
        return false; 
    if(updateData() && commit){
        if(!commitXATransaction()){
            return false;
        }
    }else{
        if(!abortXATransaction() || commit ){
            return false;
        }
    }
    memcpy( sendbuf, "0:ABCD", 7 );
    return checkData();
}

// This method deletes the record for a  given key, with the decision to 
// commit or roll back dependant on the paramater.
RWBoolean RWDBXATestApp::testDeleteData (RWBoolean commit)
{
    if(commit){
        f_->setFunctionTag("Testing data deletion with commit...");
    }else{
        f_->setFunctionTag("Testing data deletion with abort...");
    }
    if(!beginXATransaction())
        return false;
 
    memcpy( sendbuf, RECORD_ID ":", 6 );

    if(deleteData() && commit){
        if(!commitXATransaction()){
            return false;
        }
    }else{
        if(!abortXATransaction() || commit ){
            return false;
        }
    }

    if(commit){
        memcpy( sendbuf, "0:", 3 );
    }else{
        memcpy( sendbuf, "0:ABCD", 7 );
    }
    return checkData();
}

/******************************************************************************/
// runTest and Main
// runTest method is roughly equivilent to your main method within a regular
// application in that it creates the RWDBXATestEnv, RWDBXATestTM and 
// RWDBXATestApp objects, then operates on them.  Argument processing and 
// configuration loading would also occur here.
void RWDBXATester::runTest()
{
    print("---- Database Parameters for connection ----\n");
    print(format("Server Type            : %s", serverType.data()));
    print(format("Server Name            : %s", serverName.data()));
    print(format("User Name              : %s", userName.data()));
    print(format("Password               : %s", password.data()));
    print(format("Database Name          : %s", databaseName.data()));
    print(format("non-XA Property String : %s", pstring.data()));

    try {
        RWDBXATestEnv env(this);
        RWDBXATestApp app(this);
        setFunctionTag("Testing data query on empty table...");
        app.testCheckData() XACHAIN
        app.testInsertData(TRUE) XACHAIN
        app.testInsertData(FALSE) XACHAIN
        app.testUpdateData(TRUE) XACHAIN
        app.testUpdateData(FALSE) XACHAIN
        app.testDeleteData(FALSE) XACHAIN
        app.testDeleteData(TRUE);
    }catch(RWDBXAInitErr e) {}
}

// This is the AUTOTEST independant main method used to bootstrap the process
// Normal applications wouldn't need to go through the exercise of creating an
// object, passing it the arguments, then telling it to go, but this is needed
// here for the AUTOTEST enabled builds as other things happen in the go method
// there.
int main(int argc, char* argv[])
{
    RWDBXATester t;
    
    t.outputFile("testxaclient.out");
    t.processArgs(argc, argv);
    t.go ();

    return 0;
}
