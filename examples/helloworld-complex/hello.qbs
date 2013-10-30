import qbs 1.0

Project {
    property bool hasSpecialFeature: true
    Application {
        name: 'HelloWorld-Complex'

        Depends { name: 'cpp' }
        cpp.defines: ['SOMETHING']


        files: [
            "src/foo.h",
            "src/foo.cpp"
        ]

        Group {
            condition: project.hasSpecialFeature
            prefix: "src/"
            files: ["specialfeature.cpp", "specialfeature.h"]
        }

        Group {
            cpp.defines: {
                var defines = outer.concat([
                    'HAVE_MAIN_CPP',
                    cpp.debugInformation ? '_DEBUG' : '_RELEASE'
                    ]);
                if (project.hasSpecialFeature)
                    defines.push("HAS_SPECIAL_FEATURE");
                return defines;
            }
            prefix: "src/"
            files: [
                'main.cpp'
            ]
        }
    }
}
