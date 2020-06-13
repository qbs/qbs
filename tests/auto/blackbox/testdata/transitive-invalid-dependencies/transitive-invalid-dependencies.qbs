Product {
    property bool modulePresent: {
        console.info("b.present = " + b.present);
        console.info("c.present = " + c.present);
        console.info("d.present = " + d.present);
    }

    Depends { name: "b"; required: false }
    Depends { name: "c"; required: false }
    Depends { name: "d"; required: false }
}
