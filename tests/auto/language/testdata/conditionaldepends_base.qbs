Application {
    name: 'conditionaldepends_base'
    property bool someProp: false
    Depends {
        condition: someProp
        name: 'dummy'
    }
}
