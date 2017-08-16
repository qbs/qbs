import qbs 1.0
import qbs.FileInfo

Project {
    property string name: 'includeLookup'
    qbsSearchPaths: '.'
    Product {
        type: 'application'
        consoleApplication: true
        name: project.name
        files: 'main.cpp'
        Depends { name: 'cpp' }
        Depends { name: 'definition' }
    }
}
