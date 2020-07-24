#include "greeter.capnp.h"

#include <capnp/ez-rpc.h>

#include <iostream>

int main(int argc, char *argv[])
{
        const char address[] = "localhost:5050";
    capnp::EzRpcClient client(address);
    Greeter::Client greeter = client.getMain<Greeter>();

    auto& waitScope = client.getWaitScope();

    for (int i = 0; i < 2; ++i) {
        auto request = greeter.sayHelloRequest();
        request.initRequest().setName("hello workd");
        auto promise = request.send();

        auto response = promise.wait(waitScope);
        std::cout << response.getResponse().getName().cStr() << std::endl;
    }

    return 0;
}
