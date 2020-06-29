@0x85150b117366d14b;

struct HelloRequest {
    name @0 :Text;
}

struct HelloResponse {
    name @0 :Text;
}

interface Greeter {
    sayHello @0 (request: HelloRequest) -> (response: HelloResponse);
}
