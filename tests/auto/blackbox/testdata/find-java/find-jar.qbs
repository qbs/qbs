import qbs

Product {
    Depends { name: "java"; required: false }
    Probe {
        configure: {
            if (java.present)
                print(java.jarFilePath);
        }
    }
}
