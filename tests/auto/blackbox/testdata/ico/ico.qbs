Project {
    Product {
        Depends { name: "ico" }
        type: ["ico"]
        name: "icon"
        files: [
            "icon_16x16.png",
            "icon_32x32.png"
        ]
    }

    Product {
        Depends { name: "ico" }
        type: ["ico"]
        name: "icon-alpha"
        files: [
            "icon_16x16.png",
            "icon_32x32.png"
        ]
        ico.alphaThreshold: 50
    }

    Product {
        Depends { name: "ico" }
        type: ["ico"]
        name: "icon-big"
        files: [
            "dmg.iconset/icon_16x16.png",
            "dmg.iconset/icon_32x32.png",
            "dmg.iconset/icon_128x128.png",
        ]
        Group {
            name: "raw"
            prefix: "dmg.iconset/"
            files: [
                "icon_256x256.png",
                "icon_512x512.png",
            ]
            ico.raw: true
        }
    }

    Product {
        Depends { name: "ico" }
        type: ["cur"]
        name: "cursor"
        files: [
            "icon_16x16.png",
            "icon_32x32.png"
        ]
    }

    Product {
        Depends { name: "ico" }
        type: ["cur"]
        name: "cursor-hotspot"
        Group {
            files: ["icon_16x16.png"]
            ico.cursorHotspotX: 8
            ico.cursorHotspotY: 9
        }
        Group {
            files: ["icon_32x32.png"]
            ico.cursorHotspotX: 16
            ico.cursorHotspotY: 17
        }
    }

    Product {
        Depends { name: "ico" }
        type: ["cur"]
        name: "cursor-hotspot-single"
        Group {
            files: ["icon_16x16.png"]
            ico.cursorHotspotX: 8
            ico.cursorHotspotY: 9
        }
    }

    Product {
        Depends { name: "ico" }
        type: ["ico", "cur"]
        name: "iconset"
        files: ["dmg.iconset"]
    }
}
