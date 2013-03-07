import qbs 1.0

Application {
    name: 'HelloWorld'

    Depends { name: 'cpp' }

    cpp.defines: ['SOMETHING']

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

