import qbs.base 1.0

Application {
    type: 'application'
    name: 'HelloWorld'

    Depends { name: 'cpp' }

    cpp.defines: base.concat(['SOMETHING'])

    files: [
        "src/foo.h",
        "src/foo.cpp"
    ]

    Group {
        cpp.defines: outer.concat(['HAVE_MAIN_CPP', cpp.debugInformation ? '_DEBUG' : '_RELEASE'])
        prefix: "src/"
        files: [
            'main.cpp'
        ]
    }
}

