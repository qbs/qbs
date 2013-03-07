import qbs 1.0
import qbs.fileinfo as FileInfo

Project {
    property string name: 'configure'
    moduleSearchPaths: 'modules'
    Product {
        type: 'application'
        name: project.name
        files: 'main.cpp'
        Depends { name: 'cpp' }
        Depends { name: 'definition' }
    }
}
