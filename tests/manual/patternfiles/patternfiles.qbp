import qbs.base 1.0

Application {
    type: 'application'
    name: 'HelloWorld'

    Depends { name: 'cpp' }

    cpp.defines: ['SOMETHING']

    //files: '../patternfiles/././.*\\..\\patternfiles/*s*r*c*\\foo.*'

    Group {
        files: 'src/foo.*'
        excludeFiles: 'src/foo.2.cpp'
        recursive: true
    }

    Group {
        cpp.defines: outer.concat(['HAVE_MAIN_CPP', cpp.debugInformation ? '_DEBUG' : '_RELEASE'])
        prefix: "src/"
        files: 'main.*'
    }
}

