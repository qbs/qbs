Module {
    includePaths: foo + abc.def
    foo: bar

    car: {
        var x = 12
        x = 13
        var aaa = function aa(x1, x2) {
            function bbb() { bbb() }
            x = x1 + x2 + x2.def
            aa(x1, x2)
            bbb();
        }
        return aaa(x, includePaths)
    }
}
