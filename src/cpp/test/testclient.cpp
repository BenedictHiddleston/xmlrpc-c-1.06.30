/*=============================================================================
                                  testclient
===============================================================================
  Test the client C++ facilities of XML-RPC for C/C++.
  
  Contrary to what you might expect, we use the server facilities too
  because we test of the client using a simulated server, via the
  "direct" client XML transport we define herein.
=============================================================================*/
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <memory>
#include <time.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "transport_config.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/client.hpp"
#include "xmlrpc-c/client_simple.hpp"

#include "tools.hpp"
#include "testclient.hpp"

using namespace xmlrpc_c;
using namespace std;



class sampleAddMethod : public method {
public:
    sampleAddMethod() {
        this->_signature = "ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};



class carriageParm_direct : public carriageParm {
public:
    carriageParm_direct(registry * const registryP) : registryP(registryP) {}

    registry * registryP;
};


class clientXmlTransport_direct : public clientXmlTransport {

public:    
    void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         string                   const& callXml,
         string *                 const  responseXmlP) {

        carriageParm_direct * const parmP =
            dynamic_cast<carriageParm_direct *>(carriageParmP);

        if (parmP == NULL)
            throw(error("Carriage parameter passed to the direct "
                        "transport is not type carriageParm_direct"));

        parmP->registryP->processCall(callXml, responseXmlP);
    }
};



class clientDirectAsyncTestSuite : public testSuite {
/*----------------------------------------------------------------------------
   See clientDirectTestSuite for a description of how we use a
   clientXmlTransport_direct object to test client functions.

   The object of this class tests the async client functions.  With
   clientXmlTransport_direct, these are pretty simple because the
   transport doesn't even implement an asynchronous interface; it
   relies on the base class' emulation of start() using call().

   Some day, we should add true asynchronous capability to
   clientXmlTransport_direct and really test things.
-----------------------------------------------------------------------------*/
public:
    virtual string suiteName() {
        return "clientDirectAsyncTestSuite";
    }
    virtual void runtests(unsigned int const) {
        
        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
        
        carriageParm_direct carriageParmDirect(&myRegistry);
        clientXmlTransport_direct transportDirect;
        client_xml clientDirect(&transportDirect);
        paramList paramListSampleAdd1;
        paramListSampleAdd1.add(value_int(5));
        paramListSampleAdd1.add(value_int(7));
        paramList paramListSampleAdd2;
        paramListSampleAdd2.add(value_int(30));
        paramListSampleAdd2.add(value_int(-10));

        rpcPtr const rpcSampleAdd1P("sample.add", paramListSampleAdd1);
        rpcSampleAdd1P->start(&clientDirect, &carriageParmDirect);
        rpcPtr const rpcSampleAdd2P("sample.add", paramListSampleAdd2);
        rpcSampleAdd2P->start(&clientDirect, &carriageParmDirect);

        TEST(rpcSampleAdd1P->isFinished());
        TEST(rpcSampleAdd1P->isSuccessful());
        value_int const result1(rpcSampleAdd1P->getResult());
        TEST(static_cast<int>(result1) == 12);
        
        TEST(rpcSampleAdd2P->isFinished());
        TEST(rpcSampleAdd1P->isSuccessful());
        value_int const result2(rpcSampleAdd2P->getResult());
        TEST(static_cast<int>(result2) == 20);

        EXPECT_ERROR(clientDirect.finishAsync(timeout()););
        EXPECT_ERROR(clientDirect.finishAsync(timeout(50)););
    }
};



class clientDirectTestSuite : public testSuite {
/*----------------------------------------------------------------------------
  The object of this class tests the client facilities by using a
  special client XML transport defined above and an XML-RPC server we
  build ourselves and run inline.  We build the server out of a
  xmlrpc_c::registry object and our transport just delivers XML
  directly to the registry object and gets the response XML from it
  and delivers that back.  There's no network or socket or pipeline or
  anything -- the transport actually executes the XML-RPC method.
-----------------------------------------------------------------------------*/
public:
    virtual string suiteName() {
        return "clientDirectTestSuite";
    }
    virtual void runtests(unsigned int const indentation) {
        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
        
        carriageParm_direct carriageParmDirect(&myRegistry);
        clientXmlTransport_direct transportDirect;
        client_xml clientDirect(&transportDirect);
        paramList paramListSampleAdd;
        paramListSampleAdd.add(value_int(5));
        paramListSampleAdd.add(value_int(7));
        paramList paramListEmpty;
        {
            /* Test a successful RPC */
            rpcPtr rpcSampleAddP("sample.add", paramListSampleAdd);
            rpcSampleAddP->call(&clientDirect, &carriageParmDirect);
            TEST(rpcSampleAddP->isFinished());
            TEST(rpcSampleAddP->isSuccessful());
            value_int const resultDirect(rpcSampleAddP->getResult());
            TEST(static_cast<int>(resultDirect) == 12);
        }
        {
            /* Test a failed RPC */
            rpcPtr const rpcSampleAddP("sample.add", paramListEmpty);
            rpcSampleAddP->call(&clientDirect, &carriageParmDirect);
            TEST(rpcSampleAddP->isFinished());
            TEST(!rpcSampleAddP->isSuccessful());
            fault const fault0(rpcSampleAddP->getFault());
            TEST(fault0.getCode() == fault::CODE_TYPE);
        }

        {
            /* Test with an auto object transport */
            client_xml clientDirect(
                clientXmlTransportPtr(new clientXmlTransport_direct));
            rpcPtr rpcSampleAddP("sample.add", paramListSampleAdd);
            rpcSampleAddP->call(&clientDirect, &carriageParmDirect);
            TEST(rpcSampleAddP->isFinished());
            TEST(rpcSampleAddP->isSuccessful());
            EXPECT_ERROR(fault fault0(rpcSampleAddP->getFault()););
            value_int const resultDirect(rpcSampleAddP->getResult());
            TEST(static_cast<int>(resultDirect) == 12);
        }
        {
            /* Test with implicit RPC -- success */
            rpcOutcome outcome;
            clientDirect.call(&carriageParmDirect, "sample.add",
                              paramListSampleAdd, &outcome);
            TEST(outcome.succeeded());
            value_int const result(outcome.getResult());
            TEST(static_cast<int>(result) == 12);
        }
        {
            /* Test with implicit RPC - failure */
            rpcOutcome outcome;
            clientDirect.call(&carriageParmDirect, "nosuchmethod",
                              paramList(), &outcome);
            TEST(!outcome.succeeded());
            TEST(outcome.getFault().getCode() == fault::CODE_NO_SUCH_METHOD);
            TEST(outcome.getFault().getDescription().size() > 0);
        }

        clientDirectAsyncTestSuite().run(indentation+1);
    }
};



class curlTransportTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "curlTransportTestSuite";
    }
    virtual void runtests(unsigned int const) {
#if MUST_BUILD_CURL_CLIENT
        clientXmlTransport_curl transport0;
        clientXmlTransport_curl transport1("eth0");
        clientXmlTransport_curl transport2("eth0", true);
        clientXmlTransport_curl transport3("eth0", true, true);
        clientXmlTransport_curl transport4(
            clientXmlTransport_curl::constrOpt()
            .network_interface("eth0")
            .no_ssl_verifypeer(true)
            .no_ssl_verifyhost(true)
            .user_agent("my user agent")
            .ssl_cert("/etc/sslcert")
            .sslcerttype("PEM")
            .sslcertpasswd("mypass")
            .sslkey("/etc/sslkey")
            .sslkeytype("DER")
            .sslkeypasswd("mykeypass")
            .sslengine("mysslengine")
            .sslengine_default(true)
            .sslversion(XMLRPC_SSLVERSION_SSLv2)
            .cainfo("/etc/cainfo")
            .capath("/etc/cadir")
            .randomfile("/dev/random")
            .egdsocket("/tmp/egdsocket")
            .ssl_cipher_list("RC4-SHA:DEFAULT")
            );            

        clientXmlTransport_curl transport5(
            clientXmlTransport_curl::constrOpt()
            .no_ssl_verifypeer(false));

        clientXmlTransport_curl transport6(
            clientXmlTransport_curl::constrOpt());
        
        clientXmlTransportPtr transport1P(new clientXmlTransport_curl);
        clientXmlTransportPtr transport2P;
        transport2P = transport1P;
#else
        EXPECT_ERROR(clientXmlTransport_curl transport0;);
        EXPECT_ERROR(clientXmlTransport_curl transport1("eth0"););
        EXPECT_ERROR(clientXmlTransport_curl transport0("eth0", true););
        EXPECT_ERROR(clientXmlTransport_curl transport0("eth0", true, true););
#endif
    }
};



class libwwwTransportTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "libwwwTransportTestSuite";
    }
    virtual void runtests(unsigned int const) {
#if MUST_BUILD_LIBWWW_CLIENT
        clientXmlTransport_libwww transport0;
        clientXmlTransport_libwww transport1("getbent");
        clientXmlTransport_libwww transport2("getbent", "1.0");
        clientXmlTransportPtr transport1P(new clientXmlTransport_libwww);
        clientXmlTransportPtr transport2P;
        transport2P = transport1P;
#else
        EXPECT_ERROR(clientXmlTransport_libwww transport0;);
        EXPECT_ERROR(clientXmlTransport_libwww transport1("getbent"););
        EXPECT_ERROR(clientXmlTransport_libwww transport2("getbent", "1.0"););
#endif
    }
};



class wininetTransportTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "wininetTransportTestSuite";
    }
    virtual void runtests(unsigned int const) {
#if MUST_BUILD_WININET_CLIENT
        clientXmlTransport_wininet transport0;
        clientXmlTransport_wininet transport1(true);
        clientXmlTransportPtr transport1P(new clientXmlTransport_wininet);
        clientXmlTransportPtr transport2P;
        transport2P = transport1P;
#else
        EXPECT_ERROR(clientXmlTransport_wininet transport0;);
        EXPECT_ERROR(clientXmlTransport_wininet transport1(true););
#endif
    }
};



class ambivalentTransportTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "ambivalentTransportTestSuite";
    }
    virtual void runtests(unsigned int const) {
        vector<string> const typeList(
            clientXmlTransport_http::availableTypes());

        TEST(typeList.size() > 0);

        clientXmlTransportPtr const transportP(
            clientXmlTransport_http::create());
        carriageParm_http0 carriageParm0("http://whatsamatta.edux");
        client_xml client0(transportP);
        
        rpcOutcome outcome;

        // Fails because there's no such server
        EXPECT_ERROR(
            client0.call(&carriageParm0, "nosuchmethod", paramList(),
                         &outcome);
            );
    }
};



class clientXmlTransportTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "clientXmlTransportTestSuite";
    }
    virtual void runtests(unsigned int const indentation) {
        curlTransportTestSuite().run(indentation + 1);
        libwwwTransportTestSuite().run(indentation + 1);
        wininetTransportTestSuite().run(indentation + 1);
        ambivalentTransportTestSuite().run(indentation + 1);
    }
};



class clientSimpleTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "clientSimpleTestSuite";
    }
    virtual void runtests(unsigned int const) {

        clientSimple clientS0;
        paramList paramList0;

        value result0;

        // These will fail because there's no such server
        EXPECT_ERROR(clientS0.call("http://mf.comm", "biteme", &result0););

        EXPECT_ERROR(
            clientS0.call("http://mf.comm", "biteme", "s", &result0, "hard");
            );

        EXPECT_ERROR(
            clientS0.call("http://mf.comm", "biteme", paramList0, &result0);
            );
    }
};
        


class clientCurlTestSuite : public testSuite {
/*----------------------------------------------------------------------------
  The object of this class tests the combination of a client with
  Curl transport.  We assume Curl transports themselves have already
  been tested and clients with direct transports have already been tested.

  We don't have an HTTP server, so we test only superficially.

  In the future, we could either start a server or use some server that's
  normally avaailble on the Internet.
-----------------------------------------------------------------------------*/
public:
    virtual string suiteName() {
        return "clientCurlTestSuite";
    }
    virtual void runtests(unsigned int) {
#if MUST_BUILD_CURL_CLIENT
        clientXmlTransport_curl transportc0;
        client_xml client0(&transportc0);
        carriageParm_http0 carriageParmHttp("http://suckthis.com");
        carriageParm_curl0 carriageParmCurl("http://suckthis.com");
        connection connection0(&client0, &carriageParmHttp);
        paramList paramList0;
        
        rpcOutcome outcome0;

        // This fails because server doesn't exist
        EXPECT_ERROR(
            client0.call(&carriageParmHttp, "blowme", paramList0, &outcome0);
                );

        // This fails because server doesn't exist
        EXPECT_ERROR(
            client0.call(&carriageParmCurl, "blowme", paramList0, &outcome0);
            );

        rpcPtr rpc0P("blowme", paramList0);

        // This fails because server doesn't exist
        EXPECT_ERROR(rpc0P->call(&client0, &carriageParmCurl););

        rpcPtr rpc1P("blowme", paramList0);
        // This fails because server doesn't exist
        EXPECT_ERROR(rpc1P->call(connection0););

#else
        // This fails because there is no Curl transport in the library.
        EXPECT_ERROR(clientXmlTransport_curl transportc0;);
#endif
    }
};



class carriageParmTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "carriageParmTestSuite";
    }
    virtual void runtests(unsigned int) {
        carriageParm_http0 carriageParm1("http://suckthis.com");
        carriageParm_curl0 carriageParm2("http://suckthis.com");
        carriageParm_libwww0 carriageParm3("http://suckthis.com");
        carriageParm_wininet0 carriageParm4("http://suckthis.com");

        carriageParm_http0Ptr carriageParm_http1P(
            new carriageParm_http0("http://suckthis.com"));

        carriageParm_http1P->setBasicAuth("bryanh", "12345");

        carriageParm_curl0Ptr carriageParm_curl1P(
            new carriageParm_curl0("http://suckthis.com"));

        carriageParm_curl1P->setBasicAuth("bryanh", "12345");

        carriageParm_libwww0Ptr carriageParm_libwww1P(
            new carriageParm_libwww0("http://suckthis.com"));

        carriageParm_libwww1P->setBasicAuth("bryanh", "12345");

        carriageParm_wininet0Ptr carriageParm_wininet1P(
            new carriageParm_wininet0("http://suckthis.com"));

        carriageParm_wininet1P->setBasicAuth("bryanh", "12345");
    }
};



class clientRpcTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "clientRpcTestSuite";
    }
    virtual void runtests(unsigned int) {

        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
        
        carriageParm_direct carriageParm0(&myRegistry);
        clientXmlTransport_direct transportDirect;
        client_xml client0(&transportDirect);
        paramList paramListSampleAdd;
        paramListSampleAdd.add(value_int(5));
        paramListSampleAdd.add(value_int(7));
        paramList paramListEmpty;

        {
            /* Test a successful RPC */
            rpcPtr rpcSampleAddP("sample.add", paramListSampleAdd);
            TEST(!rpcSampleAddP->isFinished());
            // This fails because RPC has not been executed
            EXPECT_ERROR(value result(rpcSampleAddP->getResult()););
            
            rpcSampleAddP->call(&client0, &carriageParm0);

            TEST(rpcSampleAddP->isFinished());
            TEST(rpcSampleAddP->isSuccessful());
            value_int const resultDirect(rpcSampleAddP->getResult());
            TEST(static_cast<int>(resultDirect) == 12);
            // This fails because the RPC succeeded
            EXPECT_ERROR(fault fault0(rpcSampleAddP->getFault()););
            // This fails because the RPC has already been executed
            EXPECT_ERROR(
                rpcSampleAddP->call(&client0, &carriageParm0););
            // This fails because the RPC has already been executed
            EXPECT_ERROR(
                rpcSampleAddP->start(&client0, &carriageParm0););
        }
        {
            /* Test a failed RPC */
            rpcPtr const rpcSampleAddP("sample.add", paramListEmpty);
            rpcSampleAddP->call(&client0, &carriageParm0);
            TEST(rpcSampleAddP->isFinished());
            TEST(!rpcSampleAddP->isSuccessful());
            fault const fault0(rpcSampleAddP->getFault());
            TEST(fault0.getCode() == fault::CODE_TYPE);
            // This fails because the RPC failed
            EXPECT_ERROR(value result(rpcSampleAddP->getResult()););
            // This fails because the RPC has already been executed
            EXPECT_ERROR(
                rpcSampleAddP->call(&client0, &carriageParm0););
            // This fails because the RPC has already been executed
            EXPECT_ERROR(
                rpcSampleAddP->start(&client0, &carriageParm0););
        }
        {
            /* Test with a connection */

            connection connection0(&client0, &carriageParm0);

            rpcPtr const rpcSampleAddP("sample.add", paramListSampleAdd);

            rpcSampleAddP->call(connection0);

            TEST(rpcSampleAddP->isFinished());
            value_int const resultDirect(rpcSampleAddP->getResult());
            TEST(static_cast<int>(resultDirect) == 12);
        }
    }
};



class clientPtrTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "clientPtrTestSuite";
    }
    virtual void runtests(unsigned int) {
        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
        carriageParm_direct carriageParmDirect(&myRegistry);
        clientXmlTransport_direct transportDirect;
        
        clientPtr clientP(new client_xml(&transportDirect));

        clientPtr client2P(clientP);

        {
            clientPtr client3P;
            client3P = client2P;
        }
        rpcOutcome outcome;

        clientP->call(&carriageParmDirect, "nosuchmethod",
                      paramList(), &outcome);
        TEST(!outcome.succeeded());
        TEST(outcome.getFault().getCode() == fault::CODE_NO_SUCH_METHOD);
    }
};



class serverAccessorTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "serverAccessorTestSuite";
    }
    virtual void runtests(unsigned int) {
        clientXmlTransportPtr const transportP(new clientXmlTransport_direct);
        clientPtr const clientP(new client_xml(transportP));
        registry myRegistry;
        carriageParmPtr const carriageParmP(
            new carriageParm_direct(&myRegistry));

        serverAccessor server1(clientP, carriageParmP);

        rpcOutcome outcome;
        server1.call("nosuchmethod", paramList(), &outcome);
        TEST(!outcome.succeeded());
        TEST(outcome.getFault().getCode() == fault::CODE_NO_SUCH_METHOD);
        TEST(outcome.getFault().getDescription().size() > 0);
    }
};



string
clientTestSuite::suiteName() {
    return "clientTestSuite";
}



void
clientTestSuite::runtests(unsigned int const indentation) {

    clientDirectTestSuite().run(indentation+1);

    clientXmlTransportTestSuite().run(indentation+1);
    
    carriageParmTestSuite().run(indentation+1);
    
    clientCurlTestSuite().run(indentation+1);

    clientRpcTestSuite().run(indentation+1);
    
    clientPtrTestSuite().run(indentation+1);

    clientSimpleTestSuite().run(indentation+1);

    serverAccessorTestSuite().run(indentation+1);
}
