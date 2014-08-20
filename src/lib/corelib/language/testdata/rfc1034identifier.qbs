import qbs

CppApplication {
    name: qbs.rfc1034Identifier("this!has@special#characters$uh-oh,Undersc0r3s_Are.Bad")
    cpp.infoPlist: { return {"CFBundleIdentifier": "$(PRODUCT_NAME:rfc1034identifier)"}; }
}
