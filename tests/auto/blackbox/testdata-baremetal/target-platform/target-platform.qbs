Product {
    Depends { name: "cpp" }
    condition: {
        if (qbs.toolchainType === "keil"
            || qbs.toolchainType === "iar"
            || qbs.toolchainType === "sdcc"
            || qbs.toolchainType === "cosmic") {
                var hasNoPlatform = (qbs.targetPlatform === "none");
                var hasNoOS = (qbs.targetOS.length === 1 && qbs.targetOS[0] === "none");
                console.info("has no platform: "  + hasNoPlatform);
                console.info("has no os: "  + hasNoOS);
        } else {
            console.info("unsupported toolset: %%"
                + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
            return false;
        }
        return true;
    }
}
