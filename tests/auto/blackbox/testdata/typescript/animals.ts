export interface Mammal {
    speak(): string;
}

export class Cat implements Mammal {
    public speak() {
        return "Meow"; // a cat says meow
    }
}

export class Dog implements Mammal {
    public speak() {
        return "Woof"; // a dog says woof
    }
}

export class Human implements Mammal {
    public speak() {
        return "Hello";
    }
}
