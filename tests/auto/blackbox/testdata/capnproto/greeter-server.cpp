#include "greeter.capnp.h"

#include <capnp/ez-rpc.h>
#include <capnp/message.h>

#include <iostream>

class GreeterImpl final: public Greeter::Server
{
public:
    ::kj::Promise<void> sayHello(SayHelloContext context) override
    {
        auto response = context.getResults().initResponse();
        response.setName(context.getParams().getRequest().getName());
        return kj::READY_NOW;
    };
};

int main(int argc, char *argv[])
{
        const char address[] = "localhost:5050";
    capnp::EzRpcServer server(kj::heap<GreeterImpl>(), address);

    auto& waitScope = server.getWaitScope();
    // Run forever, accepting connections and handling requests.
    kj::NEVER_DONE.wait(waitScope);
}
