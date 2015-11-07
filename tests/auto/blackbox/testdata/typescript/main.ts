import Animals = require("./animals");
import Foo = require("./foo");
import Extra = require("./woosh/extra");

function main() {
    var mammals: Animals.Mammal[] = [];
    mammals.push(new Animals.Human());
    mammals.push(new Animals.Dog());
    mammals.push(new Animals.Cat());

    // Make everyone speak
    for (var i = 0; i < mammals.length; ++i) {
        console.log(mammals[i].speak());
    }

    (new Extra.Boom());

    var greeting: string = (new Foo.Greeter()).getGreeting();
    console.log(greeting);
}

main();
