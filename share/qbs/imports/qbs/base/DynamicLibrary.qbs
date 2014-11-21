import qbs

Library {
    type: ["dynamiclibrary"].concat(isForAndroid ? ["android.nativelibrary"] : [])
}
