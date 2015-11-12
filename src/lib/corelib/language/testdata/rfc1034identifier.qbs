import qbs
import qbs.Utilities

CppApplication {
    name: Utilities.rfc1034Identifier("this!has@special#characters$uh-oh,Undersc0r3s_Are.Bad")
    bundle.infoPlist: { return {"CFBundleIdentifier": "$(PRODUCT_NAME:rfc1034identifier)"}; }
}
