Product {
    Depends { name: 'cpp' }

    Properties {
        condition: true
        type: 'application'
        consoleApplication: true
        name: 'HelloWorld'
    }

    Properties {
        condition: name == 'HelloWorld'
        cpp.defines: ['DEFINE_IN_PROPERTIES']
    }

    Properties {
        condition: qbs.targetOS.contains("weird")
        cpp.staticLibraries: "abc"
    }

    Group {
       cpp.defines: outer.concat(['HAVE_MAIN_CPP', cpp.debugInformation ? '_DEBUG' : '_RELEASE'])
       files: ['main.cpp']
    }
}


